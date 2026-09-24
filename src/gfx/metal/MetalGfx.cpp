// Metal backend (metal-cpp). This TU owns the metal-cpp private implementation.
#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <unistd.h>

#include "core/Log.h"
#include "core/Time.h"
#include "gfx/metal/MetalGfx.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace lb::gfx {
namespace {

// Converts mach_absolute_time ticks to ns.
struct MachTimebase {
    u64 numer = 1, denom = 1;
    MachTimebase() {
        mach_timebase_info_data_t tb{};
        mach_timebase_info(&tb);
        numer = tb.numer;
        denom = tb.denom;
    }
    u64 toNs(u64 ticks) const { return static_cast<u64>((static_cast<unsigned __int128>(ticks) * numer) / denom); }
};

class MetalGfx final : public Gfx {
public:
    ~MetalGfx() override { shutdown(); }

    bool init(const GfxDesc& desc) override {
        m_window = desc.window;
        if (desc.validation && std::getenv("MTL_DEBUG_LAYER") == nullptr) {
            // Metal API validation is switched on by environment variables that the
            // framework reads at load time, so setting them now is too late: re-exec
            // this process with them set. A caller-provided MTL_DEBUG_LAYER wins.
            setenv("MTL_DEBUG_LAYER", "1", 1);
            setenv("MTL_DEBUG_LAYER_ERROR_MODE", "assert", 0);
            char path[4096];
            u32 size = sizeof(path);
            if (_NSGetExecutablePath(path, &size) == 0) {
                LB_LOG_INFO("re-launching with Metal API validation enabled");
                execv(path, *_NSGetArgv());
            }
            LB_LOG_ERROR("could not re-launch for Metal API validation; set MTL_DEBUG_LAYER=1");
            return false;
        }
        m_shaderDir = desc.shaderDir != nullptr ? desc.shaderDir : "";

        m_device = MTL::CreateSystemDefaultDevice();
        if (m_device == nullptr) {
            LB_LOG_ERROR("MTLCreateSystemDefaultDevice returned null");
            return false;
        }
        m_queue = m_device->newCommandQueue();
        if (m_queue == nullptr) {
            LB_LOG_ERROR("newCommandQueue failed");
            return false;
        }

        if (m_window != nullptr) {
            m_view = SDL_Metal_CreateView(desc.window);
            if (m_view == nullptr) {
                LB_LOG_ERROR("SDL_Metal_CreateView failed: %s", SDL_GetError());
                return false;
            }
            m_layer = static_cast<CA::MetalLayer*>(SDL_Metal_GetLayer(m_view));
            if (m_layer == nullptr) {
                LB_LOG_ERROR("SDL_Metal_GetLayer returned null");
                return false;
            }
            m_layer->setDevice(m_device);
            m_layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
            m_layer->setFramebufferOnly(true);
            setDisplaySync(desc.vsync);

            int w = 0, h = 0;
            SDL_GetWindowSizeInPixels(desc.window, &w, &h);
            resize(static_cast<u32>(w > 0 ? w : 1), static_cast<u32>(h > 0 ? h : 1));
        }

        for (Segment& s : m_segments) s.fence = m_device->newFence();

        m_caps.backend = Backend::Metal;
        std::strncpy(m_caps.deviceName, m_device->name()->utf8String(), sizeof(m_caps.deviceName) - 1);
        std::strncpy(m_caps.apiVersion, "Metal (metal-cpp macOS15_iOS18)", sizeof(m_caps.apiVersion) - 1);
        {
            NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
            std::snprintf(m_caps.driverInfo, sizeof(m_caps.driverInfo), "%s",
                          NS::ProcessInfo::processInfo()->operatingSystemVersionString()->utf8String());
            pool->release();
        }
        m_caps.unifiedMemory = m_device->hasUnifiedMemory();
        m_caps.rebar = false;
        m_caps.hostCachedMemory = false;
        m_caps.timelineSync = true; // MTLSharedEvent
        m_caps.gpuTimestamps = m_device->supportsCounterSampling(MTL::CounterSamplingPointAtStageBoundary) &&
                               findTimestampCounterSet() != nullptr;
        m_caps.clockCalibration = true;
        std::strncpy(m_caps.hostTimeDomain, "mach_absolute_time", sizeof(m_caps.hostTimeDomain) - 1);
        m_caps.gpuTimestampPeriodNs = 1.0;
        setClassInfo(MemoryClass::DeviceLocal, "MTLStorageModePrivate");
        setClassInfo(MemoryClass::Shared, "MTLStorageModeShared");

        calibrateClocks();

        LB_LOG_INFO("Metal device: %s (unified memory: %s, timestamps: %s, %s)", m_caps.deviceName,
                    m_caps.unifiedMemory ? "yes" : "no", m_caps.gpuTimestamps ? "stage-boundary" : "none",
                    m_window != nullptr ? (desc.vsync ? "vsync on" : "vsync off") : "headless");
        return true;
    }

