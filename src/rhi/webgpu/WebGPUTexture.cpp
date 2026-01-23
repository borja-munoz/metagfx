// ============================================================================
// src/rhi/webgpu/WebGPUTexture.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUTexture.h"
#include "metagfx/core/Logger.h"

namespace metagfx {
namespace rhi {

WebGPUTexture::WebGPUTexture(WebGPUContext& context, const TextureDesc& desc)
    : m_Context(context)
    , m_Width(desc.width)
    , m_Height(desc.height)
    , m_Depth(desc.depth)
    , m_MipLevels(desc.mipLevels)
    , m_Format(desc.format)
    , m_Type(desc.type)
    , m_Usage(desc.usage) {

    CreateTexture(desc);
    CreateTextureView();
}

WebGPUTexture::~WebGPUTexture() {
    m_TextureView = nullptr;
    m_Texture = nullptr;
}

void WebGPUTexture::CreateTexture(const TextureDesc& desc) {
    // Convert texture format
    wgpu::TextureFormat format = ToWebGPUTextureFormat(m_Format);
    if (format == wgpu::TextureFormat::Undefined) {
        WEBGPU_LOG_ERROR("Invalid texture format: " << static_cast<int>(m_Format));
        throw std::runtime_error("Invalid texture format");
    }

    // Determine texture dimension
    wgpu::TextureDimension dimension = wgpu::TextureDimension::e2D;
    if (m_Type == TextureType::Texture3D) {
        dimension = wgpu::TextureDimension::e3D;
    }

    // Convert texture usage
    wgpu::TextureUsage usage = wgpu::TextureUsage::None;
    if (static_cast<int>(m_Usage) & static_cast<int>(TextureUsage::Sampled)) {
        usage |= wgpu::TextureUsage::TextureBinding;
    }
    if (static_cast<int>(m_Usage) & static_cast<int>(TextureUsage::Storage)) {
        usage |= wgpu::TextureUsage::StorageBinding;
    }
    if (static_cast<int>(m_Usage) & static_cast<int>(TextureUsage::ColorAttachment)) {
        usage |= wgpu::TextureUsage::RenderAttachment;
    }
    if (static_cast<int>(m_Usage) & static_cast<int>(TextureUsage::DepthStencilAttachment)) {
        usage |= wgpu::TextureUsage::RenderAttachment;
    }
    if (static_cast<int>(m_Usage) & static_cast<int>(TextureUsage::TransferSrc)) {
        usage |= wgpu::TextureUsage::CopySrc;
    }
    if (static_cast<int>(m_Usage) & static_cast<int>(TextureUsage::TransferDst)) {
        usage |= wgpu::TextureUsage::CopyDst;
    }

    // Create texture descriptor
    wgpu::TextureDescriptor textureDesc{};
    textureDesc.label = desc.debugName ? desc.debugName : "Texture";
    textureDesc.dimension = dimension;
    textureDesc.size.width = m_Width;
    textureDesc.size.height = m_Height;
    textureDesc.size.depthOrArrayLayers = (m_Type == TextureType::TextureCube) ? 6 : m_Depth;
    textureDesc.format = format;
    textureDesc.mipLevelCount = m_MipLevels;
    textureDesc.sampleCount = 1;
    textureDesc.usage = usage;
    textureDesc.viewFormatCount = 0;
    textureDesc.viewFormats = nullptr;

    // Create the texture
    m_Texture = m_Context.device.CreateTexture(&textureDesc);

    if (!m_Texture) {
        WEBGPU_LOG_ERROR("Failed to create texture");
        throw std::runtime_error("Failed to create WebGPU texture");
    }
}

void WebGPUTexture::CreateTextureView() {
    // Determine view dimension
    wgpu::TextureViewDimension viewDimension = wgpu::TextureViewDimension::e2D;
    if (m_Type == TextureType::TextureCube) {
        viewDimension = wgpu::TextureViewDimension::Cube;
    } else if (m_Type == TextureType::Texture3D) {
        viewDimension = wgpu::TextureViewDimension::e3D;
    }

    // Create texture view descriptor
    wgpu::TextureViewDescriptor viewDesc{};
    viewDesc.format = ToWebGPUTextureFormat(m_Format);
    viewDesc.dimension = viewDimension;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = m_MipLevels;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = (m_Type == TextureType::TextureCube) ? 6 : 1;
    viewDesc.aspect = wgpu::TextureAspect::All;

    // Create the texture view
    m_TextureView = m_Texture.CreateView(&viewDesc);

    if (!m_TextureView) {
        WEBGPU_LOG_ERROR("Failed to create texture view");
        throw std::runtime_error("Failed to create WebGPU texture view");
    }
}

void WebGPUTexture::UploadData(const void* data, uint64 size) {
    if (!data || size == 0) {
        return;
    }

    // Calculate expected size for validation
    uint32 bytesPerPixel = 4;  // Assume RGBA8 for now (could be calculated from format)
    uint64 expectedSize = static_cast<uint64>(m_Width) * m_Height * m_Depth * bytesPerPixel;

    if (size < expectedSize) {
        WEBGPU_LOG_ERROR("Texture data size mismatch: provided=" << size
                        << ", expected at least=" << expectedSize);
    }

    // Prepare texture data layout
    wgpu::TextureDataLayout dataLayout{};
    dataLayout.offset = 0;
    dataLayout.bytesPerRow = m_Width * bytesPerPixel;
    dataLayout.rowsPerImage = m_Height;

    // Prepare image copy texture
    wgpu::ImageCopyTexture destination{};
    destination.texture = m_Texture;
    destination.mipLevel = 0;
    destination.origin = {0, 0, 0};
    destination.aspect = wgpu::TextureAspect::All;

    // Prepare extent
    wgpu::Extent3D writeSize{};
    writeSize.width = m_Width;
    writeSize.height = m_Height;
    writeSize.depthOrArrayLayers = 1;

    // Upload data to GPU
    m_Context.queue.WriteTexture(&destination, data, size, &dataLayout, &writeSize);
}

} // namespace rhi
} // namespace metagfx
