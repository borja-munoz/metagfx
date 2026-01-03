// ============================================================================
// src/rhi/vulkan/VulkanCommandBuffer.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanCommandBuffer.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"
#include "metagfx/rhi/vulkan/VulkanBuffer.h"
#include "metagfx/rhi/vulkan/VulkanPipeline.h"

namespace metagfx {
namespace rhi {

VulkanCommandBuffer::VulkanCommandBuffer(VulkanContext& context, VkCommandPool commandPool)
    : m_Context(context), m_CommandPool(commandPool) {
    
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    
    VK_CHECK(vkAllocateCommandBuffers(m_Context.device, &allocInfo, &m_CommandBuffer));
}

VulkanCommandBuffer::~VulkanCommandBuffer() {
    if (m_CurrentFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_Context.device, m_CurrentFramebuffer, nullptr);
    }
    
    if (m_CurrentRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_Context.device, m_CurrentRenderPass, nullptr);
    }
    
    if (m_CommandBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(m_Context.device, m_CommandPool, 1, &m_CommandBuffer);
    }
}

void VulkanCommandBuffer::Begin() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));
    m_IsRecording = true;
}

void VulkanCommandBuffer::End() {
    VK_CHECK(vkEndCommandBuffer(m_CommandBuffer));
    m_IsRecording = false;
}

void VulkanCommandBuffer::BeginRendering(const std::vector<Ref<Texture>>& colorAttachments,
                                         Ref<Texture> depthAttachment,
                                         const std::vector<ClearValue>& clearValues) {

    // For this simple implementation, create a basic render pass
    VkAttachmentDescription colorAttachment{};
    auto vkTexture = std::static_pointer_cast<VulkanTexture>(colorAttachments[0]);
    colorAttachment.format = ToVulkanFormat(vkTexture->GetFormat());
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Setup depth attachment if provided
    VkAttachmentDescription depthAttachmentDesc{};
    VkAttachmentReference depthAttachmentRef{};
    std::vector<VkAttachmentDescription> attachments = { colorAttachment };

    if (depthAttachment) {
        auto vkDepthTexture = std::static_pointer_cast<VulkanTexture>(depthAttachment);
        depthAttachmentDesc.format = ToVulkanFormat(vkDepthTexture->GetFormat());
        depthAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments.push_back(depthAttachmentDesc);

        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    if (depthAttachment) {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    
    VK_CHECK(vkCreateRenderPass(m_Context.device, &renderPassInfo, nullptr, &m_CurrentRenderPass));
    
    // Create framebuffer
    std::vector<VkImageView> framebufferAttachments = { vkTexture->GetImageView() };
    if (depthAttachment) {
        auto vkDepthTexture = std::static_pointer_cast<VulkanTexture>(depthAttachment);
        framebufferAttachments.push_back(vkDepthTexture->GetImageView());
    }

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_CurrentRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(framebufferAttachments.size());
    framebufferInfo.pAttachments = framebufferAttachments.data();
    framebufferInfo.width = vkTexture->GetWidth();
    framebufferInfo.height = vkTexture->GetHeight();
    framebufferInfo.layers = 1;
    
    VK_CHECK(vkCreateFramebuffer(m_Context.device, &framebufferInfo, nullptr, &m_CurrentFramebuffer));
    
    // Begin render pass
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = m_CurrentRenderPass;
    beginInfo.framebuffer = m_CurrentFramebuffer;
    beginInfo.renderArea.offset = { 0, 0 };
    beginInfo.renderArea.extent = { vkTexture->GetWidth(), vkTexture->GetHeight() };

    // Setup clear values
    std::vector<VkClearValue> vkClearValues;
    if (!clearValues.empty()) {
        VkClearValue colorClear{};
        colorClear.color = { clearValues[0].color[0], clearValues[0].color[1],
                            clearValues[0].color[2], clearValues[0].color[3] };
        vkClearValues.push_back(colorClear);

        if (depthAttachment && clearValues.size() > 1) {
            VkClearValue depthClear{};
            depthClear.depthStencil.depth = clearValues[1].depthStencil.depth;
            depthClear.depthStencil.stencil = clearValues[1].depthStencil.stencil;
            vkClearValues.push_back(depthClear);
        }
    }

    beginInfo.clearValueCount = static_cast<uint32_t>(vkClearValues.size());
    beginInfo.pClearValues = vkClearValues.data();

    vkCmdBeginRenderPass(m_CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandBuffer::EndRendering() {
    vkCmdEndRenderPass(m_CommandBuffer);
}

void VulkanCommandBuffer::BindPipeline(Ref<Pipeline> pipeline) {
    auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);
    vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->GetHandle());
}

void VulkanCommandBuffer::SetViewport(const Viewport& viewport) {
    VkViewport vp{};
    vp.x = viewport.x;
    vp.y = viewport.y;
    vp.width = viewport.width;
    vp.height = viewport.height;
    vp.minDepth = viewport.minDepth;
    vp.maxDepth = viewport.maxDepth;
    
    vkCmdSetViewport(m_CommandBuffer, 0, 1, &vp);
}

void VulkanCommandBuffer::SetScissor(const Rect2D& scissor) {
    VkRect2D sc{};
    sc.offset = { scissor.x, scissor.y };
    sc.extent = { scissor.width, scissor.height };
    
    vkCmdSetScissor(m_CommandBuffer, 0, 1, &sc);
}

void VulkanCommandBuffer::BindVertexBuffer(Ref<Buffer> buffer, uint64 offset) {
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    VkBuffer buffers[] = { vkBuffer->GetHandle() };
    VkDeviceSize offsets[] = { offset };
    
    vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, buffers, offsets);
}

void VulkanCommandBuffer::BindIndexBuffer(Ref<Buffer> buffer, uint64 offset) {
    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    vkCmdBindIndexBuffer(m_CommandBuffer, vkBuffer->GetHandle(), offset, VK_INDEX_TYPE_UINT32);
}

void VulkanCommandBuffer::Draw(uint32 vertexCount, uint32 instanceCount,
                              uint32 firstVertex, uint32 firstInstance) {
    vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandBuffer::DrawIndexed(uint32 indexCount, uint32 instanceCount,
                                     uint32 firstIndex, int32 vertexOffset,
                                     uint32 firstInstance) {
    vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VulkanCommandBuffer::CopyBuffer(Ref<Buffer> src, Ref<Buffer> dst,
                                    uint64 size, uint64 srcOffset, uint64 dstOffset) {
    auto vkSrc = std::static_pointer_cast<VulkanBuffer>(src);
    auto vkDst = std::static_pointer_cast<VulkanBuffer>(dst);
    
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = size;
    
    vkCmdCopyBuffer(m_CommandBuffer, vkSrc->GetHandle(), vkDst->GetHandle(), 1, &copyRegion);
}

void VulkanCommandBuffer::BindDescriptorSet(VkPipelineLayout layout, VkDescriptorSet descriptorSet) {
    vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           layout, 0, 1, &descriptorSet, 0, nullptr);
}

void VulkanCommandBuffer::PushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags,
                                       uint32 offset, uint32 size, const void* data) {
    vkCmdPushConstants(m_CommandBuffer, layout, stageFlags, offset, size, data);
}

void VulkanCommandBuffer::BufferMemoryBarrier(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size,
                                             VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                             VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;

    vkCmdPipelineBarrier(
        m_CommandBuffer,
        srcStage,
        dstStage,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );
}

} // namespace rhi
} // namespace metagfx