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
    , m_ArrayLayers(desc.type == TextureType::TextureCube ? 6 : desc.arrayLayers)
    , m_Format(desc.format)
    , m_Type(desc.type)
    , m_Usage(desc.usage) {

    CreateTexture(desc);
    CreateTextureView();
}

// Constructor for wrapping existing texture (e.g., swap chain surface)
WebGPUTexture::WebGPUTexture(WebGPUContext& context, WGPUTexture existingTexture, const TextureDesc& desc)
    : m_Context(context)
    , m_Width(desc.width)
    , m_Height(desc.height)
    , m_Depth(desc.depth)
    , m_MipLevels(desc.mipLevels)
    , m_Format(desc.format)
    , m_Type(desc.type)
    , m_Usage(desc.usage) {

    // Wrap the existing texture WITHOUT taking ownership
    // The surface owns this texture and will manage its lifetime
    m_Texture = wgpu::Texture(existingTexture);
    m_OwnsTexture = false;
    CreateTextureView();

    METAGFX_INFO << "Wrapped external texture: " << desc.width << "x" << desc.height;
}

WebGPUTexture::~WebGPUTexture() {
    m_TextureView = nullptr;

    // Only release the texture if we own it
    // Wrapped external textures (e.g., swap chain) are managed externally
    if (m_OwnsTexture) {
        m_Texture = nullptr;
    }
}

void WebGPUTexture::CreateTexture(const TextureDesc& desc) {
    // Convert texture format
    wgpu::TextureFormat format = ToWebGPUTextureFormat(m_Format);
    if (format == wgpu::TextureFormat::Undefined) {
        METAGFX_ERROR << "Invalid texture format: " << static_cast<int>(m_Format);
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
        // Sampled textures typically need CopyDst for data upload via queue.WriteTexture()
        usage |= wgpu::TextureUsage::CopyDst;
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
        METAGFX_ERROR << "Failed to create texture";
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
        METAGFX_ERROR << "Failed to create texture view";
        throw std::runtime_error("Failed to create WebGPU texture view");
    }
}

