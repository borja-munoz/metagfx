// ============================================================================
// include/metagfx/rhi/vulkan/VulkanTexture.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Texture.h"
#include "VulkanTypes.h"

namespace metagfx {
namespace rhi {

class VulkanTexture : public Texture {
public:
    // For swap chain images (don't own the image)
    VulkanTexture(VulkanContext& context, VkImage image, VkImageView imageView, 
                  uint32 width, uint32 height, VkFormat format);
    
    // For created textures (own the image)
    VulkanTexture(VulkanContext& context, const TextureDesc& desc);
    
    ~VulkanTexture() override;

    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Format GetFormat() const override { return m_Format; }
    
    // Vulkan-specific
    VkImage GetImage() const { return m_Image; }
    VkImageView GetImageView() const { return m_ImageView; }

private:
    VulkanContext& m_Context;
    VkImage m_Image = VK_NULL_HANDLE;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    
    uint32 m_Width = 0;
    uint32 m_Height = 0;
    Format m_Format = Format::Undefined;
    VkFormat m_VkFormat = VK_FORMAT_UNDEFINED;
    
    bool m_OwnsImage = true;
};

} // namespace rhi
} // namespace metagfx
