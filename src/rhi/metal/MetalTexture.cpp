// ============================================================================
// src/rhi/metal/MetalTexture.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalTexture.h"

namespace metagfx {
namespace rhi {

// Constructor for swap chain textures (borrowed, don't own)
MetalTexture::MetalTexture(MetalContext& context, MTL::Texture* texture,
                           uint32 width, uint32 height, MTL::PixelFormat format)
    : m_Context(context)
    , m_Texture(texture)
    , m_Width(width)
    , m_Height(height)
    , m_Format(FromMetalPixelFormat(format))
    , m_OwnsTexture(false) {
    // Don't retain the texture - it's owned by the drawable
}

// Constructor for created textures (owned)
MetalTexture::MetalTexture(MetalContext& context, const TextureDesc& desc)
    : m_Context(context)
    , m_Width(desc.width)
    , m_Height(desc.height)
    , m_MipLevels(desc.mipLevels)
    , m_ArrayLayers(desc.arrayLayers)
    , m_Format(desc.format)
    , m_Type(desc.type)
    , m_OwnsTexture(true) {

    MTL::TextureDescriptor* textureDesc = MTL::TextureDescriptor::alloc()->init();

    // Set texture type
    switch (desc.type) {
        case TextureType::Texture2D:
            textureDesc->setTextureType(MTL::TextureType2D);
            break;
        case TextureType::Texture3D:
            textureDesc->setTextureType(MTL::TextureType3D);
            break;
        case TextureType::TextureCube:
            textureDesc->setTextureType(MTL::TextureTypeCube);
            break;
        default:
            textureDesc->setTextureType(MTL::TextureType2D);
            break;
    }

    textureDesc->setPixelFormat(ToMetalPixelFormat(desc.format));
    textureDesc->setWidth(desc.width);
    textureDesc->setHeight(desc.height);
    textureDesc->setMipmapLevelCount(desc.mipLevels);

    // For cubemaps, arrayLength must be 1 (6 faces are implicit in the cube type)
    // For cube arrays, arrayLength would be the number of cubes
    if (desc.type == TextureType::TextureCube) {
        textureDesc->setArrayLength(1);
    } else {
        textureDesc->setArrayLength(desc.arrayLayers);
    }

    // Set usage flags
    MTL::TextureUsage usage = MTL::TextureUsageShaderRead;
    bool isRenderTarget = static_cast<int>(desc.usage) & static_cast<int>(TextureUsage::ColorAttachment);
    bool isDepthStencil = static_cast<int>(desc.usage) & static_cast<int>(TextureUsage::DepthStencilAttachment);
    if (isRenderTarget || isDepthStencil) {
        usage |= MTL::TextureUsageRenderTarget;
    }
    if (static_cast<int>(desc.usage) & static_cast<int>(TextureUsage::Storage)) {
        usage |= MTL::TextureUsageShaderWrite;
    }
    textureDesc->setUsage(usage);

    // Set storage mode based on usage
    if (isRenderTarget || isDepthStencil) {
        // Render targets should be GPU-only for performance
        textureDesc->setStorageMode(MTL::StorageModePrivate);
    } else {
        // Regular textures that need CPU upload can use shared on iOS or managed on macOS
#if TARGET_OS_OSX
        textureDesc->setStorageMode(MTL::StorageModeManaged);
#else
        textureDesc->setStorageMode(MTL::StorageModeShared);
#endif
    }

    m_Texture = m_Context.device->newTexture(textureDesc);
    textureDesc->release();

    if (!m_Texture) {
        MTL_LOG_ERROR("Failed to create texture");
        return;
    }

    if (desc.debugName) {
        NS::String* label = NS::String::string(desc.debugName, NS::UTF8StringEncoding);
        m_Texture->setLabel(label);
        label->release();
    }
}

MetalTexture::~MetalTexture() {
    if (m_Texture && m_OwnsTexture) {
        m_Texture->release();
    }
    m_Texture = nullptr;
}

// Helper function to get bytes per pixel for a format
static uint32 GetBytesPerPixel(Format format) {
    switch (format) {
        case Format::R8_UNORM:
        case Format::R8_SNORM:
        case Format::R8_UINT:
        case Format::R8_SINT:
            return 1;
        case Format::R8G8_UNORM:
        case Format::R8G8_SNORM:
        case Format::R8G8_UINT:
        case Format::R8G8_SINT:
        case Format::R16_UNORM:
        case Format::R16_SNORM:
        case Format::R16_UINT:
        case Format::R16_SINT:
        case Format::R16_SFLOAT:
            return 2;
        case Format::R8G8B8A8_UNORM:
        case Format::R8G8B8A8_SNORM:
        case Format::R8G8B8A8_UINT:
        case Format::R8G8B8A8_SINT:
        case Format::R8G8B8A8_SRGB:
        case Format::B8G8R8A8_UNORM:
        case Format::B8G8R8A8_SRGB:
        case Format::R32_UINT:
        case Format::R32_SINT:
        case Format::R32_SFLOAT:
        case Format::R16G16_UNORM:
        case Format::R16G16_SNORM:
        case Format::R16G16_UINT:
        case Format::R16G16_SINT:
        case Format::R16G16_SFLOAT:
            return 4;
        case Format::R16G16B16A16_UNORM:
        case Format::R16G16B16A16_SNORM:
        case Format::R16G16B16A16_UINT:
        case Format::R16G16B16A16_SINT:
        case Format::R16G16B16A16_SFLOAT:
        case Format::R32G32_UINT:
        case Format::R32G32_SINT:
        case Format::R32G32_SFLOAT:
            return 8;
        case Format::R32G32B32_UINT:
        case Format::R32G32B32_SINT:
        case Format::R32G32B32_SFLOAT:
            return 12;
        case Format::R32G32B32A32_UINT:
        case Format::R32G32B32A32_SINT:
        case Format::R32G32B32A32_SFLOAT:
            return 16;
        default:
            return 4;
    }
}

void MetalTexture::UploadData(const void* data, uint64 size) {
    if (!m_Texture || !data) {
        return;
    }

    uint32 bytesPerPixel = GetBytesPerPixel(m_Format);
    const uint8* srcData = static_cast<const uint8*>(data);
    uint64 offset = 0;

    // Determine number of array layers (6 for cubemaps, otherwise m_ArrayLayers)
    uint32 numLayers = (m_Type == TextureType::TextureCube) ? 6 : m_ArrayLayers;

    // Upload each mip level and array layer/face
    for (uint32 mip = 0; mip < m_MipLevels; ++mip) {
        uint32 mipWidth = std::max(1u, m_Width >> mip);
        uint32 mipHeight = std::max(1u, m_Height >> mip);
        uint32 bytesPerRow = mipWidth * bytesPerPixel;
        uint64 faceSize = static_cast<uint64>(mipWidth) * mipHeight * bytesPerPixel;

        MTL::Region region = MTL::Region::Make2D(0, 0, mipWidth, mipHeight);

        for (uint32 layer = 0; layer < numLayers; ++layer) {
            if (offset + faceSize > size) {
                MTL_LOG_ERROR("Not enough data for texture upload at mip " << mip << " layer " << layer);
                return;
            }

            // For cubemaps, 'slice' is the face index (0-5)
            // For regular textures, 'slice' is the array layer
            m_Texture->replaceRegion(region, mip, layer, srcData + offset, bytesPerRow, 0);
            offset += faceSize;
        }
    }

    METAGFX_DEBUG << "Texture data uploaded: " << m_Width << "x" << m_Height
                  << ", mips=" << m_MipLevels << ", layers=" << numLayers
                  << ", total=" << offset << " bytes";
}

} // namespace rhi
} // namespace metagfx
