// ============================================================================
// src/rhi/webgpu/WebGPUCommandBuffer.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUCommandBuffer.h"
#include "metagfx/rhi/webgpu/WebGPUPipeline.h"
#include "metagfx/rhi/webgpu/WebGPUBuffer.h"
#include "metagfx/rhi/webgpu/WebGPUTexture.h"
#include "metagfx/rhi/webgpu/WebGPUDescriptorSet.h"
#include "metagfx/core/Logger.h"

#include <cstring>

namespace metagfx {
namespace rhi {

WebGPUCommandBuffer::WebGPUCommandBuffer(WebGPUContext& context)
    : m_Context(context) {

    // Create push constant GPU buffer (small uniform buffer for push constants)
    wgpu::BufferDescriptor bufferDesc{};
    bufferDesc.label = "Push Constants Buffer";
    bufferDesc.size = MAX_PUSH_CONSTANT_SIZE;
    bufferDesc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    bufferDesc.mappedAtCreation = false;

    m_PushConstantGPUBuffer = m_Context.device.CreateBuffer(&bufferDesc);
    if (!m_PushConstantGPUBuffer) {
        METAGFX_ERROR << "Failed to create push constant buffer";
    }
}

WebGPUCommandBuffer::~WebGPUCommandBuffer() {
    m_PushConstantGPUBuffer = nullptr;
    m_RenderPassEncoder = nullptr;
    m_CommandEncoder = nullptr;
    m_CommandBuffer = nullptr;
}

void WebGPUCommandBuffer::Begin() {
    wgpu::CommandEncoderDescriptor encoderDesc{};
    encoderDesc.label = "Command Encoder";

    m_CommandEncoder = m_Context.device.CreateCommandEncoder(&encoderDesc);
    if (!m_CommandEncoder) {
        METAGFX_ERROR << "Failed to create command encoder";
    }
}

void WebGPUCommandBuffer::End() {
    // End any active render pass
    if (m_RenderPassEncoder) {
        m_RenderPassEncoder.End();
        m_RenderPassEncoder = nullptr;
    }

    // Finish encoding and get command buffer
    wgpu::CommandBufferDescriptor cmdBufferDesc{};
    cmdBufferDesc.label = "Command Buffer";

    m_CommandBuffer = m_CommandEncoder.Finish(&cmdBufferDesc);
    m_CommandEncoder = nullptr;

    if (!m_CommandBuffer) {
        METAGFX_ERROR << "Failed to finish command buffer";
    }
}

void WebGPUCommandBuffer::BeginRendering(const std::vector<Ref<Texture>>& colorAttachments,
                                          Ref<Texture> depthAttachment,
                                          const std::vector<ClearValue>& clearValues) {
    wgpu::RenderPassDescriptor passDesc{};
    passDesc.label = "Render Pass";

    // Color attachments
    std::vector<wgpu::RenderPassColorAttachment> colorAttachDescs;
    for (size_t i = 0; i < colorAttachments.size(); ++i) {
        wgpu::RenderPassColorAttachment colorAttach{};

        if (colorAttachments[i]) {
            auto webgpuTexture = static_cast<WebGPUTexture*>(colorAttachments[i].get());
            colorAttach.view = webgpuTexture->GetView();

            static bool loggedView = false;
            if (!loggedView) {
                METAGFX_INFO << "WebGPU color attachment view: " << colorAttach.view.Get();
                loggedView = true;
            }
        }

        colorAttach.loadOp = wgpu::LoadOp::Clear;
        colorAttach.storeOp = wgpu::StoreOp::Store;

        // Set clear color from clearValues if available
        if (i < clearValues.size()) {
            colorAttach.clearValue.r = clearValues[i].color[0];
            colorAttach.clearValue.g = clearValues[i].color[1];
            colorAttach.clearValue.b = clearValues[i].color[2];
            colorAttach.clearValue.a = clearValues[i].color[3];

            static bool loggedClear = false;
            if (!loggedClear) {
                METAGFX_INFO << "WebGPU BeginRendering clear color: ("
                              << colorAttach.clearValue.r << ", "
                              << colorAttach.clearValue.g << ", "
                              << colorAttach.clearValue.b << ", "
                              << colorAttach.clearValue.a << ")";
                loggedClear = true;
            }
        } else {
            colorAttach.clearValue = {0.0, 0.0, 0.0, 1.0};
        }

        colorAttachDescs.push_back(colorAttach);
    }

    // Set color attachments (nullptr for depth-only passes)
    if (colorAttachDescs.empty()) {
        passDesc.colorAttachmentCount = 0;
        passDesc.colorAttachments = nullptr;
    } else {
        passDesc.colorAttachmentCount = colorAttachDescs.size();
        passDesc.colorAttachments = colorAttachDescs.data();
    }

    // Depth attachment
    wgpu::RenderPassDepthStencilAttachment depthAttachDesc{};
    if (depthAttachment) {
        auto webgpuTexture = static_cast<WebGPUTexture*>(depthAttachment.get());
        depthAttachDesc.view = webgpuTexture->GetView();
        depthAttachDesc.depthLoadOp = wgpu::LoadOp::Clear;
        depthAttachDesc.depthStoreOp = wgpu::StoreOp::Store;

        // Use clear value from last entry if available
        double clearDepth = 1.0;
        if (!clearValues.empty()) {
            clearDepth = clearValues.back().depthStencil.depth;
        }
        depthAttachDesc.depthClearValue = static_cast<float>(clearDepth);

        passDesc.depthStencilAttachment = &depthAttachDesc;
    }

    m_RenderPassEncoder = m_CommandEncoder.BeginRenderPass(&passDesc);

    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "Failed to begin render pass";
    }

