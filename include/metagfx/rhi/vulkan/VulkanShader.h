// ============================================================================
// include/metagfx/rhi/vulkan/VulkanShader.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Shader.h"
#include "VulkanTypes.h"

namespace metagfx {
namespace rhi {

class VulkanShader : public Shader {
public:
    VulkanShader(VulkanContext& context, const ShaderDesc& desc);
    ~VulkanShader() override;

    ShaderStage GetStage() const override { return m_Stage; }
    
    // Vulkan-specific
    VkShaderModule GetModule() const { return m_Module; }
    const std::string& GetEntryPoint() const { return m_EntryPoint; }

private:
    VulkanContext& m_Context;
    VkShaderModule m_Module = VK_NULL_HANDLE;
    ShaderStage m_Stage;
    std::string m_EntryPoint;
};

} // namespace rhi
} // namespace metagfx

