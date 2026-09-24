#pragma once
// Memory placement of research buffers per transfer path (docs/05_ARCHITECTURE.md §4.2).
// The microbenchmark (M1) and the frame orchestrator (M4) both use this table so that the
// measured paths are exactly the ones the game runs.
#include "gfx/Gfx.h"

namespace lb::frame {

enum class TransferPath : u8 {
    S1Copy,        // GPU-private buffers + staging copies each handoff
    S2Direct,      // default S2 placement of §4.2
    S2HostCached,  // Vulkan sensitivity variant: both directions HOST_VISIBLE|HOST_CACHED
    S2Rebar,       // Vulkan sensitivity variant: both directions DEVICE_LOCAL|HOST_VISIBLE (CPU reads over PCIe)
    S2Coherent,    // Vulkan sensitivity variant: both directions HOST_VISIBLE|HOST_COHERENT (uncached)
};

/// GPU→CPU data ("down") and CPU→GPU data ("up"). For copy paths the GPU works on the
/// `down`/`up` buffers and the CPU on the staging buffers; for direct paths both
/// processors use `down`/`up` and there are no staging buffers.
struct PathMemory {
    bool copy = false;
    gfx::MemoryClass down = gfx::MemoryClass::DeviceLocal;
    gfx::MemoryClass up = gfx::MemoryClass::DeviceLocal;
    gfx::MemoryClass downStaging = gfx::MemoryClass::DeviceLocal;
    gfx::MemoryClass upStaging = gfx::MemoryClass::DeviceLocal;
};

const char* transferPathName(TransferPath p);

/// Fills `out` for this backend. Returns false when the path does not exist on this
/// backend/device (e.g. Vulkan-only variants on Metal, no BAR heap); never substitutes.
bool pathMemory(TransferPath p, const gfx::GfxCaps& caps, PathMemory& out);

} // namespace lb::frame