    // Reset bind group validity for new render pass
    m_HasValidBindGroup = false;
}

void WebGPUCommandBuffer::EndRendering() {
    if (m_RenderPassEncoder) {
        m_RenderPassEncoder.End();
        m_RenderPassEncoder = nullptr;
    }
}

void WebGPUCommandBuffer::BindPipeline(Ref<Pipeline> pipeline) {
    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "BindPipeline called without active render pass";
        return;
    }

    m_BoundPipeline = pipeline;
    auto webgpuPipeline = static_cast<WebGPUPipeline*>(pipeline.get());
    m_RenderPassEncoder.SetPipeline(webgpuPipeline->GetRenderPipeline());
}

void WebGPUCommandBuffer::SetViewport(const Viewport& viewport) {
    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "SetViewport called without active render pass";
        return;
    }

    m_RenderPassEncoder.SetViewport(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height,
        viewport.minDepth,
        viewport.maxDepth
    );
}

void WebGPUCommandBuffer::SetScissor(const Rect2D& scissor) {
    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "SetScissor called without active render pass";
        return;
    }

    m_RenderPassEncoder.SetScissorRect(
        scissor.x,
        scissor.y,
        scissor.width,
        scissor.height
    );
}

void WebGPUCommandBuffer::BindVertexBuffer(Ref<Buffer> buffer, uint64 offset) {
    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "BindVertexBuffer called without active render pass";
        return;
    }

    auto webgpuBuffer = static_cast<WebGPUBuffer*>(buffer.get());
    m_RenderPassEncoder.SetVertexBuffer(0, webgpuBuffer->GetHandle(), offset, buffer->GetSize() - offset);
}

void WebGPUCommandBuffer::BindIndexBuffer(Ref<Buffer> buffer, uint64 offset) {
    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "BindIndexBuffer called without active render pass";
        return;
    }

    m_BoundIndexBuffer = buffer;
    m_IndexBufferOffset = offset;

    // Assume uint32 index format (most common)
    // TODO: Detect format from buffer or pipeline configuration
    m_IndexFormat = wgpu::IndexFormat::Uint32;

    auto webgpuBuffer = static_cast<WebGPUBuffer*>(buffer.get());
    m_RenderPassEncoder.SetIndexBuffer(
        webgpuBuffer->GetHandle(),
        m_IndexFormat,
        offset,
        buffer->GetSize() - offset
    );
}

