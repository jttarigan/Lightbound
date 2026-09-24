// Vulkan 1.3 backend. Windows 11 primary; on macOS links MoltenVK directly as the
// optional API-control condition (docs/06 §2, P-M5-VK).
#include "gfx/vulkan/VulkanGfx.h"

#include "core/Log.h"
#include "core/Time.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <time.h>
#endif

namespace lb::gfx {
namespace {

#define LB_VK_CHECK(expr)                                                              \
    do {                                                                               \
        const VkResult lb_vk_r = (expr);                                               \
        if (lb_vk_r != VK_SUCCESS) {                                                   \
            LB_LOG_ERROR("%s failed: VkResult %d", #expr, static_cast<int>(lb_vk_r)); \
            return false;                                                              \
        }                                                                              \
    } while (0)

bool hasExtension(const std::vector<VkExtensionProperties>& list, const char* name) {
    for (const auto& e : list) {
        if (std::strcmp(e.extensionName, name) == 0) return true;
    }
    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data,
                                             void* user) {
    auto* counter = static_cast<std::atomic<u64>*>(user);
    const char* msg = (data != nullptr && data->pMessage != nullptr) ? data->pMessage : "";
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) {
        counter->fetch_add(1, std::memory_order_relaxed);
        LB_LOG_ERROR("[vk] %s", msg);
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0) {
        LB_LOG_WARN("[vk] %s", msg);
    } else {
        LB_LOG_TRACE("[vk] %s", msg);
    }
    return VK_FALSE;
}

std::string memoryFlagsString(VkMemoryPropertyFlags f) {
    std::string s;
    auto add = [&](VkMemoryPropertyFlags bit, const char* name) {
        if ((f & bit) == 0) return;
        if (!s.empty()) s += "|";
        s += name;
    };
    add(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "DEVICE_LOCAL");
    add(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "HOST_VISIBLE");
    add(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "HOST_COHERENT");
    add(VK_MEMORY_PROPERTY_HOST_CACHED_BIT, "HOST_CACHED");
    return s.empty() ? "0" : s;
}

// Host clock readers for calibrated timestamps (value in the domain's native unit).
u64 readHostDomain(VkTimeDomainKHR domain) {
#if defined(_WIN32)
    (void)domain;
    LARGE_INTEGER v;
    QueryPerformanceCounter(&v);
    return static_cast<u64>(v.QuadPart);
#else
    timespec ts{};
    clock_gettime(domain == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR ? CLOCK_MONOTONIC_RAW : CLOCK_MONOTONIC, &ts);
    return static_cast<u64>(ts.tv_sec) * 1000000000ull + static_cast<u64>(ts.tv_nsec);
#endif
}

u64 hostDomainToNs(VkTimeDomainKHR domain, u64 v) {
#if defined(_WIN32)
    if (domain == VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        const u64 freq = static_cast<u64>(f.QuadPart);
        return (v / freq) * 1000000000ull + ((v % freq) * 1000000000ull) / freq;
    }
#else
    (void)domain;
#endif
    return v;
}

class VulkanGfx final : public Gfx {
public:
    ~VulkanGfx() override { shutdown(); }

