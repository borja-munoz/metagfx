// ============================================================================
// src/rhi/vulkan/VulkanFramebuffer.cpp
// ============================================================================
#include "metagfx/rhi/vulkan/VulkanFramebuffer.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"
#include "metagfx/rhi/vulkan/VulkanTypes.h"
#include "metagfx/core/Logger.h"
#include <stdexcept>
#include <vector>

namespace metagfx {
namespace rhi {

VulkanFramebuffer::VulkanFramebuffer(VkDevice device, const FramebufferDesc& desc)
    : m_Device(device)
    , m_DepthAttachment(desc.depthAttachment)
    , m_ColorAttachments(desc.colorAttachments) {

    if (!m_DepthAttachment) {
        throw std::runtime_error("Framebuffer requires a depth attachment");
    }

    // Get dimensions from depth attachment
    m_Width = m_DepthAttachment->GetWidth();
    m_Height = m_DepthAttachment->GetHeight();

    CreateRenderPass();
    CreateFramebuffer();

    METAGFX_INFO << "Created Vulkan framebuffer: " << m_Width << "x" << m_Height;
}

VulkanFramebuffer::~VulkanFramebuffer() {
    if (m_Framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_Device, m_Framebuffer, nullptr);
    }
    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    }
}

void VulkanFramebuffer::CreateRenderPass() {
    // Depth-only framebuffer for shadow mapping
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = static_cast<VulkanTexture*>(m_DepthAttachment.get())->GetImage() ?
        VK_FORMAT_D32_SFLOAT : VK_FORMAT_D32_SFLOAT;  // TODO: Get actual format from texture
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pColorAttachments = nullptr;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependencies for layout transitions
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass for framebuffer");
    }
}

void VulkanFramebuffer::CreateFramebuffer() {
    auto* vulkanDepthTexture = static_cast<VulkanTexture*>(m_DepthAttachment.get());
    VkImageView attachments[] = {
        vulkanDepthTexture->GetImageView()
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_RenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_Width;
    framebufferInfo.height = m_Height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create framebuffer");
    }
}

} // namespace rhi
} // namespace metagfx
