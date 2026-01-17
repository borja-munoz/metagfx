// ============================================================================
// src/rhi/vulkan/VulkanDescriptorSet.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanDescriptorSet.h"
#include "metagfx/rhi/vulkan/VulkanBuffer.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"
#include "metagfx/rhi/vulkan/VulkanSampler.h"

namespace metagfx {
namespace rhi {

// Convert backend-agnostic DescriptorType to Vulkan
VkDescriptorType VulkanDescriptorSet::ToVulkanDescriptorType(DescriptorType type) {
    switch (type) {
        case DescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::SampledTexture: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::StorageTexture: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
        default: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

// Convert backend-agnostic ShaderStage to Vulkan
VkShaderStageFlags VulkanDescriptorSet::ToVulkanShaderStage(ShaderStage stage) {
    VkShaderStageFlags flags = 0;
    if (static_cast<int>(stage) & static_cast<int>(ShaderStage::Vertex))
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (static_cast<int>(stage) & static_cast<int>(ShaderStage::Fragment))
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (static_cast<int>(stage) & static_cast<int>(ShaderStage::Compute))
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (static_cast<int>(stage) & static_cast<int>(ShaderStage::Geometry))
        flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (static_cast<int>(stage) & static_cast<int>(ShaderStage::TessellationControl))
        flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (static_cast<int>(stage) & static_cast<int>(ShaderStage::TessellationEvaluation))
        flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    return flags;
}

// Constructor from backend-agnostic descriptor set description
VulkanDescriptorSet::VulkanDescriptorSet(VulkanContext& context, const DescriptorSetDesc& desc)
    : m_Context(context) {

    // Convert backend-agnostic bindings to Vulkan bindings
    for (const auto& binding : desc.bindings) {
        DescriptorBinding vkBinding;
        vkBinding.binding = binding.binding;
        vkBinding.type = ToVulkanDescriptorType(binding.type);
        vkBinding.stageFlags = ToVulkanShaderStage(binding.stageFlags);
        vkBinding.buffer = binding.buffer;
        vkBinding.texture = binding.texture;
        vkBinding.sampler = binding.sampler;
        m_Bindings.push_back(vkBinding);
    }

    CreateLayout(m_Bindings);
    AllocateSets();
    UpdateSets(m_Bindings);
}

// Legacy constructor for backward compatibility
VulkanDescriptorSet::VulkanDescriptorSet(VulkanContext& context,
                                         const std::vector<DescriptorBinding>& bindings)
    : m_Context(context), m_Bindings(bindings) {

    CreateLayout(bindings);
    AllocateSets();
    UpdateSets(bindings);
}

VulkanDescriptorSet::~VulkanDescriptorSet() {
    if (m_Pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_Context.device, m_Pool, nullptr);
    }
    
    if (m_Layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_Context.device, m_Layout, nullptr);
    }
}

void VulkanDescriptorSet::CreateLayout(const std::vector<DescriptorBinding>& bindings) {
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;

    for (const auto& binding : bindings) {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding.binding;
        layoutBinding.descriptorType = binding.type;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = binding.stageFlags;
        layoutBinding.pImmutableSamplers = nullptr;

        layoutBindings.push_back(layoutBinding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32>(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(m_Context.device, &layoutInfo, nullptr, &m_Layout));
}

void VulkanDescriptorSet::AllocateSets() {
    // Create descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes;
    
    for (const auto& binding : m_Bindings) {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = binding.type;
        poolSize.descriptorCount = MAX_FRAMES;
        poolSizes.push_back(poolSize);
    }
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES;
    
    VK_CHECK(vkCreateDescriptorPool(m_Context.device, &poolInfo, nullptr, &m_Pool));
    
    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES, m_Layout);
    
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_Pool;
    allocInfo.descriptorSetCount = MAX_FRAMES;
    allocInfo.pSetLayouts = layouts.data();
    
    m_DescriptorSets.resize(MAX_FRAMES);
    VK_CHECK(vkAllocateDescriptorSets(m_Context.device, &allocInfo, m_DescriptorSets.data()));
}

void VulkanDescriptorSet::UpdateSets(const std::vector<DescriptorBinding>& bindings) {
    for (uint32 i = 0; i < MAX_FRAMES; i++) {
        std::vector<VkWriteDescriptorSet> descriptorWrites;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkDescriptorImageInfo> imageInfos;

        // Reserve space to prevent reallocation (which would invalidate pointers)
        bufferInfos.reserve(bindings.size());
        imageInfos.reserve(bindings.size());
        descriptorWrites.reserve(bindings.size());

        for (const auto& binding : bindings) {
            if (binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                if (binding.buffer) {
                    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(binding.buffer);

                    VkDescriptorBufferInfo bufferInfo{};
                    bufferInfo.buffer = vkBuffer->GetHandle();
                    bufferInfo.offset = 0;
                    bufferInfo.range = vkBuffer->GetSize();
                    bufferInfos.push_back(bufferInfo);

                    VkWriteDescriptorSet descriptorWrite{};
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = m_DescriptorSets[i];
                    descriptorWrite.dstBinding = binding.binding;
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = binding.type;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pBufferInfo = &bufferInfos.back();

                    descriptorWrites.push_back(descriptorWrite);
                }
            } else if (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
                if (binding.texture && binding.sampler) {
                    auto vkTexture = std::static_pointer_cast<VulkanTexture>(binding.texture);
                    auto vkSampler = std::static_pointer_cast<VulkanSampler>(binding.sampler);

                    VkDescriptorImageInfo imageInfo{};
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    imageInfo.imageView = vkTexture->GetImageView();
                    imageInfo.sampler = vkSampler->GetHandle();
                    imageInfos.push_back(imageInfo);

                    VkWriteDescriptorSet descriptorWrite{};
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = m_DescriptorSets[i];
                    descriptorWrite.dstBinding = binding.binding;
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pImageInfo = &imageInfos.back();

                    descriptorWrites.push_back(descriptorWrite);
                }
            }
        }

        vkUpdateDescriptorSets(m_Context.device,
                              static_cast<uint32>(descriptorWrites.size()),
                              descriptorWrites.data(), 0, nullptr);
    }
}

void VulkanDescriptorSet::UpdateBuffer(uint32 binding, Ref<Buffer> buffer) {
    for (auto& b : m_Bindings) {
        if (b.binding == binding) {
            b.buffer = buffer;
            break;
        }
    }
    UpdateSets(m_Bindings);
}

void VulkanDescriptorSet::UpdateTexture(uint32 binding, Ref<Texture> texture, Ref<Sampler> sampler) {
    for (auto& b : m_Bindings) {
        if (b.binding == binding) {
            b.texture = texture;
            b.sampler = sampler;
            break;
        }
    }
    UpdateSets(m_Bindings);
}

void* VulkanDescriptorSet::GetNativeHandle(uint32 frameIndex) const {
    if (frameIndex < m_DescriptorSets.size()) {
        return (void*)m_DescriptorSets[frameIndex];
    }
    return nullptr;
}

void* VulkanDescriptorSet::GetNativeLayout() const {
    return (void*)m_Layout;
}

} // namespace rhi
} // namespace metagfx