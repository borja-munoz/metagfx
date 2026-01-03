// ============================================================================
// src/rhi/vulkan/VulkanPipeline.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanPipeline.h"
#include "metagfx/rhi/vulkan/VulkanShader.h"

namespace metagfx {
namespace rhi {

VulkanPipeline::VulkanPipeline(VulkanContext& context, const PipelineDesc& desc, 
                               VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout)
    : m_Context(context) {
    
    // Shader stages
    auto vertShader = std::static_pointer_cast<VulkanShader>(desc.vertexShader);
    auto fragShader = std::static_pointer_cast<VulkanShader>(desc.fragmentShader);
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShader->GetModule();
    vertShaderStageInfo.pName = vertShader->GetEntryPoint().c_str();
    
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShader->GetModule();
    fragShaderStageInfo.pName = fragShader->GetEntryPoint().c_str();
    
    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
    
    // Vertex input
    std::vector<VkVertexInputAttributeDescription> attributeDescs;
    for (const auto& attr : desc.vertexInput.attributes) {
        VkVertexInputAttributeDescription attrDesc{};
        attrDesc.location = attr.location;
        attrDesc.binding = 0;
        attrDesc.format = ToVulkanFormat(attr.format);
        attrDesc.offset = attr.offset;
        attributeDescs.push_back(attrDesc);
    }
    
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = desc.vertexInput.stride;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32>(attributeDescs.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescs.data();
    
    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = ToVulkanTopology(desc.topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    
    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = desc.rasterization.depthClampEnable ? VK_TRUE : VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = ToVulkanPolygonMode(desc.rasterization.polygonMode);
    rasterizer.lineWidth = desc.rasterization.lineWidth;
    rasterizer.cullMode = ToVulkanCullMode(desc.rasterization.cullMode);
    rasterizer.frontFace = ToVulkanFrontFace(desc.rasterization.frontFace);
    rasterizer.depthBiasEnable = desc.rasterization.depthBiasEnable ? VK_TRUE : VK_FALSE;
    
    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = ToVulkanCompareOp(desc.depthStencil.depthCompareOp);
    depthStencil.stencilTestEnable = desc.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;
    
    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Dynamic state
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;
    
    // Pipeline layout with push constants for camera position and material flags
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 32;  // vec4 cameraPosition + uint materialFlags + padding (32 bytes)

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (descriptorSetLayout != VK_NULL_HANDLE) {
        METAGFX_INFO << "Pipeline using descriptor set layout: " << descriptorSetLayout;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    } else {
        METAGFX_WARN << "Pipeline created WITHOUT descriptor set layout!";
        pipelineLayoutInfo.setLayoutCount = 0;
    }

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(m_Context.device, &pipelineLayoutInfo, nullptr, &m_Layout));
    
    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_Layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    
    VK_CHECK(vkCreateGraphicsPipelines(m_Context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));
    
    METAGFX_DEBUG << "Vulkan graphics pipeline created";
}

VulkanPipeline::~VulkanPipeline() {
    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Context.device, m_Pipeline, nullptr);
    }
    
    if (m_Layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_Context.device, m_Layout, nullptr);
    }
}

} // namespace rhi
} // namespace metagfx