void WebGPUCommandBuffer::Draw(uint32 vertexCount, uint32 instanceCount,
                                uint32 firstVertex, uint32 firstInstance) {
    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "Draw called without active render pass";
        return;
    }

    if (!m_HasValidBindGroup) {
        METAGFX_WARN << "Skipping draw call - no valid bind group bound (resources may not be set)";
        return;
    }

    // Flush push constants before draw
    FlushPushConstants();

    m_RenderPassEncoder.Draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void WebGPUCommandBuffer::DrawIndexed(uint32 indexCount, uint32 instanceCount,
                                       uint32 firstIndex, int32 vertexOffset,
                                       uint32 firstInstance) {
    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "DrawIndexed called without active render pass";
        return;
    }

    if (!m_HasValidBindGroup) {
        METAGFX_WARN << "Skipping draw call - no valid bind group bound (resources may not be set)";
        return;
    }

    // Flush push constants before draw
    FlushPushConstants();

    m_RenderPassEncoder.DrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void WebGPUCommandBuffer::CopyBuffer(Ref<Buffer> src, Ref<Buffer> dst,
                                      uint64 size, uint64 srcOffset, uint64 dstOffset) {
    if (m_RenderPassEncoder) {
        METAGFX_ERROR << "CopyBuffer called during active render pass";
        return;
    }

    auto webgpuSrc = static_cast<WebGPUBuffer*>(src.get());
    auto webgpuDst = static_cast<WebGPUBuffer*>(dst.get());

    m_CommandEncoder.CopyBufferToBuffer(
        webgpuSrc->GetHandle(),
        srcOffset,
        webgpuDst->GetHandle(),
        dstOffset,
        size
    );
}

void WebGPUCommandBuffer::BindDescriptorSet(Ref<Pipeline> pipeline, Ref<DescriptorSet> descriptorSet,
                                              uint32 frameIndex) {
    if (!m_RenderPassEncoder) {
        METAGFX_ERROR << "BindDescriptorSet called without active render pass";
        return;
    }

    if (!descriptorSet) {
        METAGFX_ERROR << "BindDescriptorSet called with null descriptor set";
        return;
    }

    // Cast to WebGPU descriptor set to get bind group
    auto webgpuDescriptorSet = std::static_pointer_cast<WebGPUDescriptorSet>(descriptorSet);

    // If bind group is null, create it now (lazy initialization)
    wgpu::BindGroup bindGroup = webgpuDescriptorSet->GetBindGroup();
    if (!bindGroup) {
        webgpuDescriptorSet->Update();
        bindGroup = webgpuDescriptorSet->GetBindGroup();

        if (!bindGroup) {
            METAGFX_ERROR << "Failed to create bind group - resources may not be set";
            m_HasValidBindGroup = false;
            return;
        }
    }

    // Bind the bind group to the render pass encoder
    // Group index 0 (first parameter) - all our bindings use group 0
    m_RenderPassEncoder.SetBindGroup(0, bindGroup, 0, nullptr);
    m_HasValidBindGroup = true;
}

void WebGPUCommandBuffer::PushConstants(Ref<Pipeline> pipeline, ShaderStage stages,
                                         uint32 offset, uint32 size, const void* data) {
    // Accumulate push constant data in staging buffer
    if (offset + size > MAX_PUSH_CONSTANT_SIZE) {
        METAGFX_ERROR << "Push constant size exceeds maximum: " << (offset + size) << " > " << MAX_PUSH_CONSTANT_SIZE;
        return;
    }

    std::memcpy(m_PushConstantBuffer + offset, data, size);
    m_PushConstantSize = std::max(m_PushConstantSize, offset + size);
    m_PushConstantStages = static_cast<ShaderStage>(static_cast<int>(m_PushConstantStages) | static_cast<int>(stages));
}

void WebGPUCommandBuffer::FlushPushConstants() {
    if (!m_RenderPassEncoder || m_PushConstantSize == 0) {
        return;
    }

    // NOTE: WebGPU doesn't have native push constants.
    // The application layer handles this by creating uniform buffers and binding them
    // via descriptor sets (e.g., m_ModelPushConstantBuffer at binding 14).
    // The PushConstants() calls from the application write to m_PushConstantBuffer,
    // but this is not automatically uploaded/bound in WebGPU.
    //
    // For now, we do nothing here since the application is managing the push constant
    // buffers directly via CopyData() and BindDescriptorSet() for WebGPU.
    // This is a known limitation that will be addressed in future refactoring.

    // Reset for next draw call
    m_PushConstantSize = 0;
    m_PushConstantStages = static_cast<ShaderStage>(0);
}

void WebGPUCommandBuffer::BufferMemoryBarrier(Ref<Buffer> buffer) {
    // WebGPU handles synchronization automatically
    // No explicit barrier needed (similar to Metal)
}

} // namespace rhi
} // namespace metagfx
