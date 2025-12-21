// ============================================================================
// include/metagfx/rhi/vulkan/VulkanDescriptorSet.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "VulkanTypes.h"
#include <vector>

namespace metagfx {
namespace rhi {

// Forward declarations
class VulkanBuffer;

struct DescriptorBinding {
    uint32 binding;
    VkDescriptorType type;
    VkShaderStageFlags stageFlags;
    Ref<Buffer> buffer;  // For uniform/storage buffers
};

class VulkanDescriptorSet {
public:
    VulkanDescriptorSet(VulkanContext& context, const std::vector<DescriptorBinding>& bindings);
    ~VulkanDescriptorSet();

    void UpdateBuffer(uint32 binding, Ref<Buffer> buffer);
    
    VkDescriptorSetLayout GetLayout() const { return m_Layout; }
    VkDescriptorSet GetSet(uint32 frameIndex) const { return m_DescriptorSets[frameIndex]; }

private:
    void CreateLayout(const std::vector<DescriptorBinding>& bindings);
    void AllocateSets();
    void UpdateSets(const std::vector<DescriptorBinding>& bindings);

    VulkanContext& m_Context;
    VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
    VkDescriptorPool m_Pool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    std::vector<DescriptorBinding> m_Bindings;
    
    static constexpr uint32 MAX_FRAMES = 2;
};

} // namespace rhi
} // namespace metagfx

