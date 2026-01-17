// ============================================================================
// src/rhi/metal/MetalCommandBuffer.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalCommandBuffer.h"
#include "metagfx/rhi/metal/MetalPipeline.h"
#include "metagfx/rhi/metal/MetalBuffer.h"
#include "metagfx/rhi/metal/MetalTexture.h"
#include "metagfx/rhi/metal/MetalDescriptorSet.h"
#include "metagfx/rhi/DescriptorSet.h"

#include <cstring>

namespace metagfx {
namespace rhi {

MetalCommandBuffer::MetalCommandBuffer(MetalContext& context)
    : m_Context(context) {
}

MetalCommandBuffer::~MetalCommandBuffer() {
    // Command buffer is released after commit
}

void MetalCommandBuffer::Begin() {
    m_CommandBuffer = m_Context.commandQueue->commandBuffer();
    if (!m_CommandBuffer) {
        MTL_LOG_ERROR("Failed to create command buffer");
    }
}

void MetalCommandBuffer::End() {
    // End any active encoders
    if (m_RenderEncoder) {
        m_RenderEncoder->endEncoding();
        m_RenderEncoder = nullptr;
    }
    if (m_BlitEncoder) {
        m_BlitEncoder->endEncoding();
        m_BlitEncoder = nullptr;
    }
}

void MetalCommandBuffer::BeginRendering(const std::vector<Ref<Texture>>& colorAttachments,
                                         Ref<Texture> depthAttachment,
                                         const std::vector<ClearValue>& clearValues) {
    MTL::RenderPassDescriptor* passDesc = MTL::RenderPassDescriptor::alloc()->init();

    // DEBUG: Log rendering setup
    static int frameCount = 0;
    frameCount++;
    if (frameCount <= 5) {  // Log first 5 frames
        METAGFX_INFO << "Metal BeginRendering frame " << frameCount << ": " << colorAttachments.size() << " color attachments"
                     << ", depth=" << (depthAttachment ? "yes" : "no")
                     << ", clearValues=" << clearValues.size();
        if (!clearValues.empty()) {
            METAGFX_INFO << "  Clear color: R=" << clearValues[0].color[0]
                         << " G=" << clearValues[0].color[1]
                         << " B=" << clearValues[0].color[2]
                         << " A=" << clearValues[0].color[3];
        }
    }

    // Set up color attachments
    for (size_t i = 0; i < colorAttachments.size(); ++i) {
        auto* colorAttach = passDesc->colorAttachments()->object(i);

        if (colorAttachments[i]) {
            auto metalTexture = static_cast<MetalTexture*>(colorAttachments[i].get());
            // DEBUG: Log texture info for first 5 frames
            if (frameCount <= 5) {
                MTL::Texture* texHandle = metalTexture->GetHandle();
                METAGFX_INFO << "  Color attachment " << i << ": texture="
                             << (texHandle ? "valid" : "NULL")
                             << ", size=" << metalTexture->GetWidth() << "x" << metalTexture->GetHeight();
                if (texHandle) {
                    METAGFX_INFO << "    Texture ptr=" << (void*)texHandle
                                 << ", pixelFormat=" << static_cast<int>(texHandle->pixelFormat())
                                 << ", usage=" << static_cast<int>(texHandle->usage())
                                 << ", storageMode=" << static_cast<int>(texHandle->storageMode());
                }
            }
            colorAttach->setTexture(metalTexture->GetHandle());
        }

        colorAttach->setLoadAction(MTL::LoadActionClear);
        colorAttach->setStoreAction(MTL::StoreActionStore);

        // Set clear color from clearValues if available
        if (i < clearValues.size()) {
            colorAttach->setClearColor(MTL::ClearColor::Make(
                clearValues[i].color[0],
                clearValues[i].color[1],
                clearValues[i].color[2],
                clearValues[i].color[3]
            ));
        } else {
            colorAttach->setClearColor(MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));
        }
    }

    // Set up depth attachment
    if (depthAttachment) {
        auto metalTexture = static_cast<MetalTexture*>(depthAttachment.get());
        auto* depthAttach = passDesc->depthAttachment();
        depthAttach->setTexture(metalTexture->GetHandle());
        depthAttach->setLoadAction(MTL::LoadActionClear);
        depthAttach->setStoreAction(MTL::StoreActionStore);

        // Use clear value from last entry if available (depth clear value convention)
        double clearDepth = 1.0;
        if (!clearValues.empty()) {
            clearDepth = clearValues.back().depthStencil.depth;
        }
        depthAttach->setClearDepth(clearDepth);

        // DEBUG: Log depth attachment info for first 5 frames
        if (frameCount <= 5) {
            MTL::Texture* depthTex = metalTexture->GetHandle();
            METAGFX_INFO << "  Depth attachment: texture="
                         << (depthTex ? "valid" : "NULL")
                         << ", size=" << metalTexture->GetWidth() << "x" << metalTexture->GetHeight()
                         << ", clearDepth=" << clearDepth;
            if (depthTex) {
                METAGFX_INFO << "    Depth pixelFormat=" << static_cast<int>(depthTex->pixelFormat())
                             << ", usage=" << static_cast<int>(depthTex->usage())
                             << ", storageMode=" << static_cast<int>(depthTex->storageMode());
            }
        }
    }