void WebGPUTexture::UploadData(const void* data, uint64 size) {
    if (!data || size == 0) {
        return;
    }

    // Calculate bytes per pixel based on format
    uint32 bytesPerPixel = 4;  // Default for RGBA8
    switch (m_Format) {
        case Format::R8G8B8A8_UNORM:
        case Format::B8G8R8A8_UNORM:
        case Format::R8G8B8A8_SRGB:
        case Format::B8G8R8A8_SRGB:
            bytesPerPixel = 4;
            break;
        case Format::R16G16B16A16_SFLOAT:
            bytesPerPixel = 8;
            break;
        case Format::R32G32B32A32_SFLOAT:
            bytesPerPixel = 16;
            break;
        case Format::R16G16_SFLOAT:
            bytesPerPixel = 4;
            break;
        case Format::D32_SFLOAT:
            bytesPerPixel = 4;
            break;
        case Format::D24_UNORM_S8_UINT:
            bytesPerPixel = 4;
            break;
        default:
            METAGFX_WARN << "Unknown format for bytesPerPixel calculation, using 4";
            bytesPerPixel = 4;
            break;
    }

    WGPUQueue queue = m_Context.queue.Get();
    const uint8* srcBase = static_cast<const uint8*>(data);

    if (m_Type == TextureType::TextureCube) {
        // DDS cubemap data layout is face-major:
        //   Face 0: mip0, mip1, mip2, ...
        //   Face 1: mip0, mip1, mip2, ...
        //   ...
        // Upload each face × mip level separately using the correct array layer (z).
        uint64 srcOffset = 0;

        for (uint32 face = 0; face < m_ArrayLayers; ++face) {
            for (uint32 mip = 0; mip < m_MipLevels; ++mip) {
                uint32 mipWidth  = std::max(1u, m_Width  >> mip);
                uint32 mipHeight = std::max(1u, m_Height >> mip);

                uint32 unalignedBytesPerRow = mipWidth * bytesPerPixel;
                // WebGPU: bytesPerRow must be a multiple of 256
                uint32 alignedBytesPerRow = (unalignedBytesPerRow + 255) & ~255;
                uint64 faceSize = static_cast<uint64>(unalignedBytesPerRow) * mipHeight;

                const uint8* srcFaceMip = srcBase + srcOffset;

                // Repack if alignment is needed
                std::vector<uint8> paddedData;
                const void* uploadData = srcFaceMip;

                if (unalignedBytesPerRow != alignedBytesPerRow) {
                    paddedData.resize(static_cast<uint64>(alignedBytesPerRow) * mipHeight);
                    uint8* dst = paddedData.data();
                    const uint8* src = srcFaceMip;
                    for (uint32 y = 0; y < mipHeight; ++y) {
                        std::memcpy(dst, src, unalignedBytesPerRow);
                        src += unalignedBytesPerRow;
                        dst += alignedBytesPerRow;
                    }
                    uploadData = paddedData.data();
                }

                WGPUTexelCopyBufferLayout dataLayout{};
                dataLayout.offset = 0;
                dataLayout.bytesPerRow = alignedBytesPerRow;
                dataLayout.rowsPerImage = mipHeight;

                WGPUOrigin3D origin{};
                origin.x = 0;
                origin.y = 0;
                origin.z = face;  // Cubemap face as array layer

                WGPUTexelCopyTextureInfo destination{};
                destination.texture = m_Texture.Get();
                destination.mipLevel = mip;
                destination.origin = origin;
                destination.aspect = WGPUTextureAspect_All;

                WGPUExtent3D writeSize{};
                writeSize.width = mipWidth;
                writeSize.height = mipHeight;
                writeSize.depthOrArrayLayers = 1;

                uint64 uploadSize = paddedData.empty() ? faceSize : paddedData.size();
                wgpuQueueWriteTexture(queue, &destination, uploadData, uploadSize, &dataLayout, &writeSize);

                srcOffset += faceSize;
            }
        }
    } else {
        // Standard 2D texture upload (single mip level 0)
        uint32 unalignedBytesPerRow = m_Width * bytesPerPixel;
        uint32 alignedBytesPerRow = (unalignedBytesPerRow + 255) & ~255;

        const void* uploadData = data;
        std::vector<uint8> paddedData;

        if (unalignedBytesPerRow != alignedBytesPerRow) {
            paddedData.resize(static_cast<uint64>(alignedBytesPerRow) * m_Height * m_Depth);
            const uint8* src = static_cast<const uint8*>(data);
            uint8* dst = paddedData.data();
            for (uint32 z = 0; z < m_Depth; ++z) {
                for (uint32 y = 0; y < m_Height; ++y) {
                    std::memcpy(dst, src, unalignedBytesPerRow);
                    src += unalignedBytesPerRow;
                    dst += alignedBytesPerRow;
                }
            }
            uploadData = paddedData.data();
        }

        WGPUTexelCopyBufferLayout dataLayout{};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = alignedBytesPerRow;
        dataLayout.rowsPerImage = m_Height;

        WGPUOrigin3D origin{};
        WGPUTexelCopyTextureInfo destination{};
        destination.texture = m_Texture.Get();
        destination.mipLevel = 0;
        destination.origin = origin;
        destination.aspect = WGPUTextureAspect_All;

        WGPUExtent3D writeSize{};
        writeSize.width = m_Width;
        writeSize.height = m_Height;
        writeSize.depthOrArrayLayers = 1;

        uint64 uploadSize = paddedData.empty() ? size : paddedData.size();
        wgpuQueueWriteTexture(queue, &destination, uploadData, uploadSize, &dataLayout, &writeSize);
    }
}

} // namespace rhi
} // namespace metagfx
