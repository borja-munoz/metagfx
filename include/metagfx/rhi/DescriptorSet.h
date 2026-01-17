// ============================================================================
// include/metagfx/rhi/DescriptorSet.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

namespace metagfx {
namespace rhi {

// Forward declarations
class Buffer;
class Texture;
class Sampler;

// Abstract base class for descriptor sets
// Vulkan: Wraps VkDescriptorSet and VkDescriptorSetLayout
// Metal: Manages argument buffer or direct resource bindings
// D3D12: Wraps descriptor tables
class DescriptorSet {
public:
    virtual ~DescriptorSet() = default;

    // Update a buffer binding
    virtual void UpdateBuffer(uint32 binding, Ref<Buffer> buffer) = 0;

    // Update a texture + sampler binding
    virtual void UpdateTexture(uint32 binding, Ref<Texture> texture, Ref<Sampler> sampler) = 0;

    // Get the descriptor set for a specific frame (for double/triple buffering)
    // Returns backend-specific handle that can be used with command buffers
    virtual void* GetNativeHandle(uint32 frameIndex) const = 0;

    // Get the layout/signature for pipeline creation
    virtual void* GetNativeLayout() const = 0;

protected:
    DescriptorSet() = default;
};

} // namespace rhi
} // namespace metagfx
