#include "gfx/Gfx.h"

#if defined(LB_BACKEND_METAL)
#include "gfx/metal/MetalGfx.h"
#elif defined(LB_BACKEND_VULKAN)
#include "gfx/vulkan/VulkanGfx.h"
#else
#error "No graphics backend selected (define LB_BACKEND_METAL or LB_BACKEND_VULKAN)"
#endif

namespace lb::gfx {

const char* backendName(Backend b) {
    switch (b) {
    case Backend::Metal: return "metal";
    case Backend::Vulkan: return "vulkan";
    }
    return "?";
}

const char* memoryClassName(MemoryClass c) {
    switch (c) {
    case MemoryClass::DeviceLocal: return "DeviceLocal";
    case MemoryClass::Shared: return "Shared";
    case MemoryClass::HostVisibleCached: return "HostVisibleCached";
    case MemoryClass::HostVisibleCoherent: return "HostVisibleCoherent";
    case MemoryClass::DeviceLocalHostVisible: return "DeviceLocalHostVisible";
    }
    return "?";
}

Backend compiledBackend() {
#if defined(LB_BACKEND_METAL)
    return Backend::Metal;
#else
    return Backend::Vulkan;
#endif
}

std::unique_ptr<Gfx> createGfx() {
#if defined(LB_BACKEND_METAL)
    return createMetalGfx();
#else
    return createVulkanGfx();
#endif
}

} // namespace lb::gfx
