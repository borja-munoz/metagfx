// ============================================================================
// include/metagfx/rhi/metal/MetalSampler.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Sampler.h"
#include "MetalTypes.h"

namespace metagfx {
namespace rhi {

class MetalSampler : public Sampler {
public:
    MetalSampler(MetalContext& context, const SamplerDesc& desc);
    ~MetalSampler() override;

    // Metal-specific
    MTL::SamplerState* GetHandle() const { return m_Sampler; }

private:
    MetalContext& m_Context;
    MTL::SamplerState* m_Sampler = nullptr;
};

} // namespace rhi
} // namespace metagfx
