// ============================================================================
// src/rhi/metal/MetalPipeline.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalPipeline.h"
#include "metagfx/rhi/metal/MetalShader.h"

namespace metagfx {
namespace rhi {

MetalPipeline::MetalPipeline(MetalContext& context, const PipelineDesc& desc)
    : m_Context(context) {

    // Store rasterization state
    m_PrimitiveType = ToMetalPrimitiveType(desc.topology);
    m_CullMode = ToMetalCullMode(desc.rasterization.cullMode);
    m_FrontFace = ToMetalFrontFace(desc.rasterization.frontFace);
    m_FillMode = ToMetalPolygonMode(desc.rasterization.polygonMode);

    // Create render pipeline descriptor
    MTL::RenderPipelineDescriptor* pipelineDesc = MTL::RenderPipelineDescriptor::alloc()->init();

    // Set shaders
    if (desc.vertexShader) {
        auto metalShader = static_cast<MetalShader*>(desc.vertexShader.get());
        pipelineDesc->setVertexFunction(metalShader->GetFunction());
    }

    if (desc.fragmentShader) {
        auto metalShader = static_cast<MetalShader*>(desc.fragmentShader.get());
        pipelineDesc->setFragmentFunction(metalShader->GetFunction());
    }

    // Set vertex descriptor using vertexInputState (preferred) or vertexInput (legacy)
    const auto& attributes = !desc.vertexInputState.attributes.empty()
        ? desc.vertexInputState.attributes
        : desc.vertexInput.attributes;
    const auto& bindings = desc.vertexInputState.bindings;

    // DEBUG: Log vertex descriptor setup
    static int pipelineCount = 0;
    pipelineCount++;
    bool logThis = (pipelineCount <= 5);
    if (logThis) {
        METAGFX_INFO << "MetalPipeline " << pipelineCount << ": " << attributes.size() << " vertex attributes, stride=" << desc.vertexInput.stride;
    }

    if (!attributes.empty()) {
        MTL::VertexDescriptor* vertexDesc = MTL::VertexDescriptor::alloc()->init();

        for (size_t i = 0; i < attributes.size(); ++i) {
            const auto& attr = attributes[i];
            if (logThis) {
                METAGFX_INFO << "  Attr " << i << ": location=" << attr.location
                             << ", format=" << static_cast<int>(attr.format)
                             << ", offset=" << attr.offset
                             << ", binding=" << attr.binding;
            }
            vertexDesc->attributes()->object(attr.location)->setFormat(ToMetalVertexFormat(attr.format));
            vertexDesc->attributes()->object(attr.location)->setOffset(attr.offset);
            vertexDesc->attributes()->object(attr.location)->setBufferIndex(attr.binding);
        }

        if (!bindings.empty()) {
            for (size_t i = 0; i < bindings.size(); ++i) {
                const auto& binding = bindings[i];
                if (logThis) {
                    METAGFX_INFO << "  Layout binding " << binding.binding << ": stride=" << binding.stride;
                }
                vertexDesc->layouts()->object(binding.binding)->setStride(binding.stride);
                vertexDesc->layouts()->object(binding.binding)->setStepFunction(
                    binding.inputRate == VertexInputRate::Vertex
                        ? MTL::VertexStepFunctionPerVertex
                        : MTL::VertexStepFunctionPerInstance
                );
            }
        } else {
            // Use stride from vertexInput if no bindings specified
            if (logThis) {
                METAGFX_INFO << "  Layout 0: stride=" << desc.vertexInput.stride << " (from vertexInput)";
            }
            vertexDesc->layouts()->object(0)->setStride(desc.vertexInput.stride);
            vertexDesc->layouts()->object(0)->setStepFunction(MTL::VertexStepFunctionPerVertex);
        }

        pipelineDesc->setVertexDescriptor(vertexDesc);
        vertexDesc->release();
    }

    // Set color attachments - use BGRA8 as default format (common swap chain format)
    if (desc.colorAttachments.empty()) {
        // Default: one color attachment with BGRA8
        pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    } else {
        for (size_t i = 0; i < desc.colorAttachments.size(); ++i) {
            // ColorAttachmentState doesn't have format, use default
            pipelineDesc->colorAttachments()->object(i)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

            // Set blending if enabled
            if (desc.colorAttachments[i].blendEnable) {
                pipelineDesc->colorAttachments()->object(i)->setBlendingEnabled(true);
                // Default blend mode (src alpha, one minus src alpha)
                pipelineDesc->colorAttachments()->object(i)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
                pipelineDesc->colorAttachments()->object(i)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
                pipelineDesc->colorAttachments()->object(i)->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
                pipelineDesc->colorAttachments()->object(i)->setDestinationAlphaBlendFactor(MTL::BlendFactorZero);
            }
        }
    }

    // Set depth attachment format (use D32 as default depth format)
    if (desc.depthStencil.depthTestEnable || desc.depthStencil.depthWriteEnable) {
        pipelineDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    }

    // Create render pipeline state
    NS::Error* error = nullptr;
    m_RenderPipelineState = m_Context.device->newRenderPipelineState(pipelineDesc, &error);
    pipelineDesc->release();

    if (error || !m_RenderPipelineState) {
        if (error) {
            MTL_LOG_ERROR("Failed to create render pipeline state: " << error->localizedDescription()->utf8String());
            error->release();
        } else {
            MTL_LOG_ERROR("Failed to create render pipeline state");
        }
        return;
    }

    // Create depth stencil state
    if (desc.depthStencil.depthTestEnable || desc.depthStencil.depthWriteEnable) {
        MTL::DepthStencilDescriptor* depthDesc = MTL::DepthStencilDescriptor::alloc()->init();
        depthDesc->setDepthCompareFunction(ToMetalCompareFunction(desc.depthStencil.depthCompareOp));
        depthDesc->setDepthWriteEnabled(desc.depthStencil.depthWriteEnable);

        m_DepthStencilState = m_Context.device->newDepthStencilState(depthDesc);
        depthDesc->release();
    }
}

MetalPipeline::~MetalPipeline() {
    if (m_DepthStencilState) {
        m_DepthStencilState->release();
        m_DepthStencilState = nullptr;
    }
    if (m_RenderPipelineState) {
        m_RenderPipelineState->release();
        m_RenderPipelineState = nullptr;
    }
}

} // namespace rhi
} // namespace metagfx
