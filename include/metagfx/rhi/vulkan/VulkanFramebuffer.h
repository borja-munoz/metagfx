// ============================================================================
// include/metagfx/rhi/vulkan/VulkanFramebuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Framebuffer.h"
#include <vulkan/vulkan.h>

namespace metagfx {
namespace rhi {

class VulkanFramebuffer : public Framebuffer {
public:
    VulkanFramebuffer(VkDevice device, const FramebufferDesc& desc);
    ~VulkanFramebuffer() override;

    // Framebuffer interface
    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Ref<Texture> GetDepthAttachment() const override { return m_DepthAttachment; }
    const std::vector<Ref<Texture>>& GetColorAttachments() const override { return m_ColorAttachments; }

    // Vulkan-specific getters
    VkFramebuffer GetVkFramebuffer() const { return m_Framebuffer; }
    VkRenderPass GetVkRenderPass() const { return m_RenderPass; }

private:
    void CreateRenderPass();
    void CreateFramebuffer();

    VkDevice m_Device = VK_NULL_HANDLE;
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;

    Ref<Texture> m_DepthAttachment;
    std::vector<Ref<Texture>> m_ColorAttachments;

    uint32 m_Width = 0;
    uint32 m_Height = 0;
};

} // namespace rhi
} // namespace metagfx
