// ============================================================================
// include/metagfx/rhi/vulkan/VulkanPipeline.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Pipeline.h"
#include "VulkanTypes.h"

namespace metagfx {
namespace rhi {

class VulkanPipeline : public Pipeline {
public:
    VulkanPipeline(VulkanContext& context, const PipelineDesc& desc, 
                   VkRenderPass renderPass);
    ~VulkanPipeline() override;

    // Vulkan-specific
    VkPipeline GetHandle() const { return m_Pipeline; }
    VkPipelineLayout GetLayout() const { return m_Layout; }

private:
    VulkanContext& m_Context;
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_Layout = VK_NULL_HANDLE;
};

} // namespace rhi
} // namespace metagfx