    bool init(const GfxDesc& desc) override {
        m_window = desc.window;
        m_headless = desc.window == nullptr;
        m_validation = desc.validation;
        m_vsync = desc.vsync;
        m_shaderDir = desc.shaderDir != nullptr ? desc.shaderDir : "";

        if (!m_headless) {
#if defined(LB_VULKAN_LIBRARY_PATH)
            const char* libPath = LB_VULKAN_LIBRARY_PATH;
#else
            const char* libPath = nullptr;
#endif
            // SDL_CreateWindow(SDL_WINDOW_VULKAN) already loads the default loader; passing a
            // different path then fails, so only request our path when nothing is loaded yet.
            // Either way this takes a reference that shutdown() releases.
            if (SDL_Vulkan_GetVkGetInstanceProcAddr() != nullptr) libPath = nullptr;
            if (!SDL_Vulkan_LoadLibrary(libPath)) {
                LB_LOG_ERROR("SDL_Vulkan_LoadLibrary failed: %s", SDL_GetError());
                return false;
            }
            m_libraryLoaded = true;
        }
        logLoaderPath();

        if (!createInstance()) return false;
        if (!m_headless && !SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
            LB_LOG_ERROR("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
            return false;
        }
        if (!pickPhysicalDevice()) return false;
        if (!createDevice()) return false;
        if (!loadFunctions()) return false;
        if (!createAllocator()) return false;
        describeMemoryClasses();
        if (!createSegmentSlots()) return false;
        if (!m_headless) {
            if (!createSwapchain()) return false;
            if (!createFrames()) return false;
        }
        chooseTimeDomain();
        calibrateClocks();

        LB_LOG_INFO("Vulkan device: %s (%s, %s, unified: %s, ReBAR: %s, host-cached: %s, clock: %s, %s)",
                    m_caps.deviceName, m_caps.apiVersion, m_caps.driverInfo, m_caps.unifiedMemory ? "yes" : "no",
                    m_caps.rebar ? "yes" : "no", m_caps.hostCachedMemory ? "yes" : "no", m_caps.hostTimeDomain,
                    m_headless ? "headless" : (m_vsync ? "vsync on" : "vsync off"));
        return true;
    }

    void shutdown() override {
        if (m_device != VK_NULL_HANDLE) vkDeviceWaitIdle(m_device);
        destroyFrames();
        destroySwapchain();
        if (m_device != VK_NULL_HANDLE) {
            for (SegmentSlot& s : m_segments) {
                if (s.pool != VK_NULL_HANDLE) vkDestroyCommandPool(m_device, s.pool, nullptr);
                s = SegmentSlot{};
            }
            if (m_segTimeline != VK_NULL_HANDLE) vkDestroySemaphore(m_device, m_segTimeline, nullptr);
            m_segTimeline = VK_NULL_HANDLE;
            for (VkSemaphore t : m_timelines) vkDestroySemaphore(m_device, t, nullptr);
            m_timelines.clear();
            for (TsPool& p : m_tsPools) vkDestroyQueryPool(m_device, p.pool, nullptr);
            m_tsPools.clear();
            for (Pipeline& p : m_pipelines) {
                vkDestroyPipeline(m_device, p.pipeline, nullptr);
                vkDestroyPipelineLayout(m_device, p.layout, nullptr);
                vkDestroyDescriptorSetLayout(m_device, p.setLayout, nullptr);
            }
            m_pipelines.clear();
            m_bindingSets.clear();
            if (m_descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        if (m_allocator != VK_NULL_HANDLE) {
            for (Buffer& b : m_buffers) {
                if (b.buffer != VK_NULL_HANDLE) vmaDestroyBuffer(m_allocator, b.buffer, b.alloc);
            }
            m_buffers.clear();
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE) { vkDestroyDevice(m_device, nullptr); m_device = VK_NULL_HANDLE; }
        if (m_surface != VK_NULL_HANDLE) {
            SDL_Vulkan_DestroySurface(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
        if (m_messenger != VK_NULL_HANDLE) {
            auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyFn != nullptr) destroyFn(m_instance, m_messenger, nullptr);
            m_messenger = VK_NULL_HANDLE;
        }
        if (m_instance != VK_NULL_HANDLE) { vkDestroyInstance(m_instance, nullptr); m_instance = VK_NULL_HANDLE; }
        if (m_libraryLoaded) { SDL_Vulkan_UnloadLibrary(); m_libraryLoaded = false; }
    }

    const GfxCaps& caps() const override { return m_caps; }

    // ------------------------------------------------------------------ presentation

    void resize(u32, u32) override { m_needRecreate = true; }

    bool beginFrame() override {
        if (m_headless) return false;
        if (m_needRecreate) {
            if (!recreateSwapchain()) return false;
        }
        if (m_swapchain == VK_NULL_HANDLE) return false;

        Frame& f = m_frames[m_frameIndex];
        vkWaitForFences(m_device, 1, &f.fence, VK_TRUE, UINT64_MAX);

        const VkResult acq = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, f.acquire,
                                                   VK_NULL_HANDLE, &m_imageIndex);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
            m_needRecreate = true;
            return false;
        }
        if (acq == VK_SUBOPTIMAL_KHR) {
            m_needRecreate = true;
        } else if (acq != VK_SUCCESS) {
            LB_LOG_ERROR("vkAcquireNextImageKHR failed: %d", static_cast<int>(acq));
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        vkResetFences(m_device, 1, &f.fence);
        vkResetCommandPool(m_device, f.pool, 0);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(f.cmd, &bi);
        m_inFrame = true;
        return true;
    }

    void clearBackbuffer(ClearColor c) override {
        Frame& f = m_frames[m_frameIndex];
        VkImage image = m_images[m_imageIndex];

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;

        VkImageMemoryBarrier toClear{};
        toClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toClear.srcAccessMask = 0;
        toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toClear.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toClear.image = image;
        toClear.subresourceRange = range;
        vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toClear);

        VkClearColorValue color{};
        color.float32[0] = c.r;
        color.float32[1] = c.g;
        color.float32[2] = c.b;
        color.float32[3] = c.a;
        vkCmdClearColorImage(f.cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

        VkImageMemoryBarrier toPresent = toClear;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toPresent.dstAccessMask = 0;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &toPresent);
    }

    void endFrame() override {
        if (!m_inFrame) return;
        Frame& f = m_frames[m_frameIndex];
        vkEndCommandBuffer(f.cmd);

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &f.acquire;
        si.pWaitDstStageMask = &waitStage;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &f.cmd;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &m_renderFinished[m_imageIndex];
        const VkResult sub = vkQueueSubmit(m_queue, 1, &si, f.fence);
        if (sub != VK_SUCCESS) {
            LB_LOG_ERROR("vkQueueSubmit failed: %d", static_cast<int>(sub));
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
        }

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &m_renderFinished[m_imageIndex];
        pi.swapchainCount = 1;
        pi.pSwapchains = &m_swapchain;
        pi.pImageIndices = &m_imageIndex;
        const VkResult pres = vkQueuePresentKHR(m_queue, &pi);
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
            m_needRecreate = true;
        } else if (pres != VK_SUCCESS) {
            LB_LOG_ERROR("vkQueuePresentKHR failed: %d", static_cast<int>(pres));
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
        }

        m_frameIndex = (m_frameIndex + 1) % kFramesInFlight;
        m_inFrame = false;
    }

    // ------------------------------------------------------------------ resources

    BufferHandle createBuffer(const BufferDesc& desc) override {
        const i32 type = m_classType[static_cast<u32>(desc.memory)];
        if (type < 0) {
            LB_LOG_ERROR("buffer '%s': memory class %s is not available on this device", desc.name,
                         memoryClassName(desc.memory));
            return {};
        }
        const VkMemoryPropertyFlags flags = m_memProps.memoryTypes[type].propertyFlags;
        const bool hostVisible = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;

        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = desc.size;
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_UNKNOWN;
        aci.memoryTypeBits = 1u << static_cast<u32>(type); // exactly the class's type: no silent fallback (R7)
        aci.flags = hostVisible ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;

        Buffer b;
        VmaAllocationInfo info{};
        const VkResult r = vmaCreateBuffer(m_allocator, &bci, &aci, &b.buffer, &b.alloc, &info);
        if (r != VK_SUCCESS) {
            LB_LOG_ERROR("vmaCreateBuffer('%s', %llu B, %s) failed: %d", desc.name,
                         static_cast<unsigned long long>(desc.size), memoryClassName(desc.memory), static_cast<int>(r));
            return {};
        }
        b.mapped = hostVisible ? info.pMappedData : nullptr;
        b.memory = desc.memory;
        b.research = desc.research;
        b.size = desc.size;
        return {addSlot(m_buffers, b)};
    }

    void destroyBuffer(BufferHandle h) override {
        Buffer* b = buffer(h);
        if (b == nullptr) return;
        vmaDestroyBuffer(m_allocator, b->buffer, b->alloc);
        *b = Buffer{};
    }

    void* mappedPtr(BufferHandle h) override {
        Buffer* b = buffer(h);
        return b != nullptr ? b->mapped : nullptr;
    }

    void flushHostWrites(BufferHandle h, u64 offset, u64 size) override {
        Buffer* b = buffer(h);
        if (b != nullptr && b->mapped != nullptr) vmaFlushAllocation(m_allocator, b->alloc, offset, size);
    }

    void invalidateHostReads(BufferHandle h, u64 offset, u64 size) override {
        Buffer* b = buffer(h);
        if (b != nullptr && b->mapped != nullptr) vmaInvalidateAllocation(m_allocator, b->alloc, offset, size);
    }

    PipelineHandle createComputePipeline(const ComputePipelineDesc& desc) override {
        if (desc.bufferCount > kMaxBindings || desc.pushConstantBytes > kMaxPushConstantBytes) {
            LB_LOG_ERROR("pipeline %s:%s exceeds binding/push-constant limits", desc.shader, desc.entry);
            return {};
        }
        const std::string path = m_shaderDir + desc.shader + ".spv";
        std::vector<u32> code;
        if (!readSpirv(path, code)) return {};

        Pipeline p;
        p.bufferCount = desc.bufferCount;
        p.pushBytes = desc.pushConstantBytes;

        VkDescriptorSetLayoutBinding bindings[kMaxBindings]{};
        for (u32 i = 0; i < desc.bufferCount; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo dl{};
        dl.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dl.bindingCount = desc.bufferCount;
        dl.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(m_device, &dl, nullptr, &p.setLayout) != VK_SUCCESS) return {};

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pcr.size = desc.pushConstantBytes;
        VkPipelineLayoutCreateInfo pl{};
        pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pl.setLayoutCount = 1;
        pl.pSetLayouts = &p.setLayout;
        pl.pushConstantRangeCount = desc.pushConstantBytes > 0 ? 1u : 0u;
        pl.pPushConstantRanges = &pcr;
        if (vkCreatePipelineLayout(m_device, &pl, nullptr, &p.layout) != VK_SUCCESS) return {};

        VkShaderModuleCreateInfo smi{};
        smi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smi.codeSize = code.size() * sizeof(u32);
        smi.pCode = code.data();
        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(m_device, &smi, nullptr, &module) != VK_SUCCESS) {
            LB_LOG_ERROR("vkCreateShaderModule failed for %s", path.c_str());
            return {};
        }
        VkComputePipelineCreateInfo cpi{};
        cpi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cpi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        cpi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        cpi.stage.module = module;
        cpi.stage.pName = desc.entry;
        cpi.layout = p.layout;
        const VkResult r = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &cpi, nullptr, &p.pipeline);
        vkDestroyShaderModule(m_device, module, nullptr);
        if (r != VK_SUCCESS) {
            LB_LOG_ERROR("vkCreateComputePipelines(%s:%s) failed: %d", desc.shader, desc.entry, static_cast<int>(r));
            return {};
        }
        return {addSlot(m_pipelines, p)};
    }

