#pragma once
// The only graphics header game code may include. Backend implementations live in
// src/gfx/metal and src/gfx/vulkan and must implement identical semantics (R1).
#include "core/Types.h"

#include <memory>

struct SDL_Window;

namespace lb::gfx {

enum class Backend : u8 { Metal, Vulkan };
const char* backendName(Backend b);
/// The backend this binary was compiled with (LB_BACKEND).
Backend compiledBackend();

/// Memory classes for buffers (docs/05_ARCHITECTURE.md §4.2). Game/frame code picks a
/// class; the backend maps it to API storage. Requesting a class the backend cannot
/// provide is an error, never a silent fallback (R7).
enum class MemoryClass : u8 {
    DeviceLocal,             // GPU-private. Metal: Private. Vulkan: DEVICE_LOCAL (not host-visible).
    Shared,                  // Metal: MTLStorageModeShared (unified memory). Vulkan: rejected.
    HostVisibleCached,       // Vulkan: HOST_VISIBLE|HOST_CACHED system memory (GPU writes, CPU reads).
    HostVisibleCoherent,     // Vulkan: HOST_VISIBLE|HOST_COHERENT, uncached system memory (CPU writes, GPU reads).
    DeviceLocalHostVisible,  // Vulkan: DEVICE_LOCAL|HOST_VISIBLE (ReBAR / BAR). CPU write-only by policy.
};
constexpr u32 kMemoryClassCount = 5;
const char* memoryClassName(MemoryClass c);

// ---------------------------------------------------------------------------- handles
// Plain indices into backend-owned tables; 0 is the null handle.
struct BufferHandle { u32 id = 0; bool valid() const { return id != 0; } };
struct PipelineHandle { u32 id = 0; bool valid() const { return id != 0; } };
struct BindingSetHandle { u32 id = 0; bool valid() const { return id != 0; } };
struct TimelineHandle { u32 id = 0; bool valid() const { return id != 0; } };
struct TimestampPoolHandle { u32 id = 0; bool valid() const { return id != 0; } };
/// A recorded GPU segment (one command buffer with an optional wait before and signal
/// after, docs/05 §3.2). Slots are recycled; a handle is valid until it is submitted.
struct SegmentHandle { u32 id = 0; bool valid() const { return id != 0; } };

struct BufferDesc {
    u64 size = 0;
    MemoryClass memory = MemoryClass::DeviceLocal;
    /// Research buffers (docs/05 §5) count GPU copies into/out of them (R7 audit) and use
    /// untracked hazards on Metal (explicit sync only).
    bool research = false;
    const char* name = "";
};

/// Compute pipeline from a Slang-compiled pass. Binding convention (DECISIONS #18): the
/// pass declares `bufferCount` RW/structured buffers first (binding i / [[buffer(i)]]),
/// then optionally one `[[vk::push_constant]]` block of `pushConstantBytes` bytes.
struct ComputePipelineDesc {
    const char* shader = "";     // file stem in the shader dir: <shader>.spv / <shader>.metallib
    const char* entry = "";      // entry point name
    u32 bufferCount = 0;
    u32 pushConstantBytes = 0;   // ≤ kMaxPushConstantBytes
    u32 threadsPerGroup = 64;    // must equal the [numthreads] of the entry point
};
constexpr u32 kMaxPushConstantBytes = 128;
constexpr u32 kMaxBindings = 8;

/// Memory access kinds for explicit barriers (research buffers are untracked).
enum AccessBits : u32 {
    kAccessComputeRead = 1u << 0,
    kAccessComputeWrite = 1u << 1,
    kAccessCopyRead = 1u << 2,
    kAccessCopyWrite = 1u << 3,
    kAccessHostRead = 1u << 4,
    kAccessHostWrite = 1u << 5,
};

/// Timestamps written at the start / end of one GPU pass (one encoder per pass).
constexpr u32 kNoTimestamp = 0xFFFFFFFFu;
struct PassTimestamps {
    TimestampPoolHandle pool{};
    u32 begin = kNoTimestamp;
    u32 end = kNoTimestamp;
};

/// Timeline operations attached to a segment: wait before its first command, signal after
/// its last. value 0 = none.
struct SegmentSync {
    TimelineHandle waitTimeline{};
    u64 waitValue = 0;
    TimelineHandle signalTimeline{};
    u64 signalValue = 0;
};

enum class CpuWaitMode : u8 { Spin, Block };

struct GfxDesc {
    SDL_Window* window = nullptr;   // nullptr = headless (no swapchain; microbench)
    bool vsync = true;
    bool validation = false;        // Vulkan validation layer / Metal API validation
    const char* shaderDir = nullptr; // directory holding *.spv / *.metallib
};

struct GfxCaps {
    Backend backend = Backend::Metal;
    char deviceName[256] = {};
    char apiVersion[64] = {};
    char driverInfo[128] = {};       // driver name + version (Vulkan), OS Metal family (Metal)
    bool unifiedMemory = false;      // CPU and GPU share physical memory
    bool rebar = false;              // DEVICE_LOCAL|HOST_VISIBLE heap larger than 256 MB (Vulkan)
    u64 deviceLocalHostVisibleHeapBytes = 0; // size of the BAR heap (0 if none)
    bool hostCachedMemory = false;   // HOST_VISIBLE|HOST_CACHED memory type exists (Vulkan)
    bool timelineSync = false;       // MTLSharedEvent / timeline VkSemaphore available
    bool gpuTimestamps = false;      // per-pass timestamps available
    bool clockCalibration = false;   // GPU→CPU clock correlation available
    char hostTimeDomain[48] = {};    // clock used to correlate GPU timestamps
    f64 gpuTimestampPeriodNs = 0.0;  // ns per GPU timestamp tick
    bool memoryClassSupported[kMemoryClassCount] = {};
    char memoryClassInfo[kMemoryClassCount][96] = {}; // what each class maps to (for CSV headers)
    u32 backbufferWidth = 0;
    u32 backbufferHeight = 0;
};

/// Result of one clock calibration (docs/05 §8).
struct ClockCalibration {
    bool valid = false;
    f64 maxDeviationNs = 0.0;   // uncertainty reported by the API / sampling window
};

struct ClearColor { f32 r, g, b, a; };
constexpr ClearColor kClearCave = {11.f / 255.f, 11.f / 255.f, 16.f / 255.f, 1.f}; // #0b0b10

class Gfx {
public:
    virtual ~Gfx() = default;

