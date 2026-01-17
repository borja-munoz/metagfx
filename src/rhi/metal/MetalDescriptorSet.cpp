// ============================================================================
// src/rhi/metal/MetalDescriptorSet.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalDescriptorSet.h"
#include "metagfx/rhi/metal/MetalBuffer.h"
#include "metagfx/rhi/metal/MetalTexture.h"
#include "metagfx/rhi/metal/MetalSampler.h"

namespace metagfx {
namespace rhi {

MetalDescriptorSet::MetalDescriptorSet(MetalContext& context, const DescriptorSetDesc& desc)
    : m_Context(context), m_Bindings(desc.bindings) {
    // Metal stores the bindings and applies them directly when rendering
    // No need to create layout objects like Vulkan
}

MetalDescriptorSet::~MetalDescriptorSet() {
    // Nothing to release - Metal doesn't have descriptor set objects
}

void MetalDescriptorSet::UpdateBuffer(uint32 binding, Ref<Buffer> buffer) {
    for (auto& b : m_Bindings) {
        if (b.binding == binding) {
            b.buffer = buffer;
            return;
        }
    }
}

void MetalDescriptorSet::UpdateTexture(uint32 binding, Ref<Texture> texture, Ref<Sampler> sampler) {
    for (auto& b : m_Bindings) {
        if (b.binding == binding) {
            b.texture = texture;
            b.sampler = sampler;
            return;
        }
    }
}

void* MetalDescriptorSet::GetNativeHandle(uint32 frameIndex) const {
    // Metal doesn't have descriptor set handles
    // Return this pointer as an identifier
    (void)frameIndex;
    return const_cast<MetalDescriptorSet*>(this);
}

void* MetalDescriptorSet::GetNativeLayout() const {
    // Metal doesn't have descriptor set layouts
    return nullptr;
}

void MetalDescriptorSet::ApplyToEncoder(MTL::RenderCommandEncoder* encoder, uint32 frameIndex) const {
    if (!encoder) return;

    (void)frameIndex;  // Metal doesn't need frame index for binding

    // IMPORTANT: Buffer offset must match what's used in MetalShader.cpp for SPIRV-Cross binding remapping
    // This offset separates uniform/storage buffers from vertex buffers which share the same namespace
    const uint32 BUFFER_OFFSET = 10;

    // DEBUG: Log bindings for first few frames
    static int applyCount = 0;
    applyCount++;
    bool logThis = (applyCount <= 3);
    if (logThis) {
        METAGFX_INFO << "MetalDescriptorSet::ApplyToEncoder - " << m_Bindings.size() << " bindings";
    }

    for (const auto& binding : m_Bindings) {
        switch (binding.type) {
            case DescriptorType::UniformBuffer:
            case DescriptorType::StorageBuffer:
                if (binding.buffer) {
                    auto metalBuffer = std::static_pointer_cast<MetalBuffer>(binding.buffer);
                    // Apply BUFFER_OFFSET to avoid conflict with vertex buffers
                    uint32 metalBufferIndex = binding.binding + BUFFER_OFFSET;
                    if (logThis) {
                        METAGFX_INFO << "  Binding " << binding.binding << " -> Metal buffer " << metalBufferIndex
                                     << ": Buffer (type="
                                     << (binding.type == DescriptorType::UniformBuffer ? "UBO" : "Storage")
                                     << ", size=" << metalBuffer->GetSize() << ")";
                    }
                    // Bind to both vertex and fragment stages based on stageFlags
                    if (static_cast<int>(binding.stageFlags) & static_cast<int>(ShaderStage::Vertex)) {
                        encoder->setVertexBuffer(metalBuffer->GetHandle(), 0, metalBufferIndex);
                    }
                    if (static_cast<int>(binding.stageFlags) & static_cast<int>(ShaderStage::Fragment)) {
                        encoder->setFragmentBuffer(metalBuffer->GetHandle(), 0, metalBufferIndex);
                    }
                }
                break;

            case DescriptorType::SampledTexture:
                if (binding.texture) {
                    auto metalTexture = std::static_pointer_cast<MetalTexture>(binding.texture);
                    if (logThis) {
                        METAGFX_INFO << "  Binding " << binding.binding << ": Texture ("
                                     << metalTexture->GetWidth() << "x" << metalTexture->GetHeight() << ")";
                    }
                    if (static_cast<int>(binding.stageFlags) & static_cast<int>(ShaderStage::Vertex)) {
                        encoder->setVertexTexture(metalTexture->GetHandle(), binding.binding);
                    }
                    if (static_cast<int>(binding.stageFlags) & static_cast<int>(ShaderStage::Fragment)) {
                        encoder->setFragmentTexture(metalTexture->GetHandle(), binding.binding);
                    }
                }
                if (binding.sampler) {
                    auto metalSampler = std::static_pointer_cast<MetalSampler>(binding.sampler);
                    if (logThis) {
                        METAGFX_INFO << "  Binding " << binding.binding << ": Sampler";
                    }
                    if (static_cast<int>(binding.stageFlags) & static_cast<int>(ShaderStage::Vertex)) {
                        encoder->setVertexSamplerState(metalSampler->GetHandle(), binding.binding);
                    }
                    if (static_cast<int>(binding.stageFlags) & static_cast<int>(ShaderStage::Fragment)) {
                        encoder->setFragmentSamplerState(metalSampler->GetHandle(), binding.binding);
                    }
                }
                break;

            case DescriptorType::StorageTexture:
                // Read/write textures - similar to sampled but without sampler
                if (binding.texture) {
                    auto metalTexture = std::static_pointer_cast<MetalTexture>(binding.texture);
                    if (static_cast<int>(binding.stageFlags) & static_cast<int>(ShaderStage::Fragment)) {
                        encoder->setFragmentTexture(metalTexture->GetHandle(), binding.binding);
                    }
                }
                break;

            case DescriptorType::Sampler:
                if (binding.sampler) {
                    auto metalSampler = std::static_pointer_cast<MetalSampler>(binding.sampler);
                    if (static_cast<int>(binding.stageFlags) & static_cast<int>(ShaderStage::Fragment)) {
                        encoder->setFragmentSamplerState(metalSampler->GetHandle(), binding.binding);
                    }
                }
                break;
        }
    }
}

} // namespace rhi
} // namespace metagfx