    void shutdown() override {
        if (m_queue != nullptr) waitIdle();
        for (Segment& s : m_segments) {
            if (s.cb != nullptr) { s.cb->release(); s.cb = nullptr; }
            if (s.fence != nullptr) { s.fence->release(); s.fence = nullptr; }
        }
        for (TsPool& p : m_tsPools) {
            if (p.buffer != nullptr) p.buffer->release();
        }
        m_tsPools.clear();
        for (MTL::SharedEvent* e : m_timelines) {
            if (e != nullptr) e->release();
        }
        m_timelines.clear();
        for (Pipeline& p : m_pipelines) {
            if (p.state != nullptr) p.state->release();
        }
        m_pipelines.clear();
        for (Library& l : m_libraries) {
            if (l.lib != nullptr) l.lib->release();
        }
        m_libraries.clear();
        for (Buffer& b : m_buffers) {
            if (b.buffer != nullptr) b.buffer->release();
        }
        m_buffers.clear();
        m_bindingSets.clear();
        if (m_queue != nullptr) { m_queue->release(); m_queue = nullptr; }
        if (m_view != nullptr) { SDL_Metal_DestroyView(m_view); m_view = nullptr; m_layer = nullptr; }
        if (m_device != nullptr) { m_device->release(); m_device = nullptr; }
    }

    const GfxCaps& caps() const override { return m_caps; }

    // ------------------------------------------------------------------ presentation

    void resize(u32 pixelWidth, u32 pixelHeight) override {
        if (m_layer == nullptr) return;
        m_layer->setDrawableSize(CGSizeMake(static_cast<f64>(pixelWidth), static_cast<f64>(pixelHeight)));
        m_caps.backbufferWidth = pixelWidth;
        m_caps.backbufferHeight = pixelHeight;
    }

    bool beginFrame() override {
        if (m_layer == nullptr) return false;
        m_pool = NS::AutoreleasePool::alloc()->init();
        m_drawable = m_layer->nextDrawable();
        if (m_drawable == nullptr) {
            m_pool->release();
            m_pool = nullptr;
            return false;
        }
        m_cmd = m_queue->commandBuffer();
        return true;
    }

    void clearBackbuffer(ClearColor c) override {
        MTL::RenderPassDescriptor* rp = MTL::RenderPassDescriptor::alloc()->init();
        MTL::RenderPassColorAttachmentDescriptor* att = rp->colorAttachments()->object(0);
        att->setTexture(m_drawable->texture());
        att->setLoadAction(MTL::LoadActionClear);
        att->setStoreAction(MTL::StoreActionStore);
        att->setClearColor(MTL::ClearColor(static_cast<f64>(c.r), static_cast<f64>(c.g),
                                           static_cast<f64>(c.b), static_cast<f64>(c.a)));
        MTL::RenderCommandEncoder* enc = m_cmd->renderCommandEncoder(rp);
        enc->setLabel(NS::String::string("clear", NS::UTF8StringEncoding));
        enc->endEncoding();
        rp->release();
    }

