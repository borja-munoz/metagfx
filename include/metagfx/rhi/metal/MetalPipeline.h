// ============================================================================
// include/metagfx/rhi/metal/MetalPipeline.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Pipeline.h"
#include "MetalTypes.h"

namespace metagfx {
namespace rhi {

class MetalPipeline : public Pipeline {
public:
    MetalPipeline(MetalContext& context, const PipelineDesc& desc);
    ~MetalPipeline() override;

    // Metal-specific
    MTL::RenderPipelineState* GetRenderPipelineState() const { return m_RenderPipelineState; }
    MTL::DepthStencilState* GetDepthStencilState() const { return m_DepthStencilState; }

    MTL::PrimitiveType GetPrimitiveType() const { return m_PrimitiveType; }
    MTL::CullMode GetCullMode() const { return m_CullMode; }
    MTL::Winding GetFrontFace() const { return m_FrontFace; }
    MTL::TriangleFillMode GetFillMode() const { return m_FillMode; }

private:
    MetalContext& m_Context;
    MTL::RenderPipelineState* m_RenderPipelineState = nullptr;
    MTL::DepthStencilState* m_DepthStencilState = nullptr;

    MTL::PrimitiveType m_PrimitiveType = MTL::PrimitiveTypeTriangle;
    MTL::CullMode m_CullMode = MTL::CullModeBack;
    MTL::Winding m_FrontFace = MTL::WindingCounterClockwise;
    MTL::TriangleFillMode m_FillMode = MTL::TriangleFillModeFill;
};

} // namespace rhi
} // namespace metagfx
