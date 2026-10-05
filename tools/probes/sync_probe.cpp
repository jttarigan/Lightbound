// Standalone M5 sync probe (not part of the Lightbound build or the research pipeline).
// Written 2026-10-05 as evidence for the M1 go/no-go decision; results and discussion in
// docs/STATUS.md (finding 7). Build and run from the repository root (macOS, Apple silicon):
//   clang++ -std=c++20 -O2 -isystem build/_deps/metalcpp-src tools/probes/sync_probe.cpp \
//       -framework Metal -framework Foundation -framework QuartzCore -o build/sync_probe
//   ./build/sync_probe results/probe_sync/run1.csv 10 300
//
// Question: is the slow, multimodal CPU->GPU hand-off (c2g) of the M1 microbenchmark a property
// of MTLSharedEvent waits on an idle GPU, or of the GPU itself? Same timing definitions as
// src/bench/Microbench.cpp (empty payload, chain submit, CPU spin wait):
//   rt  = consume-encoder start - produce-encoder end   (GPU timestamps)
//   g2c = CPU observes "produced"   - produce-encoder end
//   c2g = consume-encoder start     - CPU signal time
// Variants, interleaved in blocks so slow mode drift hits all of them equally:
//   idle     baseline: GPU waits on the shared event, nothing else runs
//   busy     + a second queue keeps a small ALU kernel (1 group x 32 threads, ~200 us) running back to back
//   half     + the same kernel at ~50 % duty (200 us on, ~200 us off)
//   heavy    + back-to-back ~200 us kernels that fill the whole GPU (64 groups x 256 threads)
//   light    + a second queue submits a tiny kernel every ~200 us
//   pingpong + a second thread does its own event round trips on a second queue
//   poll     c2g via a GPU kernel spinning on a Shared-memory flag (g2c still via the event)
//   pollrmw  same, but the GPU polls with an atomic RMW (fetch_or 0) instead of a load
//   poll2    both directions via Shared-memory flags (no events in the loop)
// Usage: sync_probe out.csv [rounds=8] [perBlock=250] [variants=idle,busy,...]
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <mach/mach_time.h>
#include <pthread.h>
#include <time.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

const char* kShaderSrc = R"(
#include <metal_stdlib>
using namespace metal;
kernel void produce(device uint* data [[buffer(0)]], device atomic_uint* flagG [[buffer(1)]],
                    constant uint& it [[buffer(2)]], uint tid [[thread_position_in_grid]]) {
    data[tid] = it * 2654435761u + tid;
    threadgroup_barrier(mem_flags::mem_device);
    if (tid == 0) atomic_store_explicit(flagG, it, memory_order_relaxed);
}
kernel void spin(device atomic_uint* flagC [[buffer(0)]], device uint* out [[buffer(1)]],
                 constant uint3& p [[buffer(2)]], uint tid [[thread_position_in_grid]]) {
    uint n = 0;
    if (p.z == 0) {
        while (atomic_load_explicit(flagC, memory_order_relaxed) != p.x) { if (++n >= p.y) break; }
    } else {
        while (atomic_fetch_or_explicit(flagC, 0u, memory_order_relaxed) != p.x) { if (++n >= p.y) break; }
    }
    out[0] = n;
}
kernel void consume(device uint* data [[buffer(0)]], device uint* out [[buffer(1)]],
                    uint tid [[thread_position_in_grid]]) {
    out[1 + tid] = data[tid] ^ 0x5a5a5a5au;
}
kernel void burn(device float* out [[buffer(0)]], constant uint& n [[buffer(1)]],
                 uint tid [[thread_position_in_grid]]) {
    float a = float(tid), b = 1.0001f;
    for (uint i = 0; i < n; ++i) a = fma(a, b, 0.5f);
    out[tid] = a;
}
)";

enum Variant { kIdle, kBusy, kHalf, kHeavy, kLight, kPingpong, kPoll, kPollRmw, kPoll2, kVariantCount };
const char* kVariantNames[kVariantCount] = {"idle", "busy", "half", "heavy", "light", "pingpong", "poll", "pollrmw", "poll2"};

enum : uint32_t { kTsProduceBegin, kTsProduceEnd, kTsConsumeBegin, kTsConsumeEnd, kTsCount };

mach_timebase_info_data_t g_tb;
uint64_t nowNs() { return mach_absolute_time() * g_tb.numer / g_tb.denom; }

