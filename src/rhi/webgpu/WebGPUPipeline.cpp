// ============================================================================
// src/rhi/webgpu/WebGPUPipeline.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUPipeline.h"
#include "metagfx/rhi/webgpu/WebGPUShader.h"
#include "metagfx/core/Logger.h"

namespace metagfx {
namespace rhi {

WebGPUPipeline::WebGPUPipeline(WebGPUContext& context, const PipelineDesc& desc)
    : m_Context(context) {

    // Store rasterization state
    m_PrimitiveTopology = ToWebGPUPrimitiveTopology(desc.topology);
    m_CullMode = ToWebGPUCullMode(desc.rasterization.cullMode);
    m_FrontFace = ToWebGPUFrontFace(desc.rasterization.frontFace);

    // Create pipeline layout (for now, use empty layout)
    // TODO: Add bind group layouts from descriptor sets
    wgpu::PipelineLayoutDescriptor layoutDesc{};
    layoutDesc.label = "Pipeline Layout";
    layoutDesc.bindGroupLayoutCount = 0;
    layoutDesc.bindGroupLayouts = nullptr;

    m_PipelineLayout = m_Context.device.CreatePipelineLayout(&layoutDesc);
    if (!m_PipelineLayout) {
        WEBGPU_LOG_ERROR("Failed to create pipeline layout");
        throw std::runtime_error("Failed to create WebGPU pipeline layout");
    }

    // Create render pipeline descriptor
    wgpu::RenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.label = "Render Pipeline";
    pipelineDesc.layout = m_PipelineLayout;

    // Vertex state
    wgpu::VertexState vertexState{};
    if (desc.vertexShader) {
        auto webgpuShader = static_cast<WebGPUShader*>(desc.vertexShader.get());
        vertexState.module = webgpuShader->GetModule();
        vertexState.entryPoint = webgpuShader->GetEntryPoint().c_str();
    }

    // Vertex buffer layouts
    std::vector<wgpu::VertexBufferLayout> vertexBuffers;
    std::vector<std::vector<wgpu::VertexAttribute>> vertexAttributesStorage;

    const auto& attributes = !desc.vertexInputState.attributes.empty()
        ? desc.vertexInputState.attributes
        : desc.vertexInput.attributes;
    const auto& bindings = desc.vertexInputState.bindings;

    if (!attributes.empty()) {
        // Group attributes by binding
        std::map<uint32, std::vector<VertexAttribute>> attributesByBinding;
        for (const auto& attr : attributes) {
            attributesByBinding[attr.binding].push_back(attr);
        }

        // Create vertex buffer layouts
        for (const auto& [binding, attrs] : attributesByBinding) {
            // Find stride from bindings or use vertexInput.stride
            uint32 stride = desc.vertexInput.stride;
            VertexInputRate inputRate = VertexInputRate::Vertex;

            if (!bindings.empty()) {
                auto bindingIt = std::find_if(bindings.begin(), bindings.end(),
                    [binding](const VertexInputBinding& b) { return b.binding == binding; });
                if (bindingIt != bindings.end()) {
                    stride = bindingIt->stride;
                    inputRate = bindingIt->inputRate;
                }
            }

            // Create WebGPU vertex attributes
            std::vector<wgpu::VertexAttribute> wgpuAttrs;
            for (const auto& attr : attrs) {
                wgpu::VertexAttribute wgpuAttr{};
                wgpuAttr.format = ToWebGPUVertexFormat(attr.format);
                wgpuAttr.offset = attr.offset;
                wgpuAttr.shaderLocation = attr.location;
                wgpuAttrs.push_back(wgpuAttr);
            }

            vertexAttributesStorage.push_back(std::move(wgpuAttrs));

            wgpu::VertexBufferLayout bufferLayout{};
            bufferLayout.arrayStride = stride;
            bufferLayout.stepMode = (inputRate == VertexInputRate::Vertex)
                ? wgpu::VertexStepMode::Vertex
                : wgpu::VertexStepMode::Instance;
            bufferLayout.attributeCount = vertexAttributesStorage.back().size();
            bufferLayout.attributes = vertexAttributesStorage.back().data();

            vertexBuffers.push_back(bufferLayout);
        }

        vertexState.bufferCount = vertexBuffers.size();
        vertexState.buffers = vertexBuffers.data();
    }

    pipelineDesc.vertex = vertexState;

    // Primitive state
    wgpu::PrimitiveState primitiveState{};
    primitiveState.topology = m_PrimitiveTopology;
    primitiveState.stripIndexFormat = wgpu::IndexFormat::Undefined;
    primitiveState.frontFace = m_FrontFace;
    primitiveState.cullMode = m_CullMode;

    pipelineDesc.primitive = primitiveState;

    // Depth/stencil state
    wgpu::DepthStencilState depthStencilState{};
    if (desc.depthStencil.depthTestEnable || desc.depthStencil.depthWriteEnable) {
        depthStencilState.format = wgpu::TextureFormat::Depth32Float;
        depthStencilState.depthWriteEnabled = desc.depthStencil.depthWriteEnable;
        depthStencilState.depthCompare = ToWebGPUCompareFunction(desc.depthStencil.depthCompareOp);

        pipelineDesc.depthStencil = &depthStencilState;
    }

    // Multisample state
    wgpu::MultisampleState multisampleState{};
    multisampleState.count = 1;
    multisampleState.mask = 0xFFFFFFFF;
    multisampleState.alphaToCoverageEnabled = false;

    pipelineDesc.multisample = multisampleState;

    // Fragment state
    wgpu::FragmentState fragmentState{};
    wgpu::ColorTargetState colorTarget{};
    wgpu::BlendState blendState{};

    if (desc.fragmentShader) {
        auto webgpuShader = static_cast<WebGPUShader*>(desc.fragmentShader.get());
        fragmentState.module = webgpuShader->GetModule();
        fragmentState.entryPoint = webgpuShader->GetEntryPoint().c_str();

        // Color targets
        if (desc.colorAttachments.empty()) {
            // Default: one color attachment with BGRA8
            colorTarget.format = wgpu::TextureFormat::BGRA8Unorm;
            colorTarget.writeMask = wgpu::ColorWriteMask::All;

            fragmentState.targetCount = 1;
            fragmentState.targets = &colorTarget;
        } else {
            // Use specified color attachments
            // For simplicity, use first attachment's settings
            colorTarget.format = wgpu::TextureFormat::BGRA8Unorm;
            colorTarget.writeMask = wgpu::ColorWriteMask::All;

            if (desc.colorAttachments[0].blendEnable) {
                // Default alpha blending
                blendState.color.operation = wgpu::BlendOperation::Add;
                blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
                blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
                blendState.alpha.operation = wgpu::BlendOperation::Add;
                blendState.alpha.srcFactor = wgpu::BlendFactor::One;
                blendState.alpha.dstFactor = wgpu::BlendFactor::Zero;

                colorTarget.blend = &blendState;
            }

            fragmentState.targetCount = 1;
            fragmentState.targets = &colorTarget;
        }

        pipelineDesc.fragment = &fragmentState;
    }

    // Create render pipeline
    m_RenderPipeline = m_Context.device.CreateRenderPipeline(&pipelineDesc);

    if (!m_RenderPipeline) {
        WEBGPU_LOG_ERROR("Failed to create render pipeline");
        throw std::runtime_error("Failed to create WebGPU render pipeline");
    }

    WEBGPU_LOG_INFO("WebGPU render pipeline created successfully");
}

WebGPUPipeline::~WebGPUPipeline() {
    m_RenderPipeline = nullptr;
    m_PipelineLayout = nullptr;
}

} // namespace rhi
} // namespace metagfx
