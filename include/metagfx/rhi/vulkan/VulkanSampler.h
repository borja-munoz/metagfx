// ============================================================================
// include/metagfx/rhi/vulkan/VulkanSampler.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Sampler.h"
#include "metagfx/rhi/Types.h"
#include "VulkanTypes.h"

namespace metagfx {
namespace rhi {

/**
 * @brief Vulkan implementation of sampler
 *
 * Wraps VkSampler handle and manages its lifecycle.
 */
class VulkanSampler : public Sampler {
public:
    VulkanSampler(VulkanContext& context, const SamplerDesc& desc);
    ~VulkanSampler() override;

    // Vulkan-specific access
    VkSampler GetHandle() const { return m_Sampler; }

private:
    VulkanContext& m_Context;
    VkSampler m_Sampler = VK_NULL_HANDLE;
};

} // namespace rhi
} // namespace metagfx
