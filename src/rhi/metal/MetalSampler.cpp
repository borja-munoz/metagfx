// ============================================================================
// src/rhi/metal/MetalSampler.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalSampler.h"

namespace metagfx {
namespace rhi {

MetalSampler::MetalSampler(MetalContext& context, const SamplerDesc& desc)
    : m_Context(context) {

    MTL::SamplerDescriptor* samplerDesc = MTL::SamplerDescriptor::alloc()->init();

    samplerDesc->setMinFilter(ToMetalFilter(desc.minFilter));
    samplerDesc->setMagFilter(ToMetalFilter(desc.magFilter));
    samplerDesc->setMipFilter(ToMetalMipFilter(desc.mipmapMode));

    samplerDesc->setSAddressMode(ToMetalAddressMode(desc.addressModeU));
    samplerDesc->setTAddressMode(ToMetalAddressMode(desc.addressModeV));
    samplerDesc->setRAddressMode(ToMetalAddressMode(desc.addressModeW));

    samplerDesc->setMaxAnisotropy(static_cast<NS::UInteger>(desc.maxAnisotropy));

    if (desc.enableCompare) {
        samplerDesc->setCompareFunction(ToMetalCompareFunction(desc.compareOp));
    }

    samplerDesc->setLodMinClamp(desc.minLod);
    samplerDesc->setLodMaxClamp(desc.maxLod);

    m_Sampler = m_Context.device->newSamplerState(samplerDesc);
    samplerDesc->release();

    if (!m_Sampler) {
        MTL_LOG_ERROR("Failed to create sampler");
    }
}

MetalSampler::~MetalSampler() {
    if (m_Sampler) {
        m_Sampler->release();
        m_Sampler = nullptr;
    }
}

} // namespace rhi
} // namespace metagfx
