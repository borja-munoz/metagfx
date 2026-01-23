// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUCommandBuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/CommandBuffer.h"
#include "WebGPUTypes.h"

namespace metagfx {
namespace rhi {

class WebGPUCommandBuffer : public CommandBuffer {
public:
    WebGPUCommandBuffer(WebGPUContext& context);
    ~WebGPUCommandBuffer() override;

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

    void BindDescriptorSet(Ref<Pipeline> pipeline, Ref<DescriptorSet> descriptorSet,
                           uint32 frameIndex = 0) override;
    void PushConstants(Ref<Pipeline> pipeline, ShaderStage stages,
                       uint32 offset, uint32 size, const void* data) override;
    void BufferMemoryBarrier(Ref<Buffer> buffer) override;

    // WebGPU-specific
    wgpu::CommandBuffer GetHandle() const { return m_CommandBuffer; }
    wgpu::RenderPassEncoder GetRenderPassEncoder() const { return m_RenderPassEncoder; }

private:
    WebGPUContext& m_Context;
    wgpu::CommandEncoder m_CommandEncoder = nullptr;
    wgpu::CommandBuffer m_CommandBuffer = nullptr;
    wgpu::RenderPassEncoder m_RenderPassEncoder = nullptr;

    // Current pipeline state
    Ref<Pipeline> m_BoundPipeline;
    Ref<Buffer> m_BoundIndexBuffer;
    uint64 m_IndexBufferOffset = 0;
    wgpu::IndexFormat m_IndexFormat = wgpu::IndexFormat::Uint32;

    // Push constants staging buffer (WebGPU doesn't have native push constants)
    static constexpr uint32 MAX_PUSH_CONSTANT_SIZE = 128;
    uint8 m_PushConstantBuffer[MAX_PUSH_CONSTANT_SIZE] = {};
    uint32 m_PushConstantSize = 0;
    ShaderStage m_PushConstantStages = static_cast<ShaderStage>(0);
    wgpu::Buffer m_PushConstantGPUBuffer = nullptr;

    void FlushPushConstants();
};

} // namespace rhi
} // namespace metagfx
