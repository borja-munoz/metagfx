// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUPipeline.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Pipeline.h"
#include "WebGPUTypes.h"

namespace metagfx {
namespace rhi {

class WebGPUPipeline : public Pipeline {
public:
    WebGPUPipeline(WebGPUContext& context, const PipelineDesc& desc);
    ~WebGPUPipeline() override;

    // WebGPU-specific
    wgpu::RenderPipeline GetRenderPipeline() const { return m_RenderPipeline; }
    wgpu::PipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

    wgpu::PrimitiveTopology GetPrimitiveTopology() const { return m_PrimitiveTopology; }
    wgpu::CullMode GetCullMode() const { return m_CullMode; }
    wgpu::FrontFace GetFrontFace() const { return m_FrontFace; }

private:
    WebGPUContext& m_Context;
    wgpu::RenderPipeline m_RenderPipeline = nullptr;
    wgpu::PipelineLayout m_PipelineLayout = nullptr;

    wgpu::PrimitiveTopology m_PrimitiveTopology = wgpu::PrimitiveTopology::TriangleList;
    wgpu::CullMode m_CullMode = wgpu::CullMode::Back;
    wgpu::FrontFace m_FrontFace = wgpu::FrontFace::CCW;
};

} // namespace rhi
} // namespace metagfx