    void endFrame() override {
        m_cmd->presentDrawable(m_drawable);
        std::atomic<u64>* errors = &m_errorCount;
        m_cmd->addCompletedHandler([errors](MTL::CommandBuffer* cb) {
            if (cb->status() == MTL::CommandBufferStatusError) {
                NS::Error* err = cb->error();
                LB_LOG_ERROR("Metal command buffer failed: %s",
                             err != nullptr ? err->localizedDescription()->utf8String() : "unknown");
                errors->fetch_add(1, std::memory_order_relaxed);
            }
        });
        m_cmd->commit();
        m_cmd = nullptr;
        m_drawable = nullptr;
        m_pool->release();
        m_pool = nullptr;
    }

    // ------------------------------------------------------------------ resources

    BufferHandle createBuffer(const BufferDesc& desc) override {
        MTL::ResourceOptions opts = 0;
        switch (desc.memory) {
        case MemoryClass::DeviceLocal: opts = MTL::ResourceStorageModePrivate; break;
        case MemoryClass::Shared: opts = MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeDefaultCache; break;
        case MemoryClass::HostVisibleCached:
        case MemoryClass::HostVisibleCoherent:
        case MemoryClass::DeviceLocalHostVisible:
            LB_LOG_ERROR("buffer '%s': memory class %s is not available on Metal", desc.name,
                         memoryClassName(desc.memory));
            return {};
        }
        if (desc.research) opts |= MTL::ResourceHazardTrackingModeUntracked;
        MTL::Buffer* buf = m_device->newBuffer(static_cast<NS::UInteger>(desc.size), opts);
        if (buf == nullptr) {
            LB_LOG_ERROR("newBuffer(%llu) failed for '%s'", static_cast<unsigned long long>(desc.size), desc.name);
            return {};
        }
        {
            NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
            buf->setLabel(NS::String::string(desc.name, NS::UTF8StringEncoding));
            pool->release();
        }
        Buffer b;
        b.buffer = buf;
        b.memory = desc.memory;
        b.research = desc.research;
        b.size = desc.size;
        return {addSlot(m_buffers, b)};
    }

    void destroyBuffer(BufferHandle h) override {
        Buffer* b = buffer(h);
        if (b == nullptr) return;
        b->buffer->release();
        *b = Buffer{};
    }

    void* mappedPtr(BufferHandle h) override {
        Buffer* b = buffer(h);
        if (b == nullptr || b->memory == MemoryClass::DeviceLocal) return nullptr;
        return b->buffer->contents();
    }

    void flushHostWrites(BufferHandle, u64, u64) override {}      // Shared storage is coherent
    void invalidateHostReads(BufferHandle, u64, u64) override {}  // Shared storage is coherent

    PipelineHandle createComputePipeline(const ComputePipelineDesc& desc) override {
        if (desc.bufferCount > kMaxBindings || desc.pushConstantBytes > kMaxPushConstantBytes) {
            LB_LOG_ERROR("pipeline %s:%s exceeds binding/push-constant limits", desc.shader, desc.entry);
            return {};
        }
        MTL::Library* lib = library(desc.shader);
        if (lib == nullptr) return {};
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        MTL::Function* fn = lib->newFunction(NS::String::string(desc.entry, NS::UTF8StringEncoding));
        if (fn == nullptr) {
            LB_LOG_ERROR("entry point '%s' not found in %s.metallib", desc.entry, desc.shader);
            pool->release();
            return {};
        }
        NS::Error* err = nullptr;
        MTL::ComputePipelineState* state = m_device->newComputePipelineState(fn, &err);
        fn->release();
        if (state == nullptr) {
            LB_LOG_ERROR("compute pipeline %s:%s failed: %s", desc.shader, desc.entry,
                         err != nullptr ? err->localizedDescription()->utf8String() : "?");
            pool->release();
            return {};
        }
        pool->release();
        if (state->maxTotalThreadsPerThreadgroup() < desc.threadsPerGroup) {
            LB_LOG_ERROR("pipeline %s:%s supports only %lu threads per group", desc.shader, desc.entry,
                         static_cast<unsigned long>(state->maxTotalThreadsPerThreadgroup()));
            state->release();
            return {};
        }
        Pipeline p;
        p.state = state;
        p.bufferCount = desc.bufferCount;
        p.pushBytes = desc.pushConstantBytes;
        p.threadsPerGroup = desc.threadsPerGroup;
        return {addSlot(m_pipelines, p)};
    }

