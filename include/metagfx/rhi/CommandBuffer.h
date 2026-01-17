// ============================================================================
// include/metagfx/rhi/CommandBuffer.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

namespace metagfx {
namespace rhi {

// Forward declarations
class Pipeline;
class DescriptorSet;
class Buffer;
class Texture;

class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;
    
    // Command recording
    virtual void Begin() = 0;
    virtual void End() = 0;
    
    // Render pass commands
    virtual void BeginRendering(const std::vector<Ref<Texture>>& colorAttachments,
                                Ref<Texture> depthAttachment,
                                const std::vector<ClearValue>& clearValues) = 0;
    virtual void EndRendering() = 0;
    
    // Pipeline binding
    virtual void BindPipeline(Ref<Pipeline> pipeline) = 0;
    
    // Viewport and scissor
    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void SetScissor(const Rect2D& scissor) = 0;
    
    // Vertex and index buffers
    virtual void BindVertexBuffer(Ref<Buffer> buffer, uint64 offset = 0) = 0;
    virtual void BindIndexBuffer(Ref<Buffer> buffer, uint64 offset = 0) = 0;
    
    // Draw commands
    virtual void Draw(uint32 vertexCount, uint32 instanceCount = 1,
                     uint32 firstVertex = 0, uint32 firstInstance = 0) = 0;
    virtual void DrawIndexed(uint32 indexCount, uint32 instanceCount = 1,
                            uint32 firstIndex = 0, int32 vertexOffset = 0,
                            uint32 firstInstance = 0) = 0;
    
    // Copy commands
    virtual void CopyBuffer(Ref<Buffer> src, Ref<Buffer> dst,
                           uint64 size, uint64 srcOffset = 0, uint64 dstOffset = 0) = 0;

    // Descriptor set binding
    virtual void BindDescriptorSet(Ref<Pipeline> pipeline, Ref<DescriptorSet> descriptorSet,
                                   uint32 frameIndex = 0) = 0;

    // Push constants (uniform data pushed directly without descriptor sets)
    virtual void PushConstants(Ref<Pipeline> pipeline, ShaderStage stages,
                               uint32 offset, uint32 size, const void* data) = 0;

    // Memory barriers (for synchronization between CPU and GPU)
    virtual void BufferMemoryBarrier(Ref<Buffer> buffer) = 0;

protected:
    CommandBuffer() = default;
};

} // namespace rhi
} // namespace metagfx
