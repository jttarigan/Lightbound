#include "frame/MemoryPolicy.h"

namespace lb::frame {

using gfx::Backend;
using gfx::MemoryClass;

const char* transferPathName(TransferPath p) {
    switch (p) {
    case TransferPath::S1Copy: return "s1_copy";
    case TransferPath::S2Direct: return "s2_direct";
    case TransferPath::S2HostCached: return "s2_hostcached";
    case TransferPath::S2Rebar: return "s2_rebar";
    case TransferPath::S2Coherent: return "s2_coherent";
    }
    return "?";
}

namespace {
bool supported(const gfx::GfxCaps& caps, MemoryClass c) { return caps.memoryClassSupported[static_cast<u32>(c)]; }
} // namespace

bool pathMemory(TransferPath p, const gfx::GfxCaps& caps, PathMemory& out) {
    out = PathMemory{};
    const bool metal = caps.backend == Backend::Metal;
    switch (p) {
    case TransferPath::S1Copy:
        out.copy = true;
        out.down = MemoryClass::DeviceLocal;
        out.up = MemoryClass::DeviceLocal;
        out.downStaging = metal ? MemoryClass::Shared : MemoryClass::HostVisibleCached;
        out.upStaging = metal ? MemoryClass::Shared : MemoryClass::HostVisibleCoherent;
        break;
    case TransferPath::S2Direct:
        if (metal) {
            out.down = MemoryClass::Shared;
            out.up = MemoryClass::Shared;
        } else {
            // GPU writes system memory over PCIe, CPU reads cached; CPU writes into ReBAR when
            // available. The CPU never reads ReBAR memory in the default path (DECISIONS #17).
            out.down = MemoryClass::HostVisibleCached;
            out.up = caps.rebar ? MemoryClass::DeviceLocalHostVisible : MemoryClass::HostVisibleCoherent;
        }
        break;
    case TransferPath::S2HostCached:
        if (metal) return false;
        out.down = out.up = MemoryClass::HostVisibleCached;
        break;
    case TransferPath::S2Rebar:
        if (metal) return false;
        out.down = out.up = MemoryClass::DeviceLocalHostVisible;
        break;
    case TransferPath::S2Coherent:
        if (metal) return false;
        out.down = out.up = MemoryClass::HostVisibleCoherent;
        break;
    }
    if (!supported(caps, out.down) || !supported(caps, out.up)) return false;
    if (out.copy && (!supported(caps, out.downStaging) || !supported(caps, out.upStaging))) return false;
    return true;
}

} // namespace lb::frame
