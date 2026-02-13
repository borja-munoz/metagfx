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

    // Process all bindings directly - Application.cpp now uses sparse layout
    // with explicit gaps for auto-inserted samplers
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
                // WebGPU distinguishes between read-only and read-write storage buffers
                // The shader declares the light buffer as readonly, so use ReadOnlyStorage
                entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
                entry.buffer.hasDynamicOffset = false;
                entry.buffer.minBindingSize = 0;
                break;

            case DescriptorType::SampledTexture:
                // WebGPU separates texture and sampler
                // We'll create two entries: one for texture, one for sampler
                {
                    wgpu::BindGroupLayoutEntry texEntry = entry;

                    // Determine texture sample type and view dimension from the initial texture (if provided)
                    if (binding.texture) {
                        // Check if it's a depth texture or regular texture
                        auto webgpuTex = static_cast<WebGPUTexture*>(binding.texture.get());
                        Format format = webgpuTex->GetFormat();

                        // Depth formats need Depth sample type
                        if (format == Format::D32_SFLOAT || format == Format::D24_UNORM_S8_UINT ||
                            format == Format::D16_UNORM || format == Format::D32_SFLOAT_S8_UINT) {
                            texEntry.texture.sampleType = wgpu::TextureSampleType::Depth;
                        } else {
                            texEntry.texture.sampleType = wgpu::TextureSampleType::Float;
                        }

                        // Check if it's a cubemap
                        if (webgpuTex->GetType() == TextureType::TextureCube) {
                            texEntry.texture.viewDimension = wgpu::TextureViewDimension::Cube;
                        } else {
                            texEntry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
                        }
                    } else {
                        // Default to Float and 2D if no texture provided yet
                        texEntry.texture.sampleType = wgpu::TextureSampleType::Float;
                        texEntry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
                    }

                    texEntry.texture.multisampled = false;
                    layoutEntries.push_back(texEntry);

                    wgpu::BindGroupLayoutEntry samplerEntry{};
                    // Tint separates combined image samplers: texture at N, sampler at N+1
                    // We MUST match this layout, even if it causes binding number conflicts
                    // The shader compiler dictates the binding layout
                    samplerEntry.binding = binding.binding + 1;
                    samplerEntry.visibility = entry.visibility;

                    // Determine sampler type based on the initial sampler (if provided)
                    if (binding.sampler) {
                        // Check if it's a comparison sampler by checking the underlying WebGPU sampler
                        auto webgpuSampler = static_cast<WebGPUSampler*>(binding.sampler.get());
                        // For now, assume comparison samplers are used with depth textures
                        // We'll default to Filtering for regular samplers
                        if (binding.texture) {
                            auto webgpuTex = static_cast<WebGPUTexture*>(binding.texture.get());
                            Format format = webgpuTex->GetFormat();
                            if (format == Format::D32_SFLOAT || format == Format::D24_UNORM_S8_UINT ||
                                format == Format::D16_UNORM || format == Format::D32_SFLOAT_S8_UINT) {
                                samplerEntry.sampler.type = wgpu::SamplerBindingType::Comparison;
                            } else {
                                samplerEntry.sampler.type = wgpu::SamplerBindingType::Filtering;
                            }
                        } else {
                            samplerEntry.sampler.type = wgpu::SamplerBindingType::Filtering;
                        }
                    } else {
                        samplerEntry.sampler.type = wgpu::SamplerBindingType::Filtering;
                    }

                    layoutEntries.push_back(samplerEntry);

                    // Store the sampler binding for later use in Update()
                    m_SamplerBindings[binding.binding] = samplerEntry.binding;
                }
                break;

            case DescriptorType::Sampler:
                entry.sampler.type = wgpu::SamplerBindingType::Filtering;
                break;

            case DescriptorType::StorageTexture:
                entry.storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
                entry.storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
                entry.storageTexture.viewDimension = wgpu::TextureViewDimension::e2D;
                break;

            default:
                METAGFX_WARN << "Unsupported descriptor type: " << static_cast<int>(binding.type);
                continue;
        }

        if (binding.type != DescriptorType::SampledTexture) {
            layoutEntries.push_back(entry);
        }

        // Store binding info with initial resources
        BindingInfo info{};
        info.binding = binding.binding;
        info.type = binding.type;
        info.buffer = binding.buffer;      // Copy initial buffer (if any)
        info.texture = binding.texture;    // Copy initial texture (if any)
        info.sampler = binding.sampler;    // Copy initial sampler (if any)
        m_Bindings.push_back(info);
    }

    // Create bind group layout
    wgpu::BindGroupLayoutDescriptor layoutDesc{};
    layoutDesc.label = "Bind Group Layout";
    layoutDesc.entryCount = layoutEntries.size();
    layoutDesc.entries = layoutEntries.data();

    m_BindGroupLayout = m_Context.device.CreateBindGroupLayout(&layoutDesc);
    if (!m_BindGroupLayout) {
        METAGFX_ERROR << "Failed to create bind group layout";
        throw std::runtime_error("Failed to create WebGPU bind group layout");
    }

    // Debug: Log what we created
    METAGFX_INFO << "Created bind group layout with " << layoutEntries.size() << " layout entries";
    METAGFX_INFO << "Stored " << m_Bindings.size() << " binding infos";
    METAGFX_INFO << "Created " << m_SamplerBindings.size() << " sampler binding mappings";
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

    METAGFX_WARN << "UpdateBuffer: binding " << binding << " not found";
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

    METAGFX_WARN << "UpdateTexture: binding " << binding << " not found";
}

