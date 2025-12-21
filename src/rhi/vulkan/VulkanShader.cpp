// ============================================================================
// src/rhi/vulkan/VulkanShader.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanShader.h"

namespace metagfx {
namespace rhi {

VulkanShader::VulkanShader(VulkanContext& context, const ShaderDesc& desc)
    : m_Context(context), m_Stage(desc.stage), m_EntryPoint(desc.entryPoint) {
    
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = desc.code.size();
    createInfo.pCode = reinterpret_cast<const uint32*>(desc.code.data());
    
    VK_CHECK(vkCreateShaderModule(m_Context.device, &createInfo, nullptr, &m_Module));

    METAGFX_DEBUG << "Vulkan shader created";
}

VulkanShader::~VulkanShader() {
    if (m_Module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_Context.device, m_Module, nullptr);
    }
}

} // namespace rhi
} // namespace metagfx