struct Row {
    int variant, round;
    uint32_t iter;
    double rt, g2c, c2g;
    uint32_t spinLoops;
    bool spinTimeout, memWaitTimeout;
};

double pct(std::vector<double> v, double q) {
    v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return std::isnan(x); }), v.end());
    if (v.empty()) return NAN;
    std::sort(v.begin(), v.end());
    const double pos = q * double(v.size() - 1);
    const size_t lo = size_t(pos), hi = std::min(lo + 1, v.size() - 1);
    return v[lo] + (v[hi] - v[lo]) * (pos - double(lo));
}

struct Probe {
    MTL::Device* dev = nullptr;
    MTL::CommandQueue* q = nullptr;   // probe queue
    MTL::CommandQueue* q2 = nullptr;  // background queue
    MTL::ComputePipelineState *produce = nullptr, *spin = nullptr, *consume = nullptr, *burn = nullptr;
    MTL::SharedEvent* ev = nullptr;
    MTL::SharedEvent* ev2 = nullptr;
    MTL::Buffer *data = nullptr, *flagG = nullptr, *flagC = nullptr, *out = nullptr, *burnOut = nullptr;
    MTL::CounterSampleBuffer* ts = nullptr;
    uint64_t value = 0;
    uint32_t spinMaxLoops = 1u << 20;
    uint32_t burnLoops = 20000;
    uint32_t heavyLoops = 2000;
    int64_t gpuToCpu = 0;  // cpuNs = gpuTs + gpuToCpu

    MTL::ComputePipelineState* pipe(MTL::Library* lib, const char* name) {
        MTL::Function* f = lib->newFunction(NS::String::string(name, NS::UTF8StringEncoding));
        NS::Error* err = nullptr;
        MTL::ComputePipelineState* p = dev->newComputePipelineState(f, &err);
        f->release();
        if (p == nullptr) { std::fprintf(stderr, "pipeline %s failed\n", name); std::exit(1); }
        return p;
    }

    void init() {
        dev = MTL::CreateSystemDefaultDevice();
        q = dev->newCommandQueue();
        q2 = dev->newCommandQueue();
        NS::Error* err = nullptr;
        MTL::Library* lib = dev->newLibrary(NS::String::string(kShaderSrc, NS::UTF8StringEncoding), nullptr, &err);
        if (lib == nullptr) {
            std::fprintf(stderr, "shader compile failed: %s\n", err->localizedDescription()->utf8String());
            std::exit(1);
        }
        produce = pipe(lib, "produce");
        spin = pipe(lib, "spin");
        consume = pipe(lib, "consume");
        burn = pipe(lib, "burn");
        ev = dev->newSharedEvent();
        ev2 = dev->newSharedEvent();
        const auto shared = MTL::ResourceStorageModeShared;
        data = dev->newBuffer(64 * 4, shared);
        flagG = dev->newBuffer(16, shared);
        flagC = dev->newBuffer(16, shared);
        out = dev->newBuffer(80 * 4, shared);
        burnOut = dev->newBuffer(64 * 256 * 4, shared);
        std::memset(flagG->contents(), 0, 16);
        std::memset(flagC->contents(), 0, 16);

        NS::Array* sets = dev->counterSets();
        MTL::CounterSet* tsSet = nullptr;
        for (NS::UInteger i = 0; i < sets->count(); ++i) {
            auto* s = static_cast<MTL::CounterSet*>(sets->object(i));
            if (std::strcmp(s->name()->utf8String(), "timestamp") == 0) tsSet = s;
        }
        MTL::CounterSampleBufferDescriptor* d = MTL::CounterSampleBufferDescriptor::alloc()->init();
        d->setCounterSet(tsSet);
        d->setStorageMode(MTL::StorageModeShared);
        d->setSampleCount(kTsCount);
        ts = dev->newCounterSampleBuffer(d, &err);
        d->release();
        if (ts == nullptr) { std::fprintf(stderr, "no timestamp counters\n"); std::exit(1); }
    }

    void calibrateClock() {
        MTL::Timestamp cpu = 0, gpu = 0;
        dev->sampleTimestamps(&cpu, &gpu);
        // On Apple silicon both values are ns in the mach_absolute_time domain (M5, verified in M1).
        gpuToCpu = int64_t(cpu) - int64_t(gpu);
    }