    virtual bool init(const GfxDesc& desc) = 0;
    virtual void shutdown() = 0;
    virtual const GfxCaps& caps() const = 0;

    // ------------------------------------------------------------------ presentation (play)
    /// Window pixel size changed.
    virtual void resize(u32 pixelWidth, u32 pixelHeight) = 0;
    /// Acquire the backbuffer. Returns false when no frame can be rendered (minimised,
    /// swapchain rebuilt, headless); the caller just skips rendering this iteration.
    virtual bool beginFrame() = 0;
    virtual void clearBackbuffer(ClearColor color) = 0;
    /// Submit and present.
    virtual void endFrame() = 0;

    // ------------------------------------------------------------------ resources (setup time)
    virtual bool supportsMemoryClass(MemoryClass c) const { return caps().memoryClassSupported[static_cast<u32>(c)]; }
    virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;
    virtual void destroyBuffer(BufferHandle b) = 0;
    /// Persistent CPU mapping; nullptr for DeviceLocal buffers.
    virtual void* mappedPtr(BufferHandle b) = 0;
    /// Make CPU writes visible to the GPU / GPU writes visible to the CPU for memory that
    /// is not host-coherent. No-ops where the memory is coherent (Metal Shared, Vulkan COHERENT).
    virtual void flushHostWrites(BufferHandle b, u64 offset, u64 size) = 0;
    virtual void invalidateHostReads(BufferHandle b, u64 offset, u64 size) = 0;

    virtual PipelineHandle createComputePipeline(const ComputePipelineDesc& desc) = 0;
    virtual BindingSetHandle createBindingSet(PipelineHandle p, const BufferHandle* buffers, u32 count) = 0;
    virtual void destroyBindingSet(BindingSetHandle s) = 0;

    virtual TimelineHandle createTimeline(u64 initialValue) = 0;
    virtual TimestampPoolHandle createTimestampPool(u32 count) = 0;

    // ------------------------------------------------------------------ recording & submission
    /// Starts recording a segment. Blocks if the recycled slot is still executing.
    virtual SegmentHandle beginSegment(const SegmentSync& sync) = 0;
    virtual void cmdDispatch(SegmentHandle s, PipelineHandle p, BindingSetHandle set, u32 groupsX,
                             const void* pushConstants, u32 pushBytes, const PassTimestamps& ts) = 0;
    /// GPU buffer copy. Counted by researchCopyCount() when either buffer is a research buffer.
    virtual void cmdCopyBuffer(SegmentHandle s, BufferHandle src, u64 srcOffset, BufferHandle dst, u64 dstOffset,
                               u64 size, const PassTimestamps& ts) = 0;
    /// Memory dependency between earlier and later commands (and the host) in this segment.
    virtual void cmdBarrier(SegmentHandle s, u32 srcAccess, u32 dstAccess) = 0;
    /// Finishes recording (encodes the segment's signal).
    virtual void endSegment(SegmentHandle s) = 0;
    /// Submits segments in order in one API call (chain mode submits a whole frame at once).
    virtual bool submit(const SegmentHandle* segments, u32 count) = 0;
    /// Blocks until every submitted segment has completed.
    virtual bool waitIdle() = 0;

    // ------------------------------------------------------------------ timeline sync (docs/05 §4.1)
    virtual void cpuSignal(TimelineHandle t, u64 value) = 0;
    /// Returns false on timeout.
    virtual bool cpuWait(TimelineHandle t, u64 value, CpuWaitMode mode, u64 timeoutNs) = 0;
    virtual u64 timelineValue(TimelineHandle t) = 0;

    // ------------------------------------------------------------------ timestamps (docs/05 §8)
    /// Must be called (CPU side) before timestamps in [first, first+count) are rewritten.
    virtual void resetTimestamps(TimestampPoolHandle pool, u32 first, u32 count) = 0;
    /// Converts completed timestamps to the CPU steady clock (ns, same domain as lb::nowNs()).
    /// Unwritten or unavailable samples come back as 0. Call only after waitIdle().
    virtual bool readTimestamps(TimestampPoolHandle pool, u32 first, u32 count, u64* outSteadyNs) = 0;
    /// Re-correlates the GPU and CPU clocks (at startup and periodically).
    virtual ClockCalibration calibrateClocks() = 0;

    // ------------------------------------------------------------------ audit
    /// GPU copies that touched a research buffer since init (R7).
    virtual u64 researchCopyCount() const = 0;
    /// API/validation errors observed by the backend since init (must be 0 in accepted runs).
    virtual u64 errorCount() const = 0;
};

/// Creates the compiled-in backend.
std::unique_ptr<Gfx> createGfx();

} // namespace lb::gfx