    m_RenderEncoder = m_CommandBuffer->renderCommandEncoder(passDesc);
    passDesc->release();

    if (!m_RenderEncoder) {
        MTL_LOG_ERROR("Failed to create render command encoder");
    } else {
        static bool loggedEncoder = false;
        if (!loggedEncoder) {
            METAGFX_INFO << "Metal render encoder created successfully";
            loggedEncoder = true;
        }
    }
}

void MetalCommandBuffer::EndRendering() {
    if (m_RenderEncoder) {
        m_RenderEncoder->endEncoding();
        m_RenderEncoder = nullptr;
    }
}

void MetalCommandBuffer::BindPipeline(Ref<Pipeline> pipeline) {
    m_BoundPipeline = pipeline;

    // Reset push constant staging when binding new pipeline
    m_PushConstantSize = 0;
    m_PushConstantStages = static_cast<ShaderStage>(0);
    memset(m_PushConstantBuffer, 0, sizeof(m_PushConstantBuffer));

    auto metalPipeline = static_cast<MetalPipeline*>(pipeline.get());

    if (m_RenderEncoder) {
        m_RenderEncoder->setRenderPipelineState(metalPipeline->GetRenderPipelineState());

        // DEBUG: Log depth state binding
        static int bindCount = 0;
        bindCount++;
        if (bindCount <= 10) {
            METAGFX_INFO << "BindPipeline " << bindCount << ": depthStencilState="
                         << (metalPipeline->GetDepthStencilState() ? "valid" : "NULL");
        }

        if (metalPipeline->GetDepthStencilState()) {
            m_RenderEncoder->setDepthStencilState(metalPipeline->GetDepthStencilState());
        }

        m_RenderEncoder->setCullMode(metalPipeline->GetCullMode());
        m_RenderEncoder->setFrontFacingWinding(metalPipeline->GetFrontFace());
        m_RenderEncoder->setTriangleFillMode(metalPipeline->GetFillMode());
    }
}

void MetalCommandBuffer::BindVertexBuffer(Ref<Buffer> buffer, uint64 offset) {
    if (m_RenderEncoder) {
        auto metalBuffer = static_cast<MetalBuffer*>(buffer.get());
        m_RenderEncoder->setVertexBuffer(metalBuffer->GetHandle(), offset, 0);
    }
}

void MetalCommandBuffer::BindIndexBuffer(Ref<Buffer> buffer, uint64 offset) {
    // Metal handles index buffer binding at draw time
    // Store the buffer for use in DrawIndexed
    m_BoundIndexBuffer = buffer;
    m_IndexBufferOffset = offset;
}

void MetalCommandBuffer::SetViewport(const Viewport& viewport) {
    if (m_RenderEncoder) {
        MTL::Viewport mtlViewport;
        mtlViewport.originX = viewport.x;
        mtlViewport.originY = viewport.y;
        mtlViewport.width = viewport.width;
        mtlViewport.height = viewport.height;
        mtlViewport.znear = viewport.minDepth;
        mtlViewport.zfar = viewport.maxDepth;
        m_RenderEncoder->setViewport(mtlViewport);
    }
}

void MetalCommandBuffer::SetScissor(const Rect2D& scissor) {
    if (m_RenderEncoder) {
        MTL::ScissorRect mtlScissor;
        mtlScissor.x = static_cast<NS::UInteger>(scissor.x);
        mtlScissor.y = static_cast<NS::UInteger>(scissor.y);
        mtlScissor.width = static_cast<NS::UInteger>(scissor.width);
        mtlScissor.height = static_cast<NS::UInteger>(scissor.height);
        m_RenderEncoder->setScissorRect(mtlScissor);
    }
}

void MetalCommandBuffer::Draw(uint32 vertexCount, uint32 instanceCount,
                              uint32 firstVertex, uint32 firstInstance) {
    if (m_RenderEncoder && m_BoundPipeline) {
        // Flush accumulated push constants before drawing
        FlushPushConstants();

        auto metalPipeline = static_cast<MetalPipeline*>(m_BoundPipeline.get());
        m_RenderEncoder->drawPrimitives(
            metalPipeline->GetPrimitiveType(),
            firstVertex,
            vertexCount,
            instanceCount,
            firstInstance
        );
    }
}

void MetalCommandBuffer::DrawIndexed(uint32 indexCount, uint32 instanceCount,
                                     uint32 firstIndex, int32 vertexOffset,
                                     uint32 firstInstance) {
    if (m_RenderEncoder && m_BoundPipeline && m_BoundIndexBuffer) {
        // Flush accumulated push constants before drawing
        FlushPushConstants();

        auto metalPipeline = static_cast<MetalPipeline*>(m_BoundPipeline.get());
        auto metalBuffer = static_cast<MetalBuffer*>(m_BoundIndexBuffer.get());

        m_RenderEncoder->drawIndexedPrimitives(
            metalPipeline->GetPrimitiveType(),
            indexCount,
            m_IndexType,
            metalBuffer->GetHandle(),
            m_IndexBufferOffset + firstIndex * (m_IndexType == MTL::IndexTypeUInt32 ? 4 : 2),
            instanceCount,
            vertexOffset,
            firstInstance
        );
    }
}

void MetalCommandBuffer::CopyBuffer(Ref<Buffer> src, Ref<Buffer> dst,
                                    uint64 size, uint64 srcOffset, uint64 dstOffset) {
    // End render encoder if active
    if (m_RenderEncoder) {
        m_RenderEncoder->endEncoding();
        m_RenderEncoder = nullptr;
    }

    // Create blit encoder if not active
    if (!m_BlitEncoder) {
        m_BlitEncoder = m_CommandBuffer->blitCommandEncoder();
    }

    if (m_BlitEncoder) {
        auto metalSrc = static_cast<MetalBuffer*>(src.get());
        auto metalDst = static_cast<MetalBuffer*>(dst.get());

        m_BlitEncoder->copyFromBuffer(
            metalSrc->GetHandle(), srcOffset,
            metalDst->GetHandle(), dstOffset,
            size
        );
    }
}

// Abstract interface implementations
void MetalCommandBuffer::BindDescriptorSet(Ref<Pipeline> pipeline, Ref<DescriptorSet> descriptorSet,
                                           uint32 frameIndex) {
    (void)pipeline;  // Metal doesn't need pipeline layout for binding

    if (!m_RenderEncoder || !descriptorSet) {
        return;
    }

    // Cast to MetalDescriptorSet and apply bindings directly to encoder
    auto metalDescSet = std::static_pointer_cast<MetalDescriptorSet>(descriptorSet);
    metalDescSet->ApplyToEncoder(m_RenderEncoder, frameIndex);
}

void MetalCommandBuffer::PushConstants(Ref<Pipeline> pipeline, ShaderStage stages,
                                       uint32 offset, uint32 size, const void* data) {
    (void)pipeline;  // Metal uses buffer binding index for push constants

    if (!data || size == 0) {
        return;
    }

    // Metal's setBytes replaces the entire buffer content, so we need to accumulate
    // all push constant data and send it all at once before the draw call.
    // Copy data into our staging buffer at the specified offset.

    if (offset + size > MAX_PUSH_CONSTANT_SIZE) {
        MTL_LOG_ERROR("Push constant data exceeds maximum size: offset=" << offset
                      << ", size=" << size << ", max=" << MAX_PUSH_CONSTANT_SIZE);
        return;
    }

    // Copy data to staging buffer
    memcpy(m_PushConstantBuffer + offset, data, size);

    // Track the total size (high water mark)
    if (offset + size > m_PushConstantSize) {
        m_PushConstantSize = offset + size;
    }

    // Accumulate stages that need this data
    m_PushConstantStages = static_cast<ShaderStage>(
        static_cast<int>(m_PushConstantStages) | static_cast<int>(stages)
    );
}

void MetalCommandBuffer::FlushPushConstants() {
    if (!m_RenderEncoder || m_PushConstantSize == 0) {
        return;
    }

    const uint32 pushConstantBufferIndex = 30;

    // Send accumulated push constants to Metal
    if (static_cast<int>(m_PushConstantStages) & static_cast<int>(ShaderStage::Vertex)) {
        m_RenderEncoder->setVertexBytes(m_PushConstantBuffer, m_PushConstantSize, pushConstantBufferIndex);
    }
    if (static_cast<int>(m_PushConstantStages) & static_cast<int>(ShaderStage::Fragment)) {
        m_RenderEncoder->setFragmentBytes(m_PushConstantBuffer, m_PushConstantSize, pushConstantBufferIndex);
    }

    // Don't reset here - the push constants should remain valid for subsequent draws
    // until new data is pushed. Reset happens when pipeline is bound or render pass ends.
}

void MetalCommandBuffer::BufferMemoryBarrier(Ref<Buffer> buffer) {
    // Metal handles memory coherence automatically for managed and shared storage modes
    // For private storage, explicit synchronization would be needed through MTL::BlitCommandEncoder
    // In most cases, this is a no-op for Metal
    (void)buffer;
}

} // namespace rhi
} // namespace metagfx