void WebGPUDescriptorSet::Update() {
    // Create bind group entries from current bindings
    std::vector<wgpu::BindGroupEntry> entries;

    // Count how many resources are actually set
    uint32 expectedEntryCount = 0;
    uint32 actualEntryCount = 0;

    METAGFX_INFO << "WebGPUDescriptorSet::Update() - Checking " << m_Bindings.size() << " bindings";

    for (const auto& info : m_Bindings) {
        if (info.type == DescriptorType::UniformBuffer || info.type == DescriptorType::StorageBuffer) {
            expectedEntryCount++;
            if (info.buffer) {
                actualEntryCount++;
                wgpu::BindGroupEntry entry{};
                entry.binding = info.binding;
                auto webgpuBuffer = static_cast<WebGPUBuffer*>(info.buffer.get());
                entry.buffer = webgpuBuffer->GetHandle();
                entry.offset = 0;
                // Use the actual allocated size (aligned) for WebGPU binding requirements
                entry.size = webgpuBuffer->GetAllocatedSize();
                entries.push_back(entry);

                // DEBUG: Log uniform buffer bindings, especially binding 24
                static bool loggedBinding24 = false;
                if (info.binding == 24 && !loggedBinding24) {
                    METAGFX_INFO << "Binding 24 (push constants) added to bind group:";
                    METAGFX_INFO << "  Buffer handle: " << entry.buffer.Get();
                    METAGFX_INFO << "  Offset: " << entry.offset;
                    METAGFX_INFO << "  Size: " << entry.size << " bytes";
                    loggedBinding24 = true;
                }
            } else {
                METAGFX_WARN << "  Binding " << info.binding << " (buffer) has no resource set";
            }
        } else if (info.type == DescriptorType::SampledTexture) {
            expectedEntryCount += 2;  // Texture + sampler
            if (info.texture && info.sampler) {
                actualEntryCount += 2;

                // Texture entry
                wgpu::BindGroupEntry texEntry{};
                texEntry.binding = info.binding;
                auto webgpuTexture = static_cast<WebGPUTexture*>(info.texture.get());
                texEntry.textureView = webgpuTexture->GetView();
                entries.push_back(texEntry);

                // Sampler entry - use the allocated binding from constructor
                wgpu::BindGroupEntry samplerEntry{};
                auto samplerBindingIt = m_SamplerBindings.find(info.binding);
                if (samplerBindingIt != m_SamplerBindings.end()) {
                    samplerEntry.binding = samplerBindingIt->second;
                    auto webgpuSampler = static_cast<WebGPUSampler*>(info.sampler.get());
                    samplerEntry.sampler = webgpuSampler->GetHandle();
                    entries.push_back(samplerEntry);
                } else {
                    METAGFX_ERROR << "Sampler binding not found for texture binding " << info.binding;
                    // This should never happen - indicates a bug in constructor
                    actualEntryCount--;  // Correct the count since we couldn't add sampler
                }
            } else {
                METAGFX_WARN << "  Binding " << info.binding << " (texture) has no texture or sampler set";
            }
        } else if (info.type == DescriptorType::Sampler) {
            expectedEntryCount++;
            if (info.sampler) {
                actualEntryCount++;
                wgpu::BindGroupEntry entry{};
                entry.binding = info.binding;
                auto webgpuSampler = static_cast<WebGPUSampler*>(info.sampler.get());
                entry.sampler = webgpuSampler->GetHandle();
                entries.push_back(entry);
            }
        }
    }

    // Check if we have any resources to bind
    if (entries.empty()) {
        METAGFX_WARN << "No resources to bind - bind group creation skipped";
        return;
    }

    // WebGPU requires ALL bindings in the layout to have entries
    // If we don't have all resources, we can't create a valid bind group yet
    if (actualEntryCount != expectedEntryCount) {
        METAGFX_WARN << "Skipping bind group creation: only " << actualEntryCount
                      << " of " << expectedEntryCount << " resources are set";
        METAGFX_WARN << "  Expected: " << expectedEntryCount << ", Actual: " << actualEntryCount;
        METAGFX_WARN << "  Entries created so far: " << entries.size();
        return;
    }

    // Create bind group
    wgpu::BindGroupDescriptor groupDesc{};
    groupDesc.label = "Bind Group";
    groupDesc.layout = m_BindGroupLayout;
    groupDesc.entryCount = entries.size();
    groupDesc.entries = entries.data();

    m_BindGroup = m_Context.device.CreateBindGroup(&groupDesc);
    if (!m_BindGroup) {
        METAGFX_ERROR << "Failed to create bind group with " << entries.size() << " entries";
        throw std::runtime_error("Failed to create WebGPU bind group");
    }

    METAGFX_INFO << "Bind group updated with " << entries.size() << " entries";
}

void* WebGPUDescriptorSet::GetNativeHandle(uint32 frameIndex) const {
    return m_BindGroup.Get();
}

void* WebGPUDescriptorSet::GetNativeLayout() const {
    return m_BindGroupLayout.Get();
}

} // namespace rhi
} // namespace metagfx
