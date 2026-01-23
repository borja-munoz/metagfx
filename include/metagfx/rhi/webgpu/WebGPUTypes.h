// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUTypes.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

// Dawn WebGPU C++ headers
#include <webgpu/webgpu_cpp.h>

struct SDL_Window;

namespace metagfx {
namespace rhi {

// Helper to log WebGPU errors
#define WEBGPU_LOG_ERROR(msg) \
    METAGFX_ERROR << "WebGPU error: " << msg << " at " << __FILE__ << ":" << __LINE__

// WebGPU context shared across all WebGPU objects
struct WebGPUContext {
    wgpu::Instance instance = nullptr;
    wgpu::Adapter adapter = nullptr;
    wgpu::Device device = nullptr;
    wgpu::Queue queue = nullptr;
    wgpu::Surface surface = nullptr;

    // Device capabilities (queried at init)
    bool supportsTimestampQueries = false;
    bool supportsDepthClipControl = false;
    bool supportsBGRA8UnormStorage = false;

    // Limits
    uint32_t maxBindGroups = 4;
    uint32_t maxUniformBufferBindingSize = 65536;
    uint32_t minUniformBufferOffsetAlignment = 256;
};

// Format conversion utilities
wgpu::TextureFormat ToWebGPUTextureFormat(Format format);
wgpu::IndexFormat ToWebGPUIndexFormat(Format format);
wgpu::BufferUsage ToWebGPUBufferUsage(BufferUsage usage);
wgpu::AddressMode ToWebGPUAddressMode(SamplerAddressMode mode);
wgpu::FilterMode ToWebGPUFilterMode(Filter filter);
wgpu::MipmapFilterMode ToWebGPUMipMapFilterMode(Filter filter);
wgpu::CompareFunction ToWebGPUCompareFunction(CompareOp op);
wgpu::PrimitiveTopology ToWebGPUPrimitiveTopology(PrimitiveTopology topology);
wgpu::CullMode ToWebGPUCullMode(CullMode mode);
wgpu::FrontFace ToWebGPUFrontFace(FrontFace face);
wgpu::VertexFormat ToWebGPUVertexFormat(Format format);
wgpu::ShaderStage ToWebGPUShaderStage(ShaderStage stages);
wgpu::LoadOp ToWebGPULoadOp(bool shouldLoad);
wgpu::StoreOp ToWebGPUStoreOp(bool shouldStore);

} // namespace rhi
} // namespace metagfx
