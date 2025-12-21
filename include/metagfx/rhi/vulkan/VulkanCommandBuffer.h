// ============================================================================
// include/metagfx/rhi/vulkan/VulkanCommandBuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/CommandBuffer.h"
#include "VulkanTypes.h"

namespace metagfx {
namespace rhi {

class VulkanCommandBuffer : public CommandBuffer {
public:
    VulkanCommandBuffer(VulkanContext& context, VkCommandPool commandPool);
    ~VulkanCommandBuffer() override;

    void Begin() override;
    void End() override;
    
    void BeginRendering(const std::vector<Ref<Texture>>& colorAttachments,
                       Ref<Texture> depthAttachment,
                       const std::vector<ClearValue>& clearValues) override;
    void EndRendering() override;
    
    void BindPipeline(Ref<Pipeline> pipeline) override;
    void SetViewport(const Viewport& viewport) override;
    void SetScissor(const Rect2D& scissor) override;
    
    void BindVertexBuffer(Ref<Buffer> buffer, uint64 offset = 0) override;
    void BindIndexBuffer(Ref<Buffer> buffer, uint64 offset = 0) override;
    
    void Draw(uint32 vertexCount, uint32 instanceCount = 1,
             uint32 firstVertex = 0, uint32 firstInstance = 0) override;
    void DrawIndexed(uint32 indexCount, uint32 instanceCount = 1,
                    uint32 firstIndex = 0, int32 vertexOffset = 0,
                    uint32 firstInstance = 0) override;
    
    void CopyBuffer(Ref<Buffer> src, Ref<Buffer> dst,
                   uint64 size, uint64 srcOffset = 0, uint64 dstOffset = 0) override;
    
    // Vulkan-specific
    VkCommandBuffer GetHandle() const { return m_CommandBuffer; }

    // Bind descriptor set
    void BindDescriptorSet(VkPipelineLayout layout, VkDescriptorSet descriptorSet);

private:
    VulkanContext& m_Context;
    VkCommandPool m_CommandPool;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
    VkRenderPass m_CurrentRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_CurrentFramebuffer = VK_NULL_HANDLE;
    bool m_IsRecording = false;
};

} // namespace rhi
} // namespace metagfx