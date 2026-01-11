// ============================================================================
// src/rhi/vulkan/VulkanSampler.cpp
// ============================================================================
#include "metagfx/rhi/vulkan/VulkanSampler.h"
#include "metagfx/core/Logger.h"

namespace metagfx {
namespace rhi {

// Helper to convert Filter enum to VkFilter
static VkFilter ToVkFilter(Filter filter) {
    switch (filter) {
        case Filter::Nearest: return VK_FILTER_NEAREST;
        case Filter::Linear:  return VK_FILTER_LINEAR;
    }
    return VK_FILTER_LINEAR;
}

// Helper to convert SamplerAddressMode enum to VkSamplerAddressMode
static VkSamplerAddressMode ToVkSamplerAddressMode(SamplerAddressMode mode) {
    switch (mode) {
        case SamplerAddressMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::ClampToBorder:  return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

// Helper to convert CompareOp enum to VkCompareOp
static VkCompareOp ToVkCompareOp(CompareOp op) {
    switch (op) {
        case CompareOp::Never:          return VK_COMPARE_OP_NEVER;
        case CompareOp::Less:           return VK_COMPARE_OP_LESS;
        case CompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
        case CompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareOp::Greater:        return VK_COMPARE_OP_GREATER;
        case CompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareOp::Always:         return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_ALWAYS;
}

VulkanSampler::VulkanSampler(VulkanContext& context, const SamplerDesc& desc)
    : m_Context(context) {

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = ToVkFilter(desc.magFilter);
    samplerInfo.minFilter = ToVkFilter(desc.minFilter);
    samplerInfo.mipmapMode = desc.mipmapMode == Filter::Linear ?
                             VK_SAMPLER_MIPMAP_MODE_LINEAR :
                             VK_SAMPLER_MIPMAP_MODE_NEAREST;

    samplerInfo.addressModeU = ToVkSamplerAddressMode(desc.addressModeU);
    samplerInfo.addressModeV = ToVkSamplerAddressMode(desc.addressModeV);
    samplerInfo.addressModeW = ToVkSamplerAddressMode(desc.addressModeW);

    samplerInfo.mipLodBias = desc.mipLodBias;
    samplerInfo.minLod = desc.minLod;
    samplerInfo.maxLod = desc.maxLod;

    samplerInfo.anisotropyEnable = desc.anisotropyEnable ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = desc.maxAnisotropy;

    // Comparison sampler for shadow mapping
    samplerInfo.compareEnable = desc.enableCompare ? VK_TRUE : VK_FALSE;
    samplerInfo.compareOp = ToVkCompareOp(desc.compareOp);

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(m_Context.device, &samplerInfo, nullptr, &m_Sampler));

    METAGFX_INFO << "Created Vulkan sampler";
}

VulkanSampler::~VulkanSampler() {
    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_Context.device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
}

} // namespace rhi
} // namespace metagfx