    MTL::ComputeCommandEncoder* encoder(MTL::CommandBuffer* cb, int tsBegin, int tsEnd) {
        MTL::ComputePassDescriptor* pd = MTL::ComputePassDescriptor::computePassDescriptor();
        pd->setDispatchType(MTL::DispatchTypeSerial);
        if (tsBegin >= 0 || tsEnd >= 0) {
            auto* a = pd->sampleBufferAttachments()->object(0);
            a->setSampleBuffer(ts);
            a->setStartOfEncoderSampleIndex(tsBegin >= 0 ? NS::UInteger(tsBegin) : MTL::CounterDontSample);
            a->setEndOfEncoderSampleIndex(tsEnd >= 0 ? NS::UInteger(tsEnd) : MTL::CounterDontSample);
        }
        return cb->computeCommandEncoder(pd);
    }

    void encProduce(MTL::CommandBuffer* cb, uint32_t it, MTL::Fence* fence) {
        MTL::ComputeCommandEncoder* e = encoder(cb, kTsProduceBegin, kTsProduceEnd);
        e->setComputePipelineState(produce);
        e->setBuffer(data, 0, 0);
        e->setBuffer(flagG, 0, 1);
        e->setBytes(&it, 4, 2);
        e->dispatchThreadgroups(MTL::Size(1, 1, 1), MTL::Size(64, 1, 1));
        if (fence) e->updateFence(fence);
        e->endEncoding();
    }
    void encSpin(MTL::CommandBuffer* cb, uint32_t target, MTL::Fence* waitF, MTL::Fence* updF, uint32_t maxLoops,
                 uint32_t mode = 0) {
        MTL::ComputeCommandEncoder* e = encoder(cb, -1, -1);
        if (waitF) e->waitForFence(waitF);
        e->setComputePipelineState(spin);
        e->setBuffer(flagC, 0, 0);
        e->setBuffer(out, 0, 1);
        const uint32_t p[4] = {target, maxLoops, mode, 0};
        e->setBytes(p, 16, 2);
        e->dispatchThreadgroups(MTL::Size(1, 1, 1), MTL::Size(1, 1, 1));
        if (updF) e->updateFence(updF);
        e->endEncoding();
    }
    void encConsume(MTL::CommandBuffer* cb, MTL::Fence* waitF) {
        MTL::ComputeCommandEncoder* e = encoder(cb, kTsConsumeBegin, kTsConsumeEnd);
        if (waitF) e->waitForFence(waitF);
        e->setComputePipelineState(consume);
        e->setBuffer(data, 0, 0);
        e->setBuffer(out, 0, 1);
        e->dispatchThreadgroups(MTL::Size(1, 1, 1), MTL::Size(64, 1, 1));
        e->endEncoding();
    }

