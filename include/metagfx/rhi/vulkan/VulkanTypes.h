// ============================================================================
// include/metagfx/rhi/vulkan/VulkanTypes.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"  
#include <vulkan/vulkan.h>
#include <vector>

namespace metagfx {
namespace rhi {

// Helper to check Vulkan results
#define VK_CHECK(call) \
    do { \
        VkResult result = call; \
        if (result != VK_SUCCESS) { \
            METAGFX_ERROR << "Vulkan error: " << result << " at " << __FILE__ << ":" << __LINE__; \
        } \
    } while(0)

// Vulkan context shared across all Vulkan objects
struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32 graphicsQueueFamily = 0;
    uint32 presentQueueFamily = 0;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    VkPhysicalDeviceMemoryProperties memoryProperties;
};

// Format conversion utilities
VkFormat ToVulkanFormat(Format format);
Format FromVulkanFormat(VkFormat format);

VkBufferUsageFlags ToVulkanBufferUsage(BufferUsage usage);
VkMemoryPropertyFlags ToVulkanMemoryUsage(MemoryUsage usage);

VkShaderStageFlagBits ToVulkanShaderStage(ShaderStage stage);
VkPrimitiveTopology ToVulkanTopology(PrimitiveTopology topology);
VkPolygonMode ToVulkanPolygonMode(PolygonMode mode);
VkCullModeFlags ToVulkanCullMode(CullMode mode);
VkFrontFace ToVulkanFrontFace(FrontFace face);
VkCompareOp ToVulkanCompareOp(CompareOp op);

} // namespace rhi
} // namespace metagfx