    BindingSetHandle createBindingSet(PipelineHandle ph, const BufferHandle* buffers, u32 count) override {
        const Pipeline* p = pipeline(ph);
        if (p == nullptr || count != p->bufferCount) {
            LB_LOG_ERROR("createBindingSet: pipeline expects %u buffers, got %u", p != nullptr ? p->bufferCount : 0u,
                         count);
            return {};
        }
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = m_descriptorPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &p->setLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(m_device, &ai, &set) != VK_SUCCESS) {
            LB_LOG_ERROR("vkAllocateDescriptorSets failed");
            return {};
        }
        VkDescriptorBufferInfo infos[kMaxBindings]{};
        VkWriteDescriptorSet writes[kMaxBindings]{};
        for (u32 i = 0; i < count; ++i) {
            const Buffer* b = buffer(buffers[i]);
            if (b == nullptr) {
                LB_LOG_ERROR("createBindingSet: invalid buffer at binding %u", i);
                vkFreeDescriptorSets(m_device, m_descriptorPool, 1, &set);
                return {};
            }
            infos[i].buffer = b->buffer;
            infos[i].range = VK_WHOLE_SIZE;
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = set;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &infos[i];
        }
        vkUpdateDescriptorSets(m_device, count, writes, 0, nullptr);
        return {addSlot(m_bindingSets, set)};
    }

    void destroyBindingSet(BindingSetHandle h) override {
        VkDescriptorSet* s = slot(m_bindingSets, h.id);
        if (s == nullptr || *s == VK_NULL_HANDLE) return;
        vkFreeDescriptorSets(m_device, m_descriptorPool, 1, s);
        *s = VK_NULL_HANDLE;
    }

    TimelineHandle createTimeline(u64 initialValue) override {
        VkSemaphore s = VK_NULL_HANDLE;
        if (!createTimelineSemaphore(initialValue, s)) return {};
        return {addSlot(m_timelines, s)};
    }

    TimestampPoolHandle createTimestampPool(u32 count) override {
        if (!m_caps.gpuTimestamps) return {};
        VkQueryPoolCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qi.queryCount = count;
        TsPool p;
        if (vkCreateQueryPool(m_device, &qi, nullptr, &p.pool) != VK_SUCCESS) {
            LB_LOG_ERROR("vkCreateQueryPool(%u) failed", count);
            return {};
        }
        m_fn.resetQueryPool(m_device, p.pool, 0, count);
        p.count = count;
        p.written.assign(count, 0);
        return {addSlot(m_tsPools, p)};
    }

    // ------------------------------------------------------------------ recording

    SegmentHandle beginSegment(const SegmentSync& sync) override {
        const u32 idx = m_nextSegment;
        m_nextSegment = (m_nextSegment + 1) % kSegmentSlots;
        SegmentSlot& s = m_segments[idx];
        if (s.completionValue != 0) waitSegTimeline(s.completionValue);
        vkResetCommandPool(m_device, s.pool, 0);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(s.cmd, &bi);
        s.sync = sync;
        s.recording = true;
        s.completionValue = 0;
        return {idx + 1};
    }

