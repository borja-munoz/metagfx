// ============================================================================
// src/rhi/webgpu/WebGPUSampler.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUSampler.h"
#include "metagfx/core/Logger.h"

namespace metagfx {
namespace rhi {

WebGPUSampler::WebGPUSampler(WebGPUContext& context, const SamplerDesc& desc)
    : m_Context(context) {

    // Create sampler descriptor
    wgpu::SamplerDescriptor samplerDesc{};
    samplerDesc.label = desc.debugName ? desc.debugName : "Sampler";

    // Address modes
    samplerDesc.addressModeU = ToWebGPUAddressMode(desc.addressModeU);
    samplerDesc.addressModeV = ToWebGPUAddressMode(desc.addressModeV);
    samplerDesc.addressModeW = ToWebGPUAddressMode(desc.addressModeW);

    // Filter modes
    samplerDesc.magFilter = ToWebGPUFilterMode(desc.magFilter);
    samplerDesc.minFilter = ToWebGPUFilterMode(desc.minFilter);
    samplerDesc.mipmapFilter = ToWebGPUMipMapFilterMode(desc.mipmapFilter);

    // LOD settings
    samplerDesc.lodMinClamp = desc.minLod;
    samplerDesc.lodMaxClamp = desc.maxLod;

    // Comparison (for shadow sampling)
    if (desc.compareEnable) {
        samplerDesc.compare = ToWebGPUCompareFunction(desc.compareOp);
    } else {
        samplerDesc.compare = wgpu::CompareFunction::Undefined;
    }

    // Anisotropy
    samplerDesc.maxAnisotropy = desc.maxAnisotropy;

    // Create the sampler
    m_Sampler = m_Context.device.CreateSampler(&samplerDesc);

    if (!m_Sampler) {
        WEBGPU_LOG_ERROR("Failed to create sampler");
        throw std::runtime_error("Failed to create WebGPU sampler");
    }
}

WebGPUSampler::~WebGPUSampler() {
    m_Sampler = nullptr;
}

} // namespace rhi
} // namespace metagfx
