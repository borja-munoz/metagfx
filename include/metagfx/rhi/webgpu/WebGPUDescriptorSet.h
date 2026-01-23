// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUDescriptorSet.h
// ============================================================================
#pragma once

#include "metagfx/rhi/DescriptorSet.h"
#include "WebGPUTypes.h"
#include <vector>

namespace metagfx {
namespace rhi {

class WebGPUDescriptorSet : public DescriptorSet {
public:
    WebGPUDescriptorSet(WebGPUContext& context, const DescriptorSetDesc& desc);
    ~WebGPUDescriptorSet() override;

    void UpdateBuffer(uint32 binding, Ref<Buffer> buffer) override;
    void UpdateTexture(uint32 binding, Ref<Texture> texture, Ref<Sampler> sampler) override;

    void* GetNativeHandle(uint32 frameIndex) const override;
    void* GetNativeLayout() const override;

    // WebGPU-specific
    wgpu::BindGroup GetBindGroup() const { return m_BindGroup; }
    wgpu::BindGroupLayout GetBindGroupLayout() const { return m_BindGroupLayout; }

    // Rebuild the bind group after updates
    void Update();

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
};

} // namespace rhi
} // namespace metagfx
