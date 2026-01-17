// ============================================================================
// include/metagfx/rhi/metal/MetalDescriptorSet.h
// ============================================================================
#pragma once

#include "metagfx/rhi/DescriptorSet.h"
#include "metagfx/rhi/Sampler.h"
#include "MetalTypes.h"
#include <vector>

namespace metagfx {
namespace rhi {

// Metal doesn't use descriptor sets like Vulkan.
// Resources are bound directly to the encoder using setBuffer, setTexture, etc.
// This class stores the bindings and applies them when rendering.
class MetalDescriptorSet : public DescriptorSet {
public:
    // Constructor from backend-agnostic descriptor set description
    MetalDescriptorSet(MetalContext& context, const DescriptorSetDesc& desc);

    ~MetalDescriptorSet() override;

    // DescriptorSet interface implementation
    void UpdateBuffer(uint32 binding, Ref<Buffer> buffer) override;
    void UpdateTexture(uint32 binding, Ref<Texture> texture, Ref<Sampler> sampler) override;
    void* GetNativeHandle(uint32 frameIndex) const override;
    void* GetNativeLayout() const override;

    // Metal-specific: Apply bindings to a render encoder
    void ApplyToEncoder(MTL::RenderCommandEncoder* encoder, uint32 frameIndex) const;

    // Get bindings for inspection
    const std::vector<DescriptorBindingDesc>& GetBindings() const { return m_Bindings; }

private:
    MetalContext& m_Context;
    std::vector<DescriptorBindingDesc> m_Bindings;
};

} // namespace rhi
} // namespace metagfx
