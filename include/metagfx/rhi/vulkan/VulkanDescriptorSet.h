// ============================================================================
// include/metagfx/rhi/vulkan/VulkanDescriptorSet.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/DescriptorSet.h"
#include "VulkanTypes.h"
#include <vector>

namespace metagfx {
namespace rhi {

// Forward declarations
class Buffer;
class Texture;
class Sampler;

// Legacy Vulkan-specific binding struct (for backward compatibility)
struct DescriptorBinding {
    uint32 binding;
    VkDescriptorType type;
    VkShaderStageFlags stageFlags;

    // Only one of these should be active based on descriptor type
    Ref<Buffer> buffer;    // For uniform/storage buffers
    Ref<Texture> texture;  // For combined image samplers
    Ref<Sampler> sampler;  // For combined image samplers
};

class VulkanDescriptorSet : public DescriptorSet {
public:
    // Constructor from backend-agnostic descriptor set description
    VulkanDescriptorSet(VulkanContext& context, const DescriptorSetDesc& desc);

    // Legacy constructor for backward compatibility
    VulkanDescriptorSet(VulkanContext& context, const std::vector<DescriptorBinding>& bindings);

    ~VulkanDescriptorSet() override;

    // DescriptorSet interface implementation
    void UpdateBuffer(uint32 binding, Ref<Buffer> buffer) override;
    void UpdateTexture(uint32 binding, Ref<Texture> texture, Ref<Sampler> sampler) override;
    void* GetNativeHandle(uint32 frameIndex) const override;
    void* GetNativeLayout() const override;

    // Vulkan-specific accessors (for internal use)
    VkDescriptorSetLayout GetLayout() const { return m_Layout; }
    VkDescriptorSet GetSet(uint32 frameIndex) const { return m_DescriptorSets[frameIndex]; }

private:
    void CreateLayout(const std::vector<DescriptorBinding>& bindings);
    void AllocateSets();
    void UpdateSets(const std::vector<DescriptorBinding>& bindings);

    // Convert backend-agnostic types to Vulkan types
    static VkDescriptorType ToVulkanDescriptorType(DescriptorType type);
    static VkShaderStageFlags ToVulkanShaderStage(ShaderStage stage);

    VulkanContext& m_Context;
    VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
    VkDescriptorPool m_Pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    std::vector<DescriptorBinding> m_Bindings;

    static constexpr uint32 MAX_FRAMES = 2;
};

} // namespace rhi
} // namespace metagfx