    BindingSetHandle createBindingSet(PipelineHandle ph, const BufferHandle* buffers, u32 count) override {
        const Pipeline* p = pipeline(ph);
        if (p == nullptr || count != p->bufferCount) {
            LB_LOG_ERROR("createBindingSet: pipeline expects %u buffers, got %u", p != nullptr ? p->bufferCount : 0u,
                         count);
            return {};
        }
        BindingSet s;
        s.count = count;
        for (u32 i = 0; i < count; ++i) {
            const Buffer* b = buffer(buffers[i]);
            if (b == nullptr) {
                LB_LOG_ERROR("createBindingSet: invalid buffer at binding %u", i);
                return {};
            }
            s.buffers[i] = b->buffer;
        }
        return {addSlot(m_bindingSets, s)};
    }

    void destroyBindingSet(BindingSetHandle h) override {
        BindingSet* s = slot(m_bindingSets, h.id);
        if (s != nullptr) *s = BindingSet{};
    }

    TimelineHandle createTimeline(u64 initialValue) override {
        MTL::SharedEvent* ev = m_device->newSharedEvent();
        if (ev == nullptr) {
            LB_LOG_ERROR("newSharedEvent failed");
            return {};
        }
        ev->setSignaledValue(initialValue);
        return {addSlot(m_timelines, ev)};
    }

    TimestampPoolHandle createTimestampPool(u32 count) override {
        if (!m_caps.gpuTimestamps) return {};
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        MTL::CounterSampleBufferDescriptor* d = MTL::CounterSampleBufferDescriptor::alloc()->init();
        d->setCounterSet(findTimestampCounterSet());
        d->setStorageMode(MTL::StorageModeShared);
        d->setSampleCount(count);
        NS::Error* err = nullptr;
        MTL::CounterSampleBuffer* buf = m_device->newCounterSampleBuffer(d, &err);
        d->release();
        if (buf == nullptr) {
            LB_LOG_ERROR("newCounterSampleBuffer(%u) failed: %s", count,
                         err != nullptr ? err->localizedDescription()->utf8String() : "?");
            pool->release();
            return {};
        }
        pool->release();
        TsPool p;
        p.buffer = buf;
        p.count = count;
        p.written.assign(count, 0);
        return {addSlot(m_tsPools, p)};
    }

    // ------------------------------------------------------------------ recording

    SegmentHandle beginSegment(const SegmentSync& sync) override {
        const u32 idx = m_nextSegment;
        m_nextSegment = (m_nextSegment + 1) % kSegmentSlots;
        Segment& s = m_segments[idx];
        retire(s);

        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        s.cb = m_queue->commandBufferWithUnretainedReferences();
        s.cb->retain();
        pool->release();
        s.sync = sync;
        s.pendingBarrier = false;
        s.recording = true;
        if (sync.waitValue != 0) {
            MTL::SharedEvent* ev = timeline(sync.waitTimeline);
            if (ev != nullptr) s.cb->encodeWait(ev, sync.waitValue);
        }
        return {idx + 1};
    }

