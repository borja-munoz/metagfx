// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUSampler.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Sampler.h"
#include "WebGPUTypes.h"

namespace metagfx {
namespace rhi {

class WebGPUSampler : public Sampler {
public:
    WebGPUSampler(WebGPUContext& context, const SamplerDesc& desc);
    ~WebGPUSampler() override;

    // WebGPU-specific
    wgpu::Sampler GetHandle() const { return m_Sampler; }

private:
    WebGPUContext& m_Context;
    wgpu::Sampler m_Sampler = nullptr;
};

} // namespace rhi
} // namespace metagfx