    bool spinEvent(MTL::SharedEvent* e, uint64_t v, uint64_t timeoutNs) {
        const uint64_t deadline = nowNs() + timeoutNs;
        while (e->signaledValue() < v) {
            if (nowNs() > deadline) return false;
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        return true;
    }

    // One measured round trip. Returns false on a hard failure.
    bool roundTrip(Variant var, uint32_t it, Row& r) {
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        const uint64_t vGpu = ++value, vCpu = ++value, vEnd = ++value;
        const uint32_t target = it + 1;  // never 0; flags start at 0
        auto* flagCp = static_cast<uint32_t*>(flagC->contents());
        auto* flagGp = static_cast<uint32_t*>(flagG->contents());
        uint64_t tWake = 0, tSignal = 0;
        r.spinLoops = 0;
        r.spinTimeout = r.memWaitTimeout = false;

        MTL::CommandBuffer* last = nullptr;
        const bool polling = var == kPoll || var == kPollRmw || var == kPoll2;
        if (polling) {
            // CB1 = produce (+ event signal unless poll2); CB2 = GPU spins on flagC, then consume.
            MTL::Fence* f = dev->newFence();
            MTL::CommandBuffer* cb1 = q->commandBuffer();
            encProduce(cb1, target, nullptr);
            if (var != kPoll2) cb1->encodeSignalEvent(ev, vGpu);
            MTL::CommandBuffer* cb2 = q->commandBuffer();
            encSpin(cb2, target, nullptr, f, spinMaxLoops, var == kPollRmw ? 1u : 0u);
            encConsume(cb2, f);
            cb2->encodeSignalEvent(ev, vEnd);
            cb1->commit();
            cb2->commit();
            last = cb2;
            if (var != kPoll2) {
                if (!spinEvent(ev, vGpu, 1'000'000'000ull)) { pool->release(); return false; }
            } else {
                const uint64_t deadline = nowNs() + 50'000'000ull;
                while (__atomic_load_n(flagGp, __ATOMIC_ACQUIRE) != target) {
                    if (nowNs() > deadline) { r.memWaitTimeout = true; break; }
                }
            }
            tWake = nowNs();
            tSignal = nowNs();
            __atomic_store_n(flagCp, target, __ATOMIC_RELEASE);
            f->release();
        } else {
            MTL::CommandBuffer* cb1 = q->commandBuffer();
            encProduce(cb1, target, nullptr);
            cb1->encodeSignalEvent(ev, vGpu);
            MTL::CommandBuffer* cb2 = q->commandBuffer();
            cb2->encodeWait(ev, vCpu);
            encConsume(cb2, nullptr);
            cb2->encodeSignalEvent(ev, vEnd);
            cb1->commit();
            cb2->commit();
            last = cb2;
            if (!spinEvent(ev, vGpu, 1'000'000'000ull)) { pool->release(); return false; }
            tWake = nowNs();
            tSignal = nowNs();
            std::atomic_thread_fence(std::memory_order_release);
            ev->setSignaledValue(vCpu);
        }
        if (!ev->waitUntilSignaledValue(vEnd, 2000)) { pool->release(); return false; }
        last->waitUntilCompleted();
        if (last->status() == MTL::CommandBufferStatusError) {
            std::fprintf(stderr, "command buffer error: %s\n", last->error()->localizedDescription()->utf8String());
            pool->release();
            return false;
        }
        if (polling) {
            r.spinLoops = static_cast<uint32_t*>(out->contents())[0];
            r.spinTimeout = r.spinLoops >= spinMaxLoops;
        }
        NS::Data* d = ts->resolveCounterRange(NS::Range::Make(0, kTsCount));
        const auto* t = static_cast<const MTL::CounterResultTimestamp*>(d->mutableBytes());
        auto cpuNs = [&](uint32_t i) -> double {
            const uint64_t v = t[i].timestamp;
            if (v == 0 || v == MTL::CounterErrorValue) return NAN;
            return double(int64_t(v) + gpuToCpu);
        };
        const double pe = cpuNs(kTsProduceEnd), cb = cpuNs(kTsConsumeBegin);
        r.rt = (cb - pe) * 1e-3;
        r.g2c = (double(tWake) - pe) * 1e-3;
        r.c2g = (cb - double(tSignal)) * 1e-3;
        pool->release();
        return true;
    }

    // Spin-kernel cost per loop when nobody ever writes the flag (sets the timeout bound).
    void calibrateSpin() {
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        const uint32_t loops = 1u << 20;
        MTL::CommandBuffer* cb = q->commandBuffer();
        encSpin(cb, 0xFFFFFFFFu, nullptr, nullptr, loops);
        cb->commit();
        cb->waitUntilCompleted();
        const double s = cb->GPUEndTime() - cb->GPUStartTime();
        const double nsPerLoop = s * 1e9 / loops;
        spinMaxLoops = uint32_t(std::min(20e6 / nsPerLoop, 4.0e9));  // ~20 ms
        std::printf("spin kernel: %.1f ns/loop -> timeout bound %u loops (~20 ms)\n", nsPerLoop, spinMaxLoops);
        pool->release();
    }

    double burnUs(uint32_t loops, uint32_t groups = 1, uint32_t threads = 32) {
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        MTL::CommandBuffer* cb = q2->commandBuffer();
        MTL::ComputeCommandEncoder* e = cb->computeCommandEncoder();
        e->setComputePipelineState(burn);
        e->setBuffer(burnOut, 0, 0);
        e->setBytes(&loops, 4, 1);
        e->dispatchThreadgroups(MTL::Size(groups, 1, 1), MTL::Size(threads, 1, 1));
        e->endEncoding();
        cb->commit();
        cb->waitUntilCompleted();
        const double us = (cb->GPUEndTime() - cb->GPUStartTime()) * 1e6;
        pool->release();
        return us;
    }
    void calibrateBurn() {
        for (int i = 0; i < 3; ++i) burnUs(burnLoops);
        const double us = burnUs(burnLoops);
        burnLoops = uint32_t(std::max(1000.0, burnLoops * 200.0 / std::max(us, 1.0)));
        std::printf("busy kernel: %u loops ~ %.0f us (target 200 us)\n", burnLoops, burnUs(burnLoops));
        for (int i = 0; i < 3; ++i) burnUs(heavyLoops, 64, 256);
        const double hus = burnUs(heavyLoops, 64, 256);
        heavyLoops = uint32_t(std::max(100.0, heavyLoops * 200.0 / std::max(hus, 1.0)));
        std::printf("heavy kernel: %u loops x 64x256 ~ %.0f us (target 200 us)\n", heavyLoops,
                    burnUs(heavyLoops, 64, 256));
    }

    // Background load for busy / light / pingpong.
    void background(Variant var, const std::atomic<bool>& stop) {
        MTL::CommandBuffer* inflight[2] = {nullptr, nullptr};
        uint64_t v2 = ev2->signaledValue();
        uint32_t k = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
            if (var == kBusy || var == kHalf || var == kHeavy || var == kLight) {
                MTL::CommandBuffer*& slot = inflight[k++ & 1];
                if (slot != nullptr) { slot->waitUntilCompleted(); slot->release(); }
                MTL::CommandBuffer* cb = q2->commandBuffer();
                MTL::ComputeCommandEncoder* e = cb->computeCommandEncoder();
                e->setComputePipelineState(burn);
                e->setBuffer(burnOut, 0, 0);
                const uint32_t loops = var == kLight ? 16u : var == kHeavy ? heavyLoops : burnLoops;
                e->setBytes(&loops, 4, 1);
                if (var == kHeavy) e->dispatchThreadgroups(MTL::Size(64, 1, 1), MTL::Size(256, 1, 1));
                else e->dispatchThreadgroups(MTL::Size(1, 1, 1), MTL::Size(32, 1, 1));
                e->endEncoding();
                cb->commit();
                cb->retain();
                slot = cb;
                if (var == kHalf) cb->waitUntilCompleted();
                if (var == kLight || var == kHalf) {
                    const timespec ts{0, 200'000};
                    nanosleep(&ts, nullptr);
                }
            } else if (var == kPingpong) {
                const uint64_t a = ++v2, b = ++v2, c = ++v2;
                MTL::CommandBuffer* cb1 = q2->commandBuffer();
                MTL::ComputeCommandEncoder* e = cb1->computeCommandEncoder();
                e->setComputePipelineState(burn);
                e->setBuffer(burnOut, 0, 0);
                const uint32_t loops = 16u;
                e->setBytes(&loops, 4, 1);
                e->dispatchThreadgroups(MTL::Size(1, 1, 1), MTL::Size(32, 1, 1));
                e->endEncoding();
                cb1->encodeSignalEvent(ev2, a);
                MTL::CommandBuffer* cb2 = q2->commandBuffer();
                cb2->encodeWait(ev2, b);
                cb2->encodeSignalEvent(ev2, c);
                cb1->commit();
                cb2->commit();
                spinEvent(ev2, a, 1'000'000'000ull);
                ev2->setSignaledValue(b);
                ev2->waitUntilSignaledValue(c, 2000);
                cb2->waitUntilCompleted();
            }
            pool->release();
        }
        for (MTL::CommandBuffer* cb : inflight) {
            if (cb != nullptr) { cb->waitUntilCompleted(); cb->release(); }
        }
    }
};

void summarize(const std::vector<Row>& rows, int rounds) {
    std::printf("\n%-9s %5s | %-26s | %7s | %-26s | %-17s | %s\n", "variant", "n", "rt p10 / p50 / p90 / p99", "g2c p50",
                "c2g p10 / p50 / p90", "c2g <100/<190/rest", "timeouts spin/mem");
    for (int v = 0; v < kVariantCount; ++v) {
        std::vector<double> rt, g2c, c2g;
        int fast = 0, mid = 0, slow = 0, spinTo = 0, memTo = 0;
        for (const Row& r : rows) {
            if (r.variant != v) continue;
            rt.push_back(r.rt);
            g2c.push_back(r.g2c);
            c2g.push_back(r.c2g);
            if (r.c2g < 100) ++fast; else if (r.c2g < 190) ++mid; else ++slow;
            spinTo += r.spinTimeout;
            memTo += r.memWaitTimeout;
        }
        if (rt.empty()) continue;
        const double n = double(rt.size());
        std::printf("%-9s %5zu | %5.0f %6.0f %6.0f %6.0f | %7.1f | %6.0f %6.0f %6.0f      | %4.0f%% %4.0f%% %4.0f%% | %d / %d\n",
                    kVariantNames[v], rt.size(), pct(rt, .1), pct(rt, .5), pct(rt, .9), pct(rt, .99), pct(g2c, .5),
                    pct(c2g, .1), pct(c2g, .5), pct(c2g, .9), 100 * fast / n, 100 * mid / n, 100 * slow / n, spinTo,
                    memTo);
    }
    std::printf("\nrt p50 per round (drift check):\n%-9s", "variant");
    for (int k = 0; k < rounds; ++k) std::printf(" %5d", k);
    std::printf("\n");
    for (int v = 0; v < kVariantCount; ++v) {
        if (std::none_of(rows.begin(), rows.end(), [v](const Row& r) { return r.variant == v; })) continue;
        std::printf("%-9s", kVariantNames[v]);
        for (int k = 0; k < rounds; ++k) {
            std::vector<double> rt;
            for (const Row& r : rows) if (r.variant == v && r.round == k) rt.push_back(r.rt);
            std::printf(" %5.0f", pct(rt, .5));
        }
        std::printf("\n");
    }
}

}  // namespace

int main(int argc, char** argv) {
    mach_timebase_info(&g_tb);
    const char* outPath = argc > 1 ? argv[1] : "sync_probe.csv";
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 8;
    const uint32_t perBlock = argc > 3 ? uint32_t(std::atoi(argv[3])) : 250;
    const uint32_t warm = 30;
    std::vector<Variant> vars;
    {
        const std::string list = argc > 4 ? argv[4] : "idle,busy,half,heavy,light,pingpong";
        size_t a = 0;
        while (a <= list.size()) {
            const size_t b = std::min(list.find(',', a), list.size());
            const std::string name = list.substr(a, b - a);
            for (int v = 0; v < kVariantCount; ++v) if (name == kVariantNames[v]) vars.push_back(Variant(v));
            a = b + 1;
        }
    }
    const int nv = int(vars.size());

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    Probe p;
    p.init();
    std::printf("device: %s\n", p.dev->name()->utf8String());
    p.calibrateClock();
    p.calibrateSpin();
    p.calibrateBurn();

    std::vector<Row> rows;
    rows.reserve(size_t(rounds) * size_t(nv) * perBlock);
    uint32_t it = 0;
    for (int k = 0; k < rounds; ++k) {
        for (int j = 0; j < nv; ++j) {
            const Variant var = vars[size_t((j + k) % nv)];  // rotate the order every round
            std::atomic<bool> stop{false};
            std::thread bg;
            if (var == kBusy || var == kHalf || var == kHeavy || var == kLight || var == kPingpong) {
                bg = std::thread([&] { p.background(var, stop); });
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            p.calibrateClock();
            for (uint32_t i = 0; i < warm + perBlock; ++i) {
                Row r{};
                r.variant = var;
                r.round = k;
                r.iter = i;
                if (!p.roundTrip(var, it++, r)) {
                    std::fprintf(stderr, "hard failure in %s round %d iter %u\n", kVariantNames[var], k, i);
                    stop = true;
                    if (bg.joinable()) bg.join();
                    return 1;
                }
                if (i >= warm) rows.push_back(r);
            }
            stop = true;
            if (bg.joinable()) bg.join();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    FILE* f = std::fopen(outPath, "w");
    if (f != nullptr) {
        std::fprintf(f, "variant,round,iter,rt_us,g2c_us,c2g_us,spin_loops,spin_timeout,mem_wait_timeout\n");
        for (const Row& r : rows) {
            std::fprintf(f, "%s,%d,%u,%.3f,%.3f,%.3f,%u,%d,%d\n", kVariantNames[r.variant], r.round, r.iter, r.rt,
                         r.g2c, r.c2g, r.spinLoops, int(r.spinTimeout), int(r.memWaitTimeout));
        }
        std::fclose(f);
    }
    summarize(rows, rounds);
    pool->release();
    return 0;
}