    void cmdDispatch(SegmentHandle sh, PipelineHandle ph, BindingSetHandle bh, u32 groupsX, const void* push,
                     u32 pushBytes, const PassTimestamps& ts) override {
        Segment* s = segment(sh);
        const Pipeline* p = pipeline(ph);
        const BindingSet* b = bindingSet(bh);
        if (s == nullptr || p == nullptr || b == nullptr || pushBytes != p->pushBytes) {
            LB_LOG_ERROR("cmdDispatch: invalid handle or push-constant size");
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        MTL::ComputePassDescriptor* pd = MTL::ComputePassDescriptor::computePassDescriptor();
        pd->setDispatchType(MTL::DispatchTypeSerial);
        attachSamples(pd->sampleBufferAttachments()->object(0), ts);
        MTL::ComputeCommandEncoder* enc = s->cb->computeCommandEncoder(pd);
        if (s->pendingBarrier) enc->waitForFence(s->fence);
        enc->setComputePipelineState(p->state);
        for (u32 i = 0; i < b->count; ++i) enc->setBuffer(b->buffers[i], 0, i);
        if (p->pushBytes > 0) {
            // [[vk::push_constant]] maps to [[buffer(bufferCount)]] in Slang's Metal output.
            enc->setBytes(push, pushBytes, p->bufferCount);
        }
        enc->dispatchThreadgroups(MTL::Size(groupsX, 1, 1), MTL::Size(p->threadsPerGroup, 1, 1));
        enc->updateFence(s->fence);
        enc->endEncoding();
        pool->release();
        s->pendingBarrier = false;
    }

    void cmdCopyBuffer(SegmentHandle sh, BufferHandle src, u64 srcOffset, BufferHandle dst, u64 dstOffset, u64 size,
                       const PassTimestamps& ts) override {
        Segment* s = segment(sh);
        const Buffer* a = buffer(src);
        const Buffer* b = buffer(dst);
        if (s == nullptr || a == nullptr || b == nullptr) {
            LB_LOG_ERROR("cmdCopyBuffer: invalid handle");
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (a->research || b->research) ++m_researchCopies;
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        MTL::BlitPassDescriptor* pd = MTL::BlitPassDescriptor::blitPassDescriptor();
        attachSamples(pd->sampleBufferAttachments()->object(0), ts);
        MTL::BlitCommandEncoder* enc = s->cb->blitCommandEncoder(pd);
        if (s->pendingBarrier) enc->waitForFence(s->fence);
        enc->copyFromBuffer(a->buffer, srcOffset, b->buffer, dstOffset, size);
        enc->updateFence(s->fence);
        enc->endEncoding();
        pool->release();
        s->pendingBarrier = false;
    }

    void cmdBarrier(SegmentHandle sh, u32 srcAccess, u32 dstAccess) override {
        Segment* s = segment(sh);
        if (s == nullptr) return;
        // Host accesses are ordered by the shared event (Shared storage is coherent once the
        // event is observed, docs/05 §4.1); GPU→GPU hazards between encoders use the fence.
        const u32 gpu = kAccessComputeRead | kAccessComputeWrite | kAccessCopyRead | kAccessCopyWrite;
        if ((srcAccess & gpu) != 0 && (dstAccess & gpu) != 0) s->pendingBarrier = true;
    }

    void endSegment(SegmentHandle sh) override {
        Segment* s = segment(sh);
        if (s == nullptr) return;
        if (s->sync.signalValue != 0) {
            MTL::SharedEvent* ev = timeline(s->sync.signalTimeline);
            if (ev != nullptr) s->cb->encodeSignalEvent(ev, s->sync.signalValue);
        }
        s->recording = false;
    }

    bool submit(const SegmentHandle* segments, u32 count) override {
        for (u32 i = 0; i < count; ++i) {
            Segment* s = segment(segments[i]);
            if (s == nullptr || s->recording || s->cb == nullptr || s->submitted) {
                LB_LOG_ERROR("submit: segment %u not ready", i);
                m_errorCount.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            s->cb->commit();
            s->submitted = true;
        }
        return true;
    }

    bool waitIdle() override {
        bool ok = true;
        for (Segment& s : m_segments) ok = retire(s) && ok;
        return ok;
    }

    // ------------------------------------------------------------------ sync

    void cpuSignal(TimelineHandle t, u64 value) override {
        MTL::SharedEvent* ev = timeline(t);
        if (ev == nullptr) return;
        std::atomic_thread_fence(std::memory_order_release); // CPU writes before the signal (docs/05 §4.1)
        ev->setSignaledValue(value);
    }

    bool cpuWait(TimelineHandle t, u64 value, CpuWaitMode mode, u64 timeoutNs) override {
        MTL::SharedEvent* ev = timeline(t);
        if (ev == nullptr) return false;
        bool ok = true;
        if (mode == CpuWaitMode::Block) {
            ok = ev->waitUntilSignaledValue(value, timeoutNs / 1000000u + 1u);
        } else {
            const u64 deadline = nowNs() + timeoutNs;
            while (ev->signaledValue() < value) {
                if (nowNs() > deadline) { ok = false; break; }
            }
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        return ok;
    }

    u64 timelineValue(TimelineHandle t) override {
        MTL::SharedEvent* ev = timeline(t);
        return ev != nullptr ? ev->signaledValue() : 0;
    }

    // ------------------------------------------------------------------ timestamps

    void resetTimestamps(TimestampPoolHandle h, u32 first, u32 count) override {
        TsPool* p = tsPool(h);
        if (p == nullptr) return;
        for (u32 i = first; i < first + count && i < p->count; ++i) p->written[i] = 0;
    }

    bool readTimestamps(TimestampPoolHandle h, u32 first, u32 count, u64* out) override {
        for (u32 i = 0; i < count; ++i) out[i] = 0;
        TsPool* p = tsPool(h);
        if (p == nullptr || first + count > p->count) return false;
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        NS::Data* data = p->buffer->resolveCounterRange(NS::Range::Make(first, count));
        const bool ok = data != nullptr && data->length() >= count * sizeof(MTL::CounterResultTimestamp);
        if (ok) {
            const auto* ts = static_cast<const MTL::CounterResultTimestamp*>(data->mutableBytes());
            for (u32 i = 0; i < count; ++i) {
                const u64 v = ts[i].timestamp;
                if (p->written[first + i] == 0 || v == 0 || v == MTL::CounterErrorValue) continue;
                out[i] = gpuToSteady(v);
            }
        }
        pool->release();
        return ok;
    }

    ClockCalibration calibrateClocks() override {
        // GPU timestamps and sampleTimestamps' CPU value are both mach_absolute_time based
        // (verified on M5: ns, ratio 1.0). steady_clock is a different domain (it keeps
        // counting during sleep), so the mach→steady offset is measured as well.
        MTL::Timestamp cpu = 0, gpu = 0;
        m_device->sampleTimestamps(&cpu, &gpu);
        const u64 machNow = mach_absolute_time();
        // The CPU value is ns on Apple silicon; accept mach ticks too.
        const u64 cpuNs = absDiff(cpu, machNow) < absDiff(cpu, m_timebase.toNs(machNow)) ? m_timebase.toNs(cpu) : cpu;

        if (!m_haveBaseline) {
            m_baseCpuNs = cpuNs;
            m_baseGpu = gpu;
            m_haveBaseline = true;
        } else if (cpuNs > m_baseCpuNs + 50'000'000u && gpu > m_baseGpu) {
            m_gpuToCpuSlope = static_cast<f64>(cpuNs - m_baseCpuNs) / static_cast<f64>(gpu - m_baseGpu);
        }
        m_refGpu = gpu;
        m_refCpuNs = cpuNs;

        // mach (ns) → steady (ns): tightest of several bracketed reads.
        u64 bestWindow = ~0ull;
        i64 offset = 0;
        for (int i = 0; i < 16; ++i) {
            const u64 a = nowNs();
            const u64 m = m_timebase.toNs(mach_absolute_time());
            const u64 b = nowNs();
            if (b - a < bestWindow) {
                bestWindow = b - a;
                offset = static_cast<i64>(a + (b - a) / 2) - static_cast<i64>(m);
            }
        }
        m_machToSteady = offset;

        ClockCalibration c;
        c.valid = true;
        c.maxDeviationNs = static_cast<f64>(bestWindow) / 2.0;
        return c;
    }

    // ------------------------------------------------------------------ audit

    u64 researchCopyCount() const override { return m_researchCopies; }
    u64 errorCount() const override { return m_errorCount.load(std::memory_order_relaxed); }

private:
    static constexpr u32 kSegmentSlots = 16;

    struct Buffer {
        MTL::Buffer* buffer = nullptr;
        MemoryClass memory = MemoryClass::DeviceLocal;
        bool research = false;
        u64 size = 0;
    };
    struct Library {
        std::string name;
        MTL::Library* lib = nullptr;
    };
    struct Pipeline {
        MTL::ComputePipelineState* state = nullptr;
        u32 bufferCount = 0;
        u32 pushBytes = 0;
        u32 threadsPerGroup = 64;
    };
    struct BindingSet {
        MTL::Buffer* buffers[kMaxBindings] = {};
        u32 count = 0;
    };
    struct TsPool {
        MTL::CounterSampleBuffer* buffer = nullptr;
        u32 count = 0;
        std::vector<u8> written;
    };
    struct Segment {
        MTL::CommandBuffer* cb = nullptr;
        MTL::Fence* fence = nullptr;
        SegmentSync sync{};
        bool pendingBarrier = false;
        bool recording = false;
        bool submitted = false;
    };

    template <class T> static u32 addSlot(std::vector<T>& v, const T& item) {
        v.push_back(item);
        return static_cast<u32>(v.size());
    }
    template <class T> static T* slot(std::vector<T>& v, u32 id) { return (id == 0 || id > v.size()) ? nullptr : &v[id - 1]; }

    Buffer* buffer(BufferHandle h) {
        Buffer* b = slot(m_buffers, h.id);
        return (b != nullptr && b->buffer != nullptr) ? b : nullptr;
    }
    Pipeline* pipeline(PipelineHandle h) { return slot(m_pipelines, h.id); }
    BindingSet* bindingSet(BindingSetHandle h) {
        BindingSet* s = slot(m_bindingSets, h.id);
        return (s != nullptr && s->count != 0) ? s : nullptr;
    }
    MTL::SharedEvent* timeline(TimelineHandle h) {
        MTL::SharedEvent** e = slot(m_timelines, h.id);
        return e != nullptr ? *e : nullptr;
    }
    TsPool* tsPool(TimestampPoolHandle h) { return slot(m_tsPools, h.id); }
    Segment* segment(SegmentHandle h) { return (h.id == 0 || h.id > kSegmentSlots) ? nullptr : &m_segments[h.id - 1]; }

    static u64 absDiff(u64 a, u64 b) { return a > b ? a - b : b - a; }

    u64 gpuToSteady(u64 gpu) const {
        const f64 dGpu = static_cast<f64>(static_cast<i64>(gpu - m_refGpu));
        const i64 cpuNs = static_cast<i64>(m_refCpuNs) + static_cast<i64>(dGpu * m_gpuToCpuSlope);
        return static_cast<u64>(cpuNs + m_machToSteady);
    }

    // Waits for a submitted segment and releases its command buffer.
    bool retire(Segment& s) {
        if (s.cb == nullptr) return true;
        bool ok = true;
        if (s.submitted) {
            s.cb->waitUntilCompleted();
            if (s.cb->status() == MTL::CommandBufferStatusError) {
                NS::Error* err = s.cb->error();
                LB_LOG_ERROR("Metal command buffer failed: %s",
                             err != nullptr ? err->localizedDescription()->utf8String() : "unknown");
                m_errorCount.fetch_add(1, std::memory_order_relaxed);
                ok = false;
            }
        }
        s.cb->release();
        s.cb = nullptr;
        s.submitted = false;
        s.recording = false;
        return ok;
    }

    template <class Attachment> void attachSamples(Attachment* a, const PassTimestamps& ts) {
        TsPool* p = tsPool(ts.pool);
        if (p == nullptr || (ts.begin == kNoTimestamp && ts.end == kNoTimestamp)) return;
        a->setSampleBuffer(p->buffer);
        a->setStartOfEncoderSampleIndex(ts.begin == kNoTimestamp ? MTL::CounterDontSample : ts.begin);
        a->setEndOfEncoderSampleIndex(ts.end == kNoTimestamp ? MTL::CounterDontSample : ts.end);
        if (ts.begin != kNoTimestamp && ts.begin < p->count) p->written[ts.begin] = 1;
        if (ts.end != kNoTimestamp && ts.end < p->count) p->written[ts.end] = 1;
    }

    MTL::CounterSet* findTimestampCounterSet() {
        NS::Array* sets = m_device->counterSets();
        if (sets == nullptr) return nullptr;
        for (NS::UInteger i = 0; i < sets->count(); ++i) {
            auto* set = static_cast<MTL::CounterSet*>(sets->object(i));
            if (std::strcmp(set->name()->utf8String(), "timestamp") == 0) return set;
        }
        return nullptr;
    }

    MTL::Library* library(const char* shader) {
        for (const Library& l : m_libraries) {
            if (l.name == shader) return l.lib;
        }
        const std::string path = m_shaderDir + shader + ".metallib";
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        NS::Error* err = nullptr;
        NS::URL* url = NS::URL::fileURLWithPath(NS::String::string(path.c_str(), NS::UTF8StringEncoding));
        MTL::Library* lib = m_device->newLibrary(url, &err);
        if (lib == nullptr) {
            LB_LOG_ERROR("loading %s failed: %s", path.c_str(),
                         err != nullptr ? err->localizedDescription()->utf8String() : "?");
        }
        pool->release();
        if (lib != nullptr) m_libraries.push_back({shader, lib});
        return lib;
    }

    void setClassInfo(MemoryClass c, const char* info) {
        const u32 i = static_cast<u32>(c);
        m_caps.memoryClassSupported[i] = true;
        std::strncpy(m_caps.memoryClassInfo[i], info, sizeof(m_caps.memoryClassInfo[i]) - 1);
    }

    void setDisplaySync(bool enabled) {
        // metal-cpp's CA::MetalLayer does not wrap displaySyncEnabled; send the selector directly.
        using Fn = void (*)(void*, SEL, bool);
        reinterpret_cast<Fn>(objc_msgSend)(m_layer, sel_registerName("setDisplaySyncEnabled:"), enabled);
    }

    SDL_Window* m_window = nullptr;
    SDL_MetalView m_view = nullptr;
    CA::MetalLayer* m_layer = nullptr;
    MTL::Device* m_device = nullptr;
    MTL::CommandQueue* m_queue = nullptr;
    NS::AutoreleasePool* m_pool = nullptr;
    CA::MetalDrawable* m_drawable = nullptr;
    MTL::CommandBuffer* m_cmd = nullptr;
    std::string m_shaderDir;

    std::vector<Buffer> m_buffers;
    std::vector<Library> m_libraries;
    std::vector<Pipeline> m_pipelines;
    std::vector<BindingSet> m_bindingSets;
    std::vector<MTL::SharedEvent*> m_timelines;
    std::vector<TsPool> m_tsPools;
    Segment m_segments[kSegmentSlots];
    u32 m_nextSegment = 0;
    u64 m_researchCopies = 0;

    MachTimebase m_timebase;
    bool m_haveBaseline = false;
    u64 m_baseCpuNs = 0, m_baseGpu = 0;
    u64 m_refCpuNs = 0, m_refGpu = 0;
    f64 m_gpuToCpuSlope = 1.0;
    i64 m_machToSteady = 0;

    GfxCaps m_caps{};
    std::atomic<u64> m_errorCount{0};
};

} // namespace

std::unique_ptr<Gfx> createMetalGfx() { return std::make_unique<MetalGfx>(); }

} // namespace lb::gfx