    void cmdDispatch(SegmentHandle sh, PipelineHandle ph, BindingSetHandle bh, u32 groupsX, const void* push,
                     u32 pushBytes, const PassTimestamps& ts) override {
        SegmentSlot* s = segment(sh);
        const Pipeline* p = pipeline(ph);
        VkDescriptorSet* set = slot(m_bindingSets, bh.id);
        if (s == nullptr || p == nullptr || set == nullptr || *set == VK_NULL_HANDLE || pushBytes != p->pushBytes) {
            LB_LOG_ERROR("cmdDispatch: invalid handle or push-constant size");
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        writeTimestamp(s->cmd, ts.pool, ts.begin, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
        vkCmdBindPipeline(s->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p->pipeline);
        vkCmdBindDescriptorSets(s->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p->layout, 0, 1, set, 0, nullptr);
        if (pushBytes > 0) vkCmdPushConstants(s->cmd, p->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushBytes, push);
        vkCmdDispatch(s->cmd, groupsX, 1, 1);
        writeTimestamp(s->cmd, ts.pool, ts.end, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    }

    void cmdCopyBuffer(SegmentHandle sh, BufferHandle src, u64 srcOffset, BufferHandle dst, u64 dstOffset, u64 size,
                       const PassTimestamps& ts) override {
        SegmentSlot* s = segment(sh);
        const Buffer* a = buffer(src);
        const Buffer* b = buffer(dst);
        if (s == nullptr || a == nullptr || b == nullptr) {
            LB_LOG_ERROR("cmdCopyBuffer: invalid handle");
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (a->research || b->research) ++m_researchCopies;
        writeTimestamp(s->cmd, ts.pool, ts.begin, VK_PIPELINE_STAGE_2_COPY_BIT);
        VkBufferCopy region{srcOffset, dstOffset, size};
        vkCmdCopyBuffer(s->cmd, a->buffer, b->buffer, 1, &region);
        writeTimestamp(s->cmd, ts.pool, ts.end, VK_PIPELINE_STAGE_2_COPY_BIT);
    }

    void cmdBarrier(SegmentHandle sh, u32 srcAccess, u32 dstAccess) override {
        SegmentSlot* s = segment(sh);
        if (s == nullptr) return;
        VkMemoryBarrier2 mb{};
        mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        accessToVk(srcAccess, true, mb.srcStageMask, mb.srcAccessMask);
        accessToVk(dstAccess, false, mb.dstStageMask, mb.dstAccessMask);
        VkDependencyInfo di{};
        di.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        di.memoryBarrierCount = 1;
        di.pMemoryBarriers = &mb;
        m_fn.cmdPipelineBarrier2(s->cmd, &di);
    }

    void endSegment(SegmentHandle sh) override {
        SegmentSlot* s = segment(sh);
        if (s == nullptr) return;
        vkEndCommandBuffer(s->cmd);
        s->recording = false;
    }

    bool submit(const SegmentHandle* segments, u32 count) override {
        if (count > kSegmentSlots) return false;
        VkSubmitInfo2 infos[kSegmentSlots]{};
        VkCommandBufferSubmitInfo cmds[kSegmentSlots]{};
        VkSemaphoreSubmitInfo waits[kSegmentSlots]{};
        VkSemaphoreSubmitInfo signals[kSegmentSlots][2]{};
        for (u32 i = 0; i < count; ++i) {
            SegmentSlot* s = segment(segments[i]);
            if (s == nullptr || s->recording || s->completionValue != 0) {
                LB_LOG_ERROR("submit: segment %u not ready", i);
                m_errorCount.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            cmds[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            cmds[i].commandBuffer = s->cmd;
            infos[i].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            infos[i].commandBufferInfoCount = 1;
            infos[i].pCommandBufferInfos = &cmds[i];
            if (s->sync.waitValue != 0) {
                waits[i].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                waits[i].semaphore = timeline(s->sync.waitTimeline);
                waits[i].value = s->sync.waitValue;
                // ALL_COMMANDS so that the segment's first timestamp also waits (docs/05 §8).
                waits[i].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                infos[i].waitSemaphoreInfoCount = 1;
                infos[i].pWaitSemaphoreInfos = &waits[i];
            }
            u32 n = 0;
            if (s->sync.signalValue != 0) {
                signals[i][n].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                signals[i][n].semaphore = timeline(s->sync.signalTimeline);
                signals[i][n].value = s->sync.signalValue;
                signals[i][n].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                ++n;
            }
            s->completionValue = ++m_segCounter;
            signals[i][n].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signals[i][n].semaphore = m_segTimeline;
            signals[i][n].value = s->completionValue;
            signals[i][n].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            ++n;
            infos[i].signalSemaphoreInfoCount = n;
            infos[i].pSignalSemaphoreInfos = signals[i];
        }
        const VkResult r = m_fn.queueSubmit2(m_queue, count, infos, VK_NULL_HANDLE);
        if (r != VK_SUCCESS) {
            LB_LOG_ERROR("vkQueueSubmit2 failed: %d", static_cast<int>(r));
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    bool waitIdle() override {
        if (m_segCounter == 0) return true;
        const bool ok = waitSegTimeline(m_segCounter);
        for (SegmentSlot& s : m_segments) s.completionValue = 0;
        return ok;
    }

    // ------------------------------------------------------------------ sync

    void cpuSignal(TimelineHandle t, u64 value) override {
        VkSemaphoreSignalInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        si.semaphore = timeline(t);
        si.value = value;
        const VkResult r = m_fn.signalSemaphore(m_device, &si);
        if (r != VK_SUCCESS) {
            LB_LOG_ERROR("vkSignalSemaphore failed: %d", static_cast<int>(r));
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    bool cpuWait(TimelineHandle t, u64 value, CpuWaitMode mode, u64 timeoutNs) override {
        VkSemaphore sem = timeline(t);
        if (mode == CpuWaitMode::Block) {
            VkSemaphoreWaitInfo wi{};
            wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wi.semaphoreCount = 1;
            wi.pSemaphores = &sem;
            wi.pValues = &value;
            return m_fn.waitSemaphores(m_device, &wi, timeoutNs) == VK_SUCCESS;
        }
        const u64 deadline = nowNs() + timeoutNs;
        u64 v = 0;
        while (true) {
            if (m_fn.getSemaphoreCounterValue(m_device, sem, &v) != VK_SUCCESS) return false;
            if (v >= value) return true;
            if (nowNs() > deadline) return false;
        }
    }

    u64 timelineValue(TimelineHandle t) override {
        u64 v = 0;
        m_fn.getSemaphoreCounterValue(m_device, timeline(t), &v);
        return v;
    }

    // ------------------------------------------------------------------ timestamps

    void resetTimestamps(TimestampPoolHandle h, u32 first, u32 count) override {
        TsPool* p = tsPool(h);
        if (p == nullptr || first + count > p->count) return;
        m_fn.resetQueryPool(m_device, p->pool, first, count);
        for (u32 i = first; i < first + count; ++i) p->written[i] = 0;
    }

    bool readTimestamps(TimestampPoolHandle h, u32 first, u32 count, u64* out) override {
        for (u32 i = 0; i < count; ++i) out[i] = 0;
        TsPool* p = tsPool(h);
        if (p == nullptr || first + count > p->count) return false;
        for (u32 i = 0; i < count; ++i) {
            if (p->written[first + i] == 0) continue;
            u64 v = 0;
            const VkResult r = vkGetQueryPoolResults(m_device, p->pool, first + i, 1, sizeof(u64), &v, sizeof(u64),
                                                     VK_QUERY_RESULT_64_BIT);
            if (r != VK_SUCCESS) continue;
            out[i] = gpuToSteady(v & m_timestampMask);
        }
        return true;
    }

    ClockCalibration calibrateClocks() override {
        ClockCalibration c;
        if (!m_caps.clockCalibration) return c;
        VkCalibratedTimestampInfoKHR infos[2]{};
        infos[0].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR;
        infos[0].timeDomain = VK_TIME_DOMAIN_DEVICE_KHR;
        infos[1].sType = VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_KHR;
        infos[1].timeDomain = m_hostDomain;
        u64 best[2] = {0, 0};
        u64 bestDev = ~0ull;
        for (int i = 0; i < 8; ++i) {
            u64 ts[2] = {0, 0};
            u64 dev = 0;
            if (m_fn.getCalibratedTimestamps(m_device, 2, infos, ts, &dev) != VK_SUCCESS) continue;
            if (dev < bestDev) {
                bestDev = dev;
                best[0] = ts[0];
                best[1] = ts[1];
            }
        }
        if (bestDev == ~0ull) return c;

        // Host domain → steady_clock: tightest of several bracketed reads.
        u64 window = ~0ull;
        i64 offset = 0;
        for (int i = 0; i < 16; ++i) {
            const u64 a = nowNs();
            const u64 h = hostDomainToNs(m_hostDomain, readHostDomain(m_hostDomain));
            const u64 b = nowNs();
            if (b - a < window) {
                window = b - a;
                offset = static_cast<i64>(a + (b - a) / 2) - static_cast<i64>(h);
            }
        }
        m_refGpu = best[0] & m_timestampMask;
        m_refSteadyNs = static_cast<i64>(hostDomainToNs(m_hostDomain, best[1])) + offset;
        c.valid = true;
        c.maxDeviationNs = static_cast<f64>(bestDev) + static_cast<f64>(window) / 2.0;
        return c;
    }

    // ------------------------------------------------------------------ audit

    u64 researchCopyCount() const override { return m_researchCopies; }
    u64 errorCount() const override { return m_errorCount.load(std::memory_order_relaxed); }

private:
    static constexpr u32 kFramesInFlight = 2;
    static constexpr u32 kSegmentSlots = 16;

    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore acquire = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation alloc = VK_NULL_HANDLE;
        void* mapped = nullptr;
        MemoryClass memory = MemoryClass::DeviceLocal;
        bool research = false;
        u64 size = 0;
    };
    struct Pipeline {
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        u32 bufferCount = 0;
        u32 pushBytes = 0;
    };
    struct TsPool {
        VkQueryPool pool = VK_NULL_HANDLE;
        u32 count = 0;
        std::vector<u8> written;
    };
    struct SegmentSlot {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        SegmentSync sync{};
        u64 completionValue = 0;   // value of m_segTimeline signalled when this slot completes
        bool recording = false;
    };
    // Functions not guaranteed to be exported by every loader/ICD (1.2/1.3 core, extensions).
    struct Fns {
        PFN_vkQueueSubmit2 queueSubmit2 = nullptr;
        PFN_vkCmdPipelineBarrier2 cmdPipelineBarrier2 = nullptr;
        PFN_vkCmdWriteTimestamp2 cmdWriteTimestamp2 = nullptr;
        PFN_vkWaitSemaphores waitSemaphores = nullptr;
        PFN_vkSignalSemaphore signalSemaphore = nullptr;
        PFN_vkGetSemaphoreCounterValue getSemaphoreCounterValue = nullptr;
        PFN_vkResetQueryPool resetQueryPool = nullptr;
        PFN_vkGetCalibratedTimestampsKHR getCalibratedTimestamps = nullptr;
    };

    template <class T> static u32 addSlot(std::vector<T>& v, const T& item) {
        v.push_back(item);
        return static_cast<u32>(v.size());
    }
    template <class T> static T* slot(std::vector<T>& v, u32 id) { return (id == 0 || id > v.size()) ? nullptr : &v[id - 1]; }

    Buffer* buffer(BufferHandle h) {
        Buffer* b = slot(m_buffers, h.id);
        return (b != nullptr && b->buffer != VK_NULL_HANDLE) ? b : nullptr;
    }
    Pipeline* pipeline(PipelineHandle h) { return slot(m_pipelines, h.id); }
    VkSemaphore timeline(TimelineHandle h) {
        VkSemaphore* s = slot(m_timelines, h.id);
        return s != nullptr ? *s : VK_NULL_HANDLE;
    }
    TsPool* tsPool(TimestampPoolHandle h) { return slot(m_tsPools, h.id); }
    SegmentSlot* segment(SegmentHandle h) { return (h.id == 0 || h.id > kSegmentSlots) ? nullptr : &m_segments[h.id - 1]; }

    u64 gpuToSteady(u64 ticks) const {
        const i64 dTicks = static_cast<i64>(ticks - m_refGpu);
        return static_cast<u64>(m_refSteadyNs + static_cast<i64>(static_cast<f64>(dTicks) * m_caps.gpuTimestampPeriodNs));
    }

    void writeTimestamp(VkCommandBuffer cmd, TimestampPoolHandle h, u32 index, VkPipelineStageFlags2 stage) {
        if (index == kNoTimestamp) return;
        TsPool* p = tsPool(h);
        if (p == nullptr || index >= p->count) return;
        m_fn.cmdWriteTimestamp2(cmd, stage, p->pool, index);
        p->written[index] = 1;
    }

    static void accessToVk(u32 access, bool src, VkPipelineStageFlags2& stages, VkAccessFlags2& accessMask) {
        stages = 0;
        accessMask = 0;
        if ((access & (kAccessComputeRead | kAccessComputeWrite)) != 0) stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        if ((access & (kAccessCopyRead | kAccessCopyWrite)) != 0) stages |= VK_PIPELINE_STAGE_2_COPY_BIT;
        if ((access & (kAccessHostRead | kAccessHostWrite)) != 0) stages |= VK_PIPELINE_STAGE_2_HOST_BIT;
        // Source scopes only need the writes; reads need just the execution dependency.
        if ((access & kAccessComputeWrite) != 0) accessMask |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
        if ((access & kAccessCopyWrite) != 0) accessMask |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
        if ((access & kAccessHostWrite) != 0) accessMask |= VK_ACCESS_2_HOST_WRITE_BIT;
        if (!src) {
            if ((access & kAccessComputeRead) != 0) accessMask |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            if ((access & kAccessCopyRead) != 0) accessMask |= VK_ACCESS_2_TRANSFER_READ_BIT;
            if ((access & kAccessHostRead) != 0) accessMask |= VK_ACCESS_2_HOST_READ_BIT;
        }
    }

    bool waitSegTimeline(u64 value) {
        VkSemaphoreWaitInfo wi{};
        wi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wi.semaphoreCount = 1;
        wi.pSemaphores = &m_segTimeline;
        wi.pValues = &value;
        const VkResult r = m_fn.waitSemaphores(m_device, &wi, 10'000'000'000ull);
        if (r != VK_SUCCESS) {
            LB_LOG_ERROR("waiting for GPU segment completion failed: %d", static_cast<int>(r));
            m_errorCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    bool createTimelineSemaphore(u64 initial, VkSemaphore& out) {
        VkSemaphoreTypeCreateInfo ti{};
        ti.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        ti.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        ti.initialValue = initial;
        VkSemaphoreCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        si.pNext = &ti;
        LB_VK_CHECK(vkCreateSemaphore(m_device, &si, nullptr, &out));
        return true;
    }

    static bool readSpirv(const std::string& path, std::vector<u32>& out) {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (f == nullptr) {
            LB_LOG_ERROR("cannot open %s", path.c_str());
            return false;
        }
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (size <= 0 || (size % 4) != 0) {
            std::fclose(f);
            LB_LOG_ERROR("%s is not a SPIR-V binary", path.c_str());
            return false;
        }
        out.resize(static_cast<usize>(size) / 4);
        const usize read = std::fread(out.data(), 1, static_cast<usize>(size), f);
        std::fclose(f);
        return read == static_cast<usize>(size);
    }

    // Records which Vulkan loader/ICD library is actually in use (SDK loader, vulkan-1.dll,
    // or MoltenVK directly) - part of the run's system description.
    void logLoaderPath() {
        const void* fn = m_headless ? reinterpret_cast<const void*>(&vkGetInstanceProcAddr)
                                    : reinterpret_cast<const void*>(SDL_Vulkan_GetVkGetInstanceProcAddr());
#if defined(_WIN32)
        HMODULE module = nullptr;
        char path[MAX_PATH] = {};
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(fn), &module) != 0 &&
            GetModuleFileNameA(module, path, MAX_PATH) != 0) {
            m_loaderPath = path;
        }
#else
        Dl_info info{};
        if (fn != nullptr && dladdr(fn, &info) != 0 && info.dli_fname != nullptr) m_loaderPath = info.dli_fname;
#endif
        if (!m_loaderPath.empty()) LB_LOG_INFO("Vulkan loader: %s", m_loaderPath.c_str());
    }

    bool createInstance() {
        u32 instVersion = VK_API_VERSION_1_0;
        vkEnumerateInstanceVersion(&instVersion);
        if (instVersion < VK_API_VERSION_1_3) {
            LB_LOG_ERROR("Vulkan instance version %u.%u < 1.3", VK_API_VERSION_MAJOR(instVersion),
                         VK_API_VERSION_MINOR(instVersion));
            return false;
        }

        u32 extCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> available(extCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, available.data());

        std::vector<const char*> exts;
        if (!m_headless) {
            Uint32 sdlCount = 0;
            const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlCount);
            if (sdlExts == nullptr) {
                LB_LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
                return false;
            }
            exts.assign(sdlExts, sdlExts + sdlCount);
        }

        VkInstanceCreateFlags flags = 0;
        if (hasExtension(available, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
        bool debugUtils = false;
        if (m_validation && hasExtension(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            debugUtils = true;
        }

        std::vector<const char*> layers;
        if (m_validation) {
            u32 layerCount = 0;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> layerProps(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());
            for (const auto& lp : layerProps) {
                if (std::strcmp(lp.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                    layers.push_back("VK_LAYER_KHRONOS_validation");
                }
            }
            if (layers.empty()) {
                LB_LOG_WARN("Validation requested but VK_LAYER_KHRONOS_validation is not installed");
            } else {
                LB_LOG_INFO("Vulkan validation layer enabled (VK_LAYER_KHRONOS_validation)");
            }
        }

        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "Lightbound";
        app.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        app.pEngineName = "Lightbound";
        app.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        app.apiVersion = VK_API_VERSION_1_3;

        VkDebugUtilsMessengerCreateInfoEXT dbg{};
        dbg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbg.pfnUserCallback = debugCallback;
        dbg.pUserData = &m_errorCount;

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.flags = flags;
        ci.pApplicationInfo = &app;
        ci.enabledExtensionCount = static_cast<u32>(exts.size());
        ci.ppEnabledExtensionNames = exts.data();
        ci.enabledLayerCount = static_cast<u32>(layers.size());
        ci.ppEnabledLayerNames = layers.data();
        if (debugUtils) ci.pNext = &dbg;
        LB_VK_CHECK(vkCreateInstance(&ci, nullptr, &m_instance));

        if (debugUtils) {
            auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
            if (createFn != nullptr) createFn(m_instance, &dbg, nullptr, &m_messenger);
        }
        return true;
    }

    bool pickPhysicalDevice() {
        u32 count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        if (count == 0) {
            LB_LOG_ERROR("No Vulkan physical devices");
            return false;
        }
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        int bestScore = -1;
        for (VkPhysicalDevice pd : devices) {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(pd, &props);
            if (props.apiVersion < VK_API_VERSION_1_3) continue;

            u32 qCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, nullptr);
            std::vector<VkQueueFamilyProperties> qProps(qCount);
            vkGetPhysicalDeviceQueueFamilyProperties(pd, &qCount, qProps.data());
            u32 family = UINT32_MAX;
            u32 validBits = 0;
            for (u32 i = 0; i < qCount; ++i) {
                const bool graphics = (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                const bool compute = (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
                const bool present = m_headless || SDL_Vulkan_GetPresentationSupport(m_instance, pd, i);
                if (graphics && compute && present) {
                    family = i;
                    validBits = qProps[i].timestampValidBits;
                    break;
                }
            }
            if (family == UINT32_MAX) continue;

            int score = 1;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 100;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 50;
            if (score > bestScore) {
                bestScore = score;
                m_physical = pd;
                m_queueFamily = family;
                m_props = props;
                m_timestampValidBits = validBits;
            }
        }
        if (m_physical == VK_NULL_HANDLE) {
            LB_LOG_ERROR("No suitable Vulkan device (need Vulkan 1.3 and a graphics+compute(+present) queue)");
            return false;
        }
        m_timestampMask = m_timestampValidBits >= 64 ? ~0ull : ((1ull << m_timestampValidBits) - 1ull);

        VkPhysicalDeviceDriverProperties driver{};
        driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        VkPhysicalDeviceProperties2 p2{};
        p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        p2.pNext = &driver;
        vkGetPhysicalDeviceProperties2(m_physical, &p2);

        m_caps.backend = Backend::Vulkan;
        std::strncpy(m_caps.deviceName, m_props.deviceName, sizeof(m_caps.deviceName) - 1);
        std::snprintf(m_caps.apiVersion, sizeof(m_caps.apiVersion), "Vulkan %u.%u.%u",
                      VK_API_VERSION_MAJOR(m_props.apiVersion), VK_API_VERSION_MINOR(m_props.apiVersion),
                      VK_API_VERSION_PATCH(m_props.apiVersion));
        std::snprintf(m_caps.driverInfo, sizeof(m_caps.driverInfo), "%s %s (0x%08x)", driver.driverName,
                      driver.driverInfo, m_props.driverVersion);
        m_caps.gpuTimestampPeriodNs = static_cast<f64>(m_props.limits.timestampPeriod);
        m_caps.gpuTimestamps = m_timestampValidBits > 0 && m_props.limits.timestampPeriod > 0.0f;

        vkGetPhysicalDeviceMemoryProperties(m_physical, &m_memProps);
        bool allHeapsDeviceLocal = true;
        for (u32 h = 0; h < m_memProps.memoryHeapCount; ++h) {
            if ((m_memProps.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0) allHeapsDeviceLocal = false;
        }
        m_caps.unifiedMemory = (m_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) || allHeapsDeviceLocal;
        for (u32 i = 0; i < m_memProps.memoryTypeCount; ++i) {
            const VkMemoryPropertyFlags fl = m_memProps.memoryTypes[i].propertyFlags;
            const VkDeviceSize heapSize = m_memProps.memoryHeaps[m_memProps.memoryTypes[i].heapIndex].size;
            const bool deviceLocal = (fl & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
            const bool hostVisible = (fl & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
            const bool hostCached = (fl & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;
            if (hostVisible && hostCached) m_caps.hostCachedMemory = true;
            if (deviceLocal && hostVisible) {
                m_caps.deviceLocalHostVisibleHeapBytes = std::max<u64>(m_caps.deviceLocalHostVisibleHeapBytes, heapSize);
            }
        }
        // ReBAR: a DEVICE_LOCAL|HOST_VISIBLE heap larger than the legacy 256 MB BAR window.
        m_caps.rebar = !m_caps.unifiedMemory && m_caps.deviceLocalHostVisibleHeapBytes > (256ull << 20);
        return true;
    }

    bool createDevice() {
        u32 extCount = 0;
        vkEnumerateDeviceExtensionProperties(m_physical, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> available(extCount);
        vkEnumerateDeviceExtensionProperties(m_physical, nullptr, &extCount, available.data());

        std::vector<const char*> exts;
        if (!m_headless) exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        if (hasExtension(available, "VK_KHR_portability_subset")) exts.push_back("VK_KHR_portability_subset");
        if (hasExtension(available, VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)) {
            exts.push_back(VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
            m_calibratedExt = 1;
        } else if (hasExtension(available, VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME)) {
            exts.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
            m_calibratedExt = 2;
        }

        VkPhysicalDeviceVulkan13Features avail13{};
        avail13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceVulkan12Features avail12{};
        avail12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        avail12.pNext = &avail13;
        VkPhysicalDeviceFeatures2 avail2{};
        avail2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        avail2.pNext = &avail12;
        vkGetPhysicalDeviceFeatures2(m_physical, &avail2);

        if (avail12.timelineSemaphore == VK_FALSE || avail12.hostQueryReset == VK_FALSE ||
            avail13.synchronization2 == VK_FALSE) {
            LB_LOG_ERROR("Device lacks timelineSemaphore / hostQueryReset / synchronization2 (required, docs/05 §4.1)");
            return false;
        }
        m_caps.timelineSync = true;

        VkPhysicalDeviceVulkan13Features want13{};
        want13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        want13.synchronization2 = VK_TRUE;
        want13.dynamicRendering = avail13.dynamicRendering;
        VkPhysicalDeviceVulkan12Features want12{};
        want12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        want12.timelineSemaphore = VK_TRUE;
        want12.hostQueryReset = VK_TRUE;
        want12.pNext = &want13;
        VkPhysicalDeviceFeatures2 want2{};
        want2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        want2.pNext = &want12;

        const float priority = 1.0f;
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = m_queueFamily;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;

        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.pNext = &want2;
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = static_cast<u32>(exts.size());
        dci.ppEnabledExtensionNames = exts.data();
        LB_VK_CHECK(vkCreateDevice(m_physical, &dci, nullptr, &m_device));
        vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
        return true;
    }

    bool loadFunctions() {
        auto get = [&](const char* name) { return vkGetDeviceProcAddr(m_device, name); };
        m_fn.queueSubmit2 = reinterpret_cast<PFN_vkQueueSubmit2>(get("vkQueueSubmit2"));
        m_fn.cmdPipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(get("vkCmdPipelineBarrier2"));
        m_fn.cmdWriteTimestamp2 = reinterpret_cast<PFN_vkCmdWriteTimestamp2>(get("vkCmdWriteTimestamp2"));
        m_fn.waitSemaphores = reinterpret_cast<PFN_vkWaitSemaphores>(get("vkWaitSemaphores"));
        m_fn.signalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(get("vkSignalSemaphore"));
        m_fn.getSemaphoreCounterValue = reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(get("vkGetSemaphoreCounterValue"));
        m_fn.resetQueryPool = reinterpret_cast<PFN_vkResetQueryPool>(get("vkResetQueryPool"));
        if (m_calibratedExt == 1) {
            m_fn.getCalibratedTimestamps = reinterpret_cast<PFN_vkGetCalibratedTimestampsKHR>(get("vkGetCalibratedTimestampsKHR"));
        } else if (m_calibratedExt == 2) {
            m_fn.getCalibratedTimestamps = reinterpret_cast<PFN_vkGetCalibratedTimestampsKHR>(get("vkGetCalibratedTimestampsEXT"));
        }
        if (m_fn.queueSubmit2 == nullptr || m_fn.cmdPipelineBarrier2 == nullptr || m_fn.cmdWriteTimestamp2 == nullptr ||
            m_fn.waitSemaphores == nullptr || m_fn.signalSemaphore == nullptr ||
            m_fn.getSemaphoreCounterValue == nullptr || m_fn.resetQueryPool == nullptr) {
            LB_LOG_ERROR("Missing Vulkan 1.2/1.3 core entry points");
            return false;
        }
        return true;
    }

    bool createAllocator() {
        VmaVulkanFunctions fns{};
        fns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        fns.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        VmaAllocatorCreateInfo ci{};
        ci.physicalDevice = m_physical;
        ci.device = m_device;
        ci.instance = m_instance;
        ci.pVulkanFunctions = &fns;
        ci.vulkanApiVersion = VK_API_VERSION_1_3;
        LB_VK_CHECK(vmaCreateAllocator(&ci, &m_allocator));
        return true;
    }

    // Maps each MemoryClass to exactly one memory type (docs/05 §4.2), or marks it unsupported.
    i32 findType(VkMemoryPropertyFlags required, VkMemoryPropertyFlags forbidden) const {
        for (u32 i = 0; i < m_memProps.memoryTypeCount; ++i) {
            const VkMemoryPropertyFlags f = m_memProps.memoryTypes[i].propertyFlags;
            if ((f & required) == required && (f & forbidden) == 0) return static_cast<i32>(i);
        }
        return -1;
    }

    void describeMemoryClasses() {
        const VkMemoryPropertyFlags DL = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        const VkMemoryPropertyFlags HV = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        const VkMemoryPropertyFlags HCOH = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const VkMemoryPropertyFlags HCA = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        // On unified memory every heap is device-local, so "system memory" cannot exclude it.
        const VkMemoryPropertyFlags notDeviceLocal = m_caps.unifiedMemory ? 0 : DL;

        i32 dl = findType(DL, HV);
        if (dl < 0) dl = findType(DL, 0);
        m_classType[static_cast<u32>(MemoryClass::DeviceLocal)] = dl;
        m_classType[static_cast<u32>(MemoryClass::Shared)] = -1; // Metal-only class
        m_classType[static_cast<u32>(MemoryClass::HostVisibleCached)] = findType(HV | HCA, notDeviceLocal);
        i32 coherent = findType(HV | HCOH, HCA | notDeviceLocal);
        // Unified-memory devices (MoltenVK) may expose only cached host-visible memory; there is
        // no uncached system memory to prefer, so accept the coherent (cached) type.
        if (coherent < 0 && m_caps.unifiedMemory) coherent = findType(HV | HCOH, 0);
        m_classType[static_cast<u32>(MemoryClass::HostVisibleCoherent)] = coherent;
        m_classType[static_cast<u32>(MemoryClass::DeviceLocalHostVisible)] = findType(DL | HV, 0);

        for (u32 c = 0; c < kMemoryClassCount; ++c) {
            const i32 t = m_classType[c];
            m_caps.memoryClassSupported[c] = t >= 0;
            if (t < 0) {
                std::snprintf(m_caps.memoryClassInfo[c], sizeof(m_caps.memoryClassInfo[c]), "unsupported");
                continue;
            }
            const VkMemoryType& mt = m_memProps.memoryTypes[t];
            std::snprintf(m_caps.memoryClassInfo[c], sizeof(m_caps.memoryClassInfo[c]), "type %d %s heap %llu MB", t,
                          memoryFlagsString(mt.propertyFlags).c_str(),
                          static_cast<unsigned long long>(m_memProps.memoryHeaps[mt.heapIndex].size >> 20));
            LB_LOG_INFO("memory class %-22s -> %s", memoryClassName(static_cast<MemoryClass>(c)),
                        m_caps.memoryClassInfo[c]);
        }
    }

    bool createSegmentSlots() {
        for (SegmentSlot& s : m_segments) {
            VkCommandPoolCreateInfo pci{};
            pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pci.queueFamilyIndex = m_queueFamily;
            LB_VK_CHECK(vkCreateCommandPool(m_device, &pci, nullptr, &s.pool));
            VkCommandBufferAllocateInfo cai{};
            cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cai.commandPool = s.pool;
            cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cai.commandBufferCount = 1;
            LB_VK_CHECK(vkAllocateCommandBuffers(m_device, &cai, &s.cmd));
        }
        if (!createTimelineSemaphore(0, m_segTimeline)) return false;

        const VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024}};
        VkDescriptorPoolCreateInfo dpi{};
        dpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dpi.maxSets = 256;
        dpi.poolSizeCount = 1;
        dpi.pPoolSizes = sizes;
        LB_VK_CHECK(vkCreateDescriptorPool(m_device, &dpi, nullptr, &m_descriptorPool));
        return true;
    }

    void chooseTimeDomain() {
        const char* name = "none";
        if (m_fn.getCalibratedTimestamps != nullptr) {
            auto getDomains = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsKHR>(vkGetInstanceProcAddr(
                m_instance, m_calibratedExt == 1 ? "vkGetPhysicalDeviceCalibrateableTimeDomainsKHR"
                                                 : "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"));
            u32 n = 0;
            if (getDomains != nullptr) getDomains(m_physical, &n, nullptr);
            std::vector<VkTimeDomainKHR> domains(n);
            if (n > 0) getDomains(m_physical, &n, domains.data());
            auto has = [&](VkTimeDomainKHR d) { return std::find(domains.begin(), domains.end(), d) != domains.end(); };
#if defined(_WIN32)
            const VkTimeDomainKHR prefs[] = {VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_KHR};
            const char* names[] = {"QueryPerformanceCounter"};
#else
            const VkTimeDomainKHR prefs[] = {VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_KHR, VK_TIME_DOMAIN_CLOCK_MONOTONIC_KHR};
            const char* names[] = {"CLOCK_MONOTONIC_RAW", "CLOCK_MONOTONIC"};
#endif
            for (usize i = 0; i < sizeof(prefs) / sizeof(prefs[0]); ++i) {
                if (has(VK_TIME_DOMAIN_DEVICE_KHR) && has(prefs[i])) {
                    m_hostDomain = prefs[i];
                    m_caps.clockCalibration = true;
                    name = names[i];
                    break;
                }
            }
        }
        if (!m_caps.clockCalibration) {
            LB_LOG_WARN("no usable calibrated-timestamp time domain: g2c/c2g latencies will not be reported");
        }
        std::snprintf(m_caps.hostTimeDomain, sizeof(m_caps.hostTimeDomain), "%s", name);
    }

    bool createSwapchain() {
        VkSurfaceCapabilitiesKHR sc{};
        LB_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical, m_surface, &sc));

        u32 fmtCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &fmtCount, formats.data());
        VkSurfaceFormatKHR chosen = formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM,
                                                                          VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                                                    : formats[0];
        for (const auto& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen = f;
                break;
            }
        }
        m_format = chosen.format;

        u32 pmCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical, m_surface, &pmCount, nullptr);
        std::vector<VkPresentModeKHR> modes(pmCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical, m_surface, &pmCount, modes.data());
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!m_vsync) {
            if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end()) {
                presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            } else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            } else {
                LB_LOG_WARN("vsync off requested but only FIFO present mode is available");
            }
        }

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(m_window, &w, &h);
        VkExtent2D extent = sc.currentExtent;
        if (extent.width == UINT32_MAX) {
            extent.width = std::clamp(static_cast<u32>(w), sc.minImageExtent.width, sc.maxImageExtent.width);
            extent.height = std::clamp(static_cast<u32>(h), sc.minImageExtent.height, sc.maxImageExtent.height);
        }
        if (extent.width == 0 || extent.height == 0) {
            m_needRecreate = true; // minimised; try again later
            return true;
        }
        m_extent = extent;

        u32 imageCount = std::max(3u, sc.minImageCount);
        if (sc.maxImageCount > 0) imageCount = std::min(imageCount, sc.maxImageCount);

        VkSwapchainCreateInfoKHR ci{};
        ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface = m_surface;
        ci.minImageCount = imageCount;
        ci.imageFormat = chosen.format;
        ci.imageColorSpace = chosen.colorSpace;
        ci.imageExtent = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform = sc.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = presentMode;
        ci.clipped = VK_TRUE;
        LB_VK_CHECK(vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain));

        u32 count = 0;
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, nullptr);
        m_images.resize(count);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, &count, m_images.data());

        m_renderFinished.resize(count, VK_NULL_HANDLE);
        VkSemaphoreCreateInfo semCi{};
        semCi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (u32 i = 0; i < count; ++i) {
            LB_VK_CHECK(vkCreateSemaphore(m_device, &semCi, nullptr, &m_renderFinished[i]));
        }

        m_caps.backbufferWidth = extent.width;
        m_caps.backbufferHeight = extent.height;
        m_needRecreate = false;
        LB_LOG_TRACE("Swapchain %ux%u, %u images, present mode %d", extent.width, extent.height, count,
                     static_cast<int>(presentMode));
        return true;
    }

    void destroySwapchain() {
        if (m_device == VK_NULL_HANDLE) return;
        for (VkSemaphore s : m_renderFinished) {
            if (s != VK_NULL_HANDLE) vkDestroySemaphore(m_device, s, nullptr);
        }
        m_renderFinished.clear();
        m_images.clear();
        if (m_swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }
    }

    bool recreateSwapchain() {
        vkDeviceWaitIdle(m_device);
        destroySwapchain();
        return createSwapchain() && m_swapchain != VK_NULL_HANDLE;
    }

    bool createFrames() {
        for (Frame& f : m_frames) {
            VkCommandPoolCreateInfo pci{};
            pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pci.queueFamilyIndex = m_queueFamily;
            LB_VK_CHECK(vkCreateCommandPool(m_device, &pci, nullptr, &f.pool));

            VkCommandBufferAllocateInfo cai{};
            cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cai.commandPool = f.pool;
            cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cai.commandBufferCount = 1;
            LB_VK_CHECK(vkAllocateCommandBuffers(m_device, &cai, &f.cmd));

            VkSemaphoreCreateInfo semCi{};
            semCi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            LB_VK_CHECK(vkCreateSemaphore(m_device, &semCi, nullptr, &f.acquire));

            VkFenceCreateInfo fci{};
            fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            LB_VK_CHECK(vkCreateFence(m_device, &fci, nullptr, &f.fence));
        }
        return true;
    }

    void destroyFrames() {
        if (m_device == VK_NULL_HANDLE) return;
        for (Frame& f : m_frames) {
            if (f.fence != VK_NULL_HANDLE) vkDestroyFence(m_device, f.fence, nullptr);
            if (f.acquire != VK_NULL_HANDLE) vkDestroySemaphore(m_device, f.acquire, nullptr);
            if (f.pool != VK_NULL_HANDLE) vkDestroyCommandPool(m_device, f.pool, nullptr);
            f = Frame{};
        }
    }

    SDL_Window* m_window = nullptr;
    bool m_headless = false;
    bool m_validation = false;
    bool m_vsync = true;
    bool m_libraryLoaded = false;
    bool m_needRecreate = false;
    bool m_inFrame = false;
    std::string m_shaderDir;
    std::string m_loaderPath;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_props{};
    VkPhysicalDeviceMemoryProperties m_memProps{};
    u32 m_queueFamily = 0;
    u32 m_timestampValidBits = 64;
    u64 m_timestampMask = ~0ull;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
    Fns m_fn{};
    int m_calibratedExt = 0; // 0 none, 1 KHR, 2 EXT

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    std::vector<VkImage> m_images;
    std::vector<VkSemaphore> m_renderFinished;
    Frame m_frames[kFramesInFlight];
    u32 m_frameIndex = 0;
    u32 m_imageIndex = 0;

    i32 m_classType[kMemoryClassCount] = {-1, -1, -1, -1, -1};
    std::vector<Buffer> m_buffers;
    std::vector<Pipeline> m_pipelines;
    std::vector<VkDescriptorSet> m_bindingSets;
    std::vector<VkSemaphore> m_timelines;
    std::vector<TsPool> m_tsPools;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    SegmentSlot m_segments[kSegmentSlots];
    u32 m_nextSegment = 0;
    VkSemaphore m_segTimeline = VK_NULL_HANDLE;
    u64 m_segCounter = 0;
    u64 m_researchCopies = 0;

    VkTimeDomainKHR m_hostDomain = VK_TIME_DOMAIN_DEVICE_KHR;
    u64 m_refGpu = 0;
    i64 m_refSteadyNs = 0;

    GfxCaps m_caps{};
    std::atomic<u64> m_errorCount{0};
};

} // namespace

std::unique_ptr<Gfx> createVulkanGfx() { return std::make_unique<VulkanGfx>(); }

} // namespace lb::gfx
