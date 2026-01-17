// ============================================================================
// include/metagfx/rhi/metal/MetalCommandBuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/CommandBuffer.h"
#include "MetalTypes.h"

namespace metagfx {
namespace rhi {

class MetalCommandBuffer : public CommandBuffer {
public:
    MetalCommandBuffer(MetalContext& context);
    ~MetalCommandBuffer() override;

    // CommandBuffer interface
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

    // Abstract interface implementations
    void BindDescriptorSet(Ref<Pipeline> pipeline, Ref<DescriptorSet> descriptorSet,
                           uint32 frameIndex = 0) override;
    void PushConstants(Ref<Pipeline> pipeline, ShaderStage stages,
                       uint32 offset, uint32 size, const void* data) override;
    void BufferMemoryBarrier(Ref<Buffer> buffer) override;

    // Metal-specific
    MTL::CommandBuffer* GetHandle() const { return m_CommandBuffer; }
    MTL::RenderCommandEncoder* GetRenderEncoder() const { return m_RenderEncoder; }

private:
    MetalContext& m_Context;
    MTL::CommandBuffer* m_CommandBuffer = nullptr;
    MTL::RenderCommandEncoder* m_RenderEncoder = nullptr;
    MTL::BlitCommandEncoder* m_BlitEncoder = nullptr;

    // Current pipeline state
    Ref<Pipeline> m_BoundPipeline;
    Ref<Buffer> m_BoundIndexBuffer;
    uint64 m_IndexBufferOffset = 0;
    MTL::IndexType m_IndexType = MTL::IndexTypeUInt32;

    // Push constants staging buffer (Metal's setBytes replaces entire buffer,
    // so we accumulate all push constant data before sending)
    static constexpr uint32 MAX_PUSH_CONSTANT_SIZE = 128;  // Typical max push constant size
    uint8 m_PushConstantBuffer[MAX_PUSH_CONSTANT_SIZE] = {};
    uint32 m_PushConstantSize = 0;  // Current size of push constant data
    ShaderStage m_PushConstantStages = static_cast<ShaderStage>(0);  // Which stages need the data

    void FlushPushConstants();  // Send accumulated push constants to Metal
};

} // namespace rhi
} // namespace metagfx
