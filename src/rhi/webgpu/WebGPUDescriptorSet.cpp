// ============================================================================
// src/rhi/webgpu/WebGPUDescriptorSet.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUDescriptorSet.h"
#include "metagfx/rhi/webgpu/WebGPUBuffer.h"
#include "metagfx/rhi/webgpu/WebGPUTexture.h"
#include "metagfx/rhi/webgpu/WebGPUSampler.h"
#include "metagfx/core/Logger.h"

namespace metagfx {
namespace rhi {

WebGPUDescriptorSet::WebGPUDescriptorSet(WebGPUContext& context, const DescriptorSetDesc& desc)
    : m_Context(context) {

    // Create bind group layout
    std::vector<wgpu::BindGroupLayoutEntry> layoutEntries;

    for (const auto& binding : desc.bindings) {
        wgpu::BindGroupLayoutEntry entry{};
        entry.binding = binding.binding;
        entry.visibility = ToWebGPUShaderStage(binding.stageFlags);

        switch (binding.type) {
            case DescriptorType::UniformBuffer:
                entry.buffer.type = wgpu::BufferBindingType::Uniform;
                entry.buffer.hasDynamicOffset = false;
                entry.buffer.minBindingSize = 0;
                break;

            case DescriptorType::StorageBuffer:
                entry.buffer.type = wgpu::BufferBindingType::Storage;
                entry.buffer.hasDynamicOffset = false;
                entry.buffer.minBindingSize = 0;
                break;

            case DescriptorType::CombinedImageSampler:
                // WebGPU separates texture and sampler
                // We'll create two entries: one for texture, one for sampler
                {
                    wgpu::BindGroupLayoutEntry texEntry = entry;
                    texEntry.texture.sampleType = wgpu::TextureSampleType::Float;
                    texEntry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
                    texEntry.texture.multisampled = false;
                    layoutEntries.push_back(texEntry);

                    wgpu::BindGroupLayoutEntry samplerEntry{};
                    samplerEntry.binding = binding.binding + 1000;  // Offset sampler binding
                    samplerEntry.visibility = entry.visibility;
                    samplerEntry.sampler.type = wgpu::SamplerBindingType::Filtering;
                    layoutEntries.push_back(samplerEntry);
                }
                break;

            case DescriptorType::Sampler:
                entry.sampler.type = wgpu::SamplerBindingType::Filtering;
                break;

            case DescriptorType::SampledImage:
                entry.texture.sampleType = wgpu::TextureSampleType::Float;
                entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
                entry.texture.multisampled = false;
                break;

            case DescriptorType::StorageImage:
                entry.storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
                entry.storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
                entry.storageTexture.viewDimension = wgpu::TextureViewDimension::e2D;
                break;

            default:
                WEBGPU_LOG_WARNING("Unsupported descriptor type: " << static_cast<int>(binding.type));
                continue;
        }

        if (binding.type != DescriptorType::CombinedImageSampler) {
            layoutEntries.push_back(entry);
        }

        // Store binding info
        BindingInfo info{};
        info.binding = binding.binding;
        info.type = binding.type;
        m_Bindings.push_back(info);
    }

    // Create bind group layout
    wgpu::BindGroupLayoutDescriptor layoutDesc{};
    layoutDesc.label = "Bind Group Layout";
    layoutDesc.entryCount = layoutEntries.size();
    layoutDesc.entries = layoutEntries.data();

    m_BindGroupLayout = m_Context.device.CreateBindGroupLayout(&layoutDesc);
    if (!m_BindGroupLayout) {
        WEBGPU_LOG_ERROR("Failed to create bind group layout");
        throw std::runtime_error("Failed to create WebGPU bind group layout");
    }
}

WebGPUDescriptorSet::~WebGPUDescriptorSet() {
    m_BindGroup = nullptr;
    m_BindGroupLayout = nullptr;
}

void WebGPUDescriptorSet::UpdateBuffer(uint32 binding, Ref<Buffer> buffer) {
    // Find binding info
    for (auto& info : m_Bindings) {
        if (info.binding == binding) {
            info.buffer = buffer;
            return;
        }
    }

    WEBGPU_LOG_WARNING("UpdateBuffer: binding " << binding << " not found");
}

void WebGPUDescriptorSet::UpdateTexture(uint32 binding, Ref<Texture> texture, Ref<Sampler> sampler) {
    // Find binding info
    for (auto& info : m_Bindings) {
        if (info.binding == binding) {
            info.texture = texture;
            info.sampler = sampler;
            return;
        }
    }

    WEBGPU_LOG_WARNING("UpdateTexture: binding " << binding << " not found");
}

void WebGPUDescriptorSet::Update() {
    // Create bind group entries from current bindings
    std::vector<wgpu::BindGroupEntry> entries;

    for (const auto& info : m_Bindings) {
        if (info.type == DescriptorType::UniformBuffer || info.type == DescriptorType::StorageBuffer) {
            if (info.buffer) {
                wgpu::BindGroupEntry entry{};
                entry.binding = info.binding;
                auto webgpuBuffer = static_cast<WebGPUBuffer*>(info.buffer.get());
                entry.buffer = webgpuBuffer->GetHandle();
                entry.offset = 0;
                entry.size = info.buffer->GetSize();
                entries.push_back(entry);
            }
        } else if (info.type == DescriptorType::CombinedImageSampler) {
            if (info.texture && info.sampler) {
                // Texture entry
                wgpu::BindGroupEntry texEntry{};
                texEntry.binding = info.binding;
                auto webgpuTexture = static_cast<WebGPUTexture*>(info.texture.get());
                texEntry.textureView = webgpuTexture->GetView();
                entries.push_back(texEntry);

                // Sampler entry
                wgpu::BindGroupEntry samplerEntry{};
                samplerEntry.binding = info.binding + 1000;  // Offset sampler binding
                auto webgpuSampler = static_cast<WebGPUSampler*>(info.sampler.get());
                samplerEntry.sampler = webgpuSampler->GetHandle();
                entries.push_back(samplerEntry);
            }
        } else if (info.type == DescriptorType::SampledImage) {
            if (info.texture) {
                wgpu::BindGroupEntry entry{};
                entry.binding = info.binding;
                auto webgpuTexture = static_cast<WebGPUTexture*>(info.texture.get());
                entry.textureView = webgpuTexture->GetView();
                entries.push_back(entry);
            }
        } else if (info.type == DescriptorType::Sampler) {
            if (info.sampler) {
                wgpu::BindGroupEntry entry{};
                entry.binding = info.binding;
                auto webgpuSampler = static_cast<WebGPUSampler*>(info.sampler.get());
                entry.sampler = webgpuSampler->GetHandle();
                entries.push_back(entry);
            }
        }
    }

    // Create bind group
    wgpu::BindGroupDescriptor groupDesc{};
    groupDesc.label = "Bind Group";
    groupDesc.layout = m_BindGroupLayout;
    groupDesc.entryCount = entries.size();
    groupDesc.entries = entries.data();

    m_BindGroup = m_Context.device.CreateBindGroup(&groupDesc);
    if (!m_BindGroup) {
        WEBGPU_LOG_ERROR("Failed to create bind group");
        throw std::runtime_error("Failed to create WebGPU bind group");
    }

    WEBGPU_LOG_INFO("Bind group updated with " << entries.size() << " entries");
}

void* WebGPUDescriptorSet::GetNativeHandle(uint32 frameIndex) const {
    return m_BindGroup.Get();
}

void* WebGPUDescriptorSet::GetNativeLayout() const {
    return m_BindGroupLayout.Get();
}

} // namespace rhi
} // namespace metagfx
