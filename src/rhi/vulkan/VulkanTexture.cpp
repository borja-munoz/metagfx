// ============================================================================
// src/rhi/vulkan/VulkanTexture.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"

namespace metagfx {
namespace rhi {

VulkanTexture::VulkanTexture(VulkanContext& context, VkImage image, VkImageView imageView,
                            uint32 width, uint32 height, VkFormat format)
    : m_Context(context), m_Image(image), m_ImageView(imageView),
      m_Width(width), m_Height(height), m_VkFormat(format), m_OwnsImage(false) {
    m_Format = FromVulkanFormat(format);
}

VulkanTexture::VulkanTexture(VulkanContext& context, const TextureDesc& desc)
    : m_Context(context), m_Width(desc.width), m_Height(desc.height),
      m_Format(desc.format), m_OwnsImage(true) {
    
    m_VkFormat = ToVulkanFormat(desc.format);
    
    // TODO: Implement texture creation when needed
    METAGFX_WARN << "Custom texture creation not yet implemented";
}

VulkanTexture::~VulkanTexture() {
    if (m_OwnsImage) {
        if (m_ImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_Context.device, m_ImageView, nullptr);
        }
        
        if (m_Image != VK_NULL_HANDLE) {
            vkDestroyImage(m_Context.device, m_Image, nullptr);
        }
        
        if (m_Memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_Context.device, m_Memory, nullptr);
        }
    }
}

} // namespace rhi
} // namespace metagfx