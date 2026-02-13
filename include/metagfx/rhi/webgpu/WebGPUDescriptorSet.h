// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUDescriptorSet.h
// ============================================================================
#pragma once

#include "metagfx/rhi/DescriptorSet.h"
#include "WebGPUTypes.h"
#include <vector>
#include <unordered_map>

namespace metagfx {
namespace rhi {

class WebGPUDescriptorSet : public DescriptorSet {
public:
    WebGPUDescriptorSet(WebGPUContext& context, const DescriptorSetDesc& desc);
    ~WebGPUDescriptorSet() override;

    void UpdateBuffer(uint32 binding, Ref<Buffer> buffer) override;
    void UpdateTexture(uint32 binding, Ref<Texture> texture, Ref<Sampler> sampler) override;
    void Update() override;

    void* GetNativeHandle(uint32 frameIndex) const override;
    void* GetNativeLayout() const override;

    // WebGPU-specific
    wgpu::BindGroup GetBindGroup() const { return m_BindGroup; }
    wgpu::BindGroupLayout GetBindGroupLayout() const { return m_BindGroupLayout; }

private:
    WebGPUContext& m_Context;
    wgpu::BindGroup m_BindGroup = nullptr;
    wgpu::BindGroupLayout m_BindGroupLayout = nullptr;

    // Store binding information for updates
    struct BindingInfo {
        uint32 binding;
        DescriptorType type;
        Ref<Buffer> buffer;
        Ref<Texture> texture;
        Ref<Sampler> sampler;
    };
    std::vector<BindingInfo> m_Bindings;

    // Map from texture binding to its corresponding sampler binding
    // Needed because WebGPU separates textures and samplers
    std::unordered_map<uint32, uint32> m_SamplerBindings;
};

} // namespace rhi
} // namespace metagfx
