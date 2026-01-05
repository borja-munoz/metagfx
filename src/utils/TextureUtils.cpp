// ============================================================================
// src/utils/TextureUtils.cpp
// ============================================================================
#include "metagfx/utils/TextureUtils.h"
#include "metagfx/core/Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <fstream>
#include <cstring>

namespace metagfx {
namespace utils {

// ============================================================================
// DDS File Format Structures
// ============================================================================

#pragma pack(push, 1)

struct DDSPixelFormat {
    uint32 size;
    uint32 flags;
    uint32 fourCC;
    uint32 RGBBitCount;
    uint32 RBitMask;
    uint32 GBitMask;
    uint32 BBitMask;
    uint32 ABitMask;
};

struct DDSHeader {
    uint32 size;
    uint32 flags;
    uint32 height;
    uint32 width;
    uint32 pitchOrLinearSize;
    uint32 depth;
    uint32 mipMapCount;
    uint32 reserved1[11];
    DDSPixelFormat ddspf;
    uint32 caps;
    uint32 caps2;
    uint32 caps3;
    uint32 caps4;
    uint32 reserved2;
};

struct DDSHeaderDXT10 {
    uint32 dxgiFormat;
    uint32 resourceDimension;
    uint32 miscFlag;
    uint32 arraySize;
    uint32 miscFlags2;
};

#pragma pack(pop)

// DDS constants
constexpr uint32 DDS_MAGIC = 0x20534444; // "DDS "
constexpr uint32 DDSD_CAPS = 0x1;
constexpr uint32 DDSD_HEIGHT = 0x2;
constexpr uint32 DDSD_WIDTH = 0x4;
constexpr uint32 DDSD_PIXELFORMAT = 0x1000;
constexpr uint32 DDSD_MIPMAPCOUNT = 0x20000;
constexpr uint32 DDSCAPS_COMPLEX = 0x8;
constexpr uint32 DDSCAPS_TEXTURE = 0x1000;
constexpr uint32 DDSCAPS_MIPMAP = 0x400000;
constexpr uint32 DDSCAPS2_CUBEMAP = 0x200;
constexpr uint32 DDSCAPS2_CUBEMAP_POSITIVEX = 0x400;
constexpr uint32 DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800;
constexpr uint32 DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000;
constexpr uint32 DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000;
constexpr uint32 DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000;
constexpr uint32 DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000;
constexpr uint32 DDSCAPS2_CUBEMAP_ALLFACES =
    DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX |
    DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY |
    DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ;
constexpr uint32 DDPF_FOURCC = 0x4;
constexpr uint32 DDPF_RGB = 0x40;
constexpr uint32 DDPF_RGBA = 0x41;

// FourCC codes
constexpr uint32 FOURCC_DXT1 = 0x31545844; // "DXT1"
constexpr uint32 FOURCC_DXT3 = 0x33545844; // "DXT3"
constexpr uint32 FOURCC_DXT5 = 0x35545844; // "DXT5"
constexpr uint32 FOURCC_DX10 = 0x30315844; // "DX10"

// DXGI formats (DX10 header)
constexpr uint32 DXGI_FORMAT_R16G16B16A16_FLOAT = 10;
constexpr uint32 DXGI_FORMAT_R16G16_FLOAT = 34;
constexpr uint32 DXGI_FORMAT_R32G32B32A32_FLOAT = 2;

// ============================================================================
// DDS Helper Functions
// ============================================================================

ImageData LoadImage(const std::string& filepath, int desiredChannels) {
    ImageData data;

    int width, height, channels;
    data.pixels = stbi_load(filepath.c_str(), &width, &height, &channels, desiredChannels);

    if (!data.pixels) {
        METAGFX_ERROR << "Failed to load image: " << filepath << " - " << stbi_failure_reason();
        return data;
    }

    data.width = static_cast<uint32>(width);
    data.height = static_cast<uint32>(height);
    data.channels = desiredChannels > 0 ? static_cast<uint32>(desiredChannels) : static_cast<uint32>(channels);

    METAGFX_INFO << "Loaded image: " << filepath << " (" << data.width << "x" << data.height << ", " << data.channels << " channels)";

    return data;
}

ImageData LoadImageFromMemory(const uint8* buffer, uint32 bufferSize, int desiredChannels) {
    ImageData data;

    int width, height, channels;
    data.pixels = stbi_load_from_memory(buffer, static_cast<int>(bufferSize), &width, &height, &channels, desiredChannels);

    if (!data.pixels) {
        METAGFX_ERROR << "Failed to load image from memory - " << stbi_failure_reason();
        return data;
    }

    data.width = static_cast<uint32>(width);
    data.height = static_cast<uint32>(height);
    data.channels = desiredChannels > 0 ? static_cast<uint32>(desiredChannels) : static_cast<uint32>(channels);

    METAGFX_INFO << "Loaded embedded image from memory (" << data.width << "x" << data.height << ", " << data.channels << " channels)";

    return data;
}

HDRImageData LoadHDRImage(const std::string& filepath, int desiredChannels) {
    HDRImageData data;

    int width, height, channels;
    data.pixels = stbi_loadf(filepath.c_str(), &width, &height, &channels, desiredChannels);

    if (!data.pixels) {
        METAGFX_ERROR << "Failed to load HDR image: " << filepath << " - " << stbi_failure_reason();
        return data;
    }

    data.width = static_cast<uint32>(width);
    data.height = static_cast<uint32>(height);
    data.channels = desiredChannels > 0 ? static_cast<uint32>(desiredChannels) : static_cast<uint32>(channels);

    METAGFX_INFO << "Loaded HDR image: " << filepath << " (" << data.width << "x" << data.height << ", " << data.channels << " channels)";

    return data;
}

void FreeImage(ImageData& data) {
    if (data.pixels) {
        stbi_image_free(data.pixels);
        data.pixels = nullptr;
        data.width = 0;
        data.height = 0;
        data.channels = 0;
    }
}

void FreeHDRImage(HDRImageData& data) {
    if (data.pixels) {
        stbi_image_free(data.pixels);
        data.pixels = nullptr;
        data.width = 0;
        data.height = 0;
        data.channels = 0;
    }
}

Ref<rhi::Texture> CreateTextureFromImage(
    rhi::GraphicsDevice* device,
    const ImageData& imageData,
    rhi::Format format
) {
    if (!imageData.pixels) {
        METAGFX_ERROR << "Cannot create texture from empty image data";
        return nullptr;
    }

    // Create texture descriptor
    rhi::TextureDesc desc;
    desc.width = imageData.width;
    desc.height = imageData.height;
    desc.format = format;
    desc.usage = rhi::TextureUsage::Sampled;

    // Create texture
    auto texture = device->CreateTexture(desc);

    // Upload pixel data
    uint64 imageSize = static_cast<uint64>(imageData.width) * imageData.height * imageData.channels;
    texture->UploadData(imageData.pixels, imageSize);

    return texture;
}

Ref<rhi::Texture> CreateTextureFromHDRImage(
    rhi::GraphicsDevice* device,
    const HDRImageData& imageData,
    rhi::Format format
) {
    if (!imageData.pixels) {
        METAGFX_ERROR << "Cannot create texture from empty HDR image data";
        return nullptr;
    }

    // Create texture descriptor
    rhi::TextureDesc desc;
    desc.width = imageData.width;
    desc.height = imageData.height;
    desc.format = format;
    desc.usage = rhi::TextureUsage::Sampled;

    // Create texture
    auto texture = device->CreateTexture(desc);

    // Upload pixel data (float data: 4 bytes per component)
    uint64 imageSize = static_cast<uint64>(imageData.width) * imageData.height * imageData.channels * sizeof(float);
    texture->UploadData(imageData.pixels, imageSize);

    return texture;
}

Ref<rhi::Texture> LoadTexture(
    rhi::GraphicsDevice* device,
    const std::string& filepath
) {
    // Load image data
    ImageData imageData = LoadImage(filepath, 4); // Force RGBA

    if (!imageData.pixels) {
        METAGFX_ERROR << "Failed to load texture from: " << filepath;
        return nullptr;
    }

    // Create texture from image data
    auto texture = CreateTextureFromImage(device, imageData, rhi::Format::R8G8B8A8_SRGB);

    // Free image data
    FreeImage(imageData);

    return texture;
}

Ref<rhi::Texture> LoadHDRTexture(
    rhi::GraphicsDevice* device,
    const std::string& filepath
) {
    // Load HDR image data
    HDRImageData imageData = LoadHDRImage(filepath, 4); // Force RGBA

    if (!imageData.pixels) {
        METAGFX_ERROR << "Failed to load HDR texture from: " << filepath;
        return nullptr;
    }

    // Create texture from HDR image data
    auto texture = CreateTextureFromHDRImage(device, imageData, rhi::Format::R16G16B16A16_SFLOAT);

    // Free HDR image data
    FreeHDRImage(imageData);

    return texture;
}

Ref<rhi::Texture> LoadDDS2DTexture(
    rhi::GraphicsDevice* device,
    const std::string& filepath
) {
    // Open file
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        METAGFX_ERROR << "Failed to open DDS file: " << filepath;
        return nullptr;
    }

    // Read magic number
    uint32 magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != DDS_MAGIC) {
        METAGFX_ERROR << "Invalid DDS file (bad magic number): " << filepath;
        return nullptr;
    }

    // Read DDS header
    DDSHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.size != 124) {
        METAGFX_ERROR << "Invalid DDS header size: " << filepath;
        return nullptr;
    }

    // Check if this is a 2D texture (not a cubemap)
    bool isCubemap = (header.caps2 & DDSCAPS2_CUBEMAP) != 0;
    if (isCubemap) {
        METAGFX_ERROR << "DDS file is a cubemap, not a 2D texture: " << filepath;
        return nullptr;
    }

    // Determine format
    rhi::Format format = rhi::Format::R8G8B8A8_UNORM;
    uint32 bytesPerPixel = 4;
    bool isDXT10 = false;

    if (header.ddspf.flags & DDPF_FOURCC) {
        if (header.ddspf.fourCC == FOURCC_DX10) {
            // DX10 extended header
            DDSHeaderDXT10 dx10Header;
            file.read(reinterpret_cast<char*>(&dx10Header), sizeof(dx10Header));
            isDXT10 = true;

            // Map DXGI format to our format
            if (dx10Header.dxgiFormat == DXGI_FORMAT_R16G16B16A16_FLOAT) {
                format = rhi::Format::R16G16B16A16_SFLOAT;
                bytesPerPixel = 8; // 4 channels * 2 bytes (float16)
            } else if (dx10Header.dxgiFormat == DXGI_FORMAT_R16G16_FLOAT) {
                format = rhi::Format::R16G16_SFLOAT;
                bytesPerPixel = 4; // 2 channels * 2 bytes (float16)
            } else if (dx10Header.dxgiFormat == DXGI_FORMAT_R32G32B32A32_FLOAT) {
                format = rhi::Format::R32G32B32A32_SFLOAT;
                bytesPerPixel = 16; // 4 channels * 4 bytes (float32)
            } else {
                METAGFX_ERROR << "Unsupported DXGI format in DDS file: " << dx10Header.dxgiFormat;
                return nullptr;
            }
        } else {
            METAGFX_ERROR << "Compressed DDS formats (DXT1/3/5) not yet supported";
            return nullptr;
        }
    } else if (header.ddspf.flags & DDPF_RGB) {
        // Uncompressed RGB/RGBA
        if (header.ddspf.RGBBitCount == 32) {
            format = rhi::Format::R8G8B8A8_UNORM;
            bytesPerPixel = 4;
        } else {
            METAGFX_ERROR << "Unsupported RGB bit count: " << header.ddspf.RGBBitCount;
            return nullptr;
        }
    }

    // Get dimensions and mip levels
    uint32 width = header.width;
    uint32 height = header.height;
    uint32 mipLevels = (header.flags & DDSD_MIPMAPCOUNT) ? header.mipMapCount : 1;

    METAGFX_INFO << "Loading DDS 2D texture: " << filepath;
    METAGFX_INFO << "  Dimensions: " << width << "x" << height;
    METAGFX_INFO << "  Mip levels: " << mipLevels;
    METAGFX_INFO << "  Format: " << static_cast<int>(format);

    // Calculate total data size
    uint64 totalSize = 0;
    for (uint32 mip = 0; mip < mipLevels; ++mip) {
        uint32 mipWidth = std::max(1u, width >> mip);
        uint32 mipHeight = std::max(1u, height >> mip);
        uint64 mipSize = static_cast<uint64>(mipWidth) * mipHeight * bytesPerPixel;
        totalSize += mipSize;
    }

    // Allocate buffer for all data
    std::vector<uint8> imageData(totalSize);
    file.read(reinterpret_cast<char*>(imageData.data()), totalSize);

    if (!file) {
        METAGFX_ERROR << "Failed to read DDS texture data from: " << filepath;
        return nullptr;
    }

    file.close();

    // Create texture descriptor
    rhi::TextureDesc desc;
    desc.type = rhi::TextureType::Texture2D;
    desc.width = width;
    desc.height = height;
    desc.mipLevels = mipLevels;
    desc.arrayLayers = 1;
    desc.format = format;
    desc.usage = rhi::TextureUsage::Sampled;

    // Create texture
    auto texture = device->CreateTexture(desc);

    // Upload data
    texture->UploadData(imageData.data(), totalSize);

    METAGFX_INFO << "Successfully loaded DDS 2D texture: " << filepath;

    return texture;
}

Ref<rhi::Texture> LoadDDSCubemap(
    rhi::GraphicsDevice* device,
    const std::string& filepath
) {
    // Open file
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        METAGFX_ERROR << "Failed to open DDS file: " << filepath;
        return nullptr;
    }

    // Read magic number
    uint32 magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != DDS_MAGIC) {
        METAGFX_ERROR << "Invalid DDS file (bad magic number): " << filepath;
        return nullptr;
    }

    // Read DDS header
    DDSHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.size != 124) {
        METAGFX_ERROR << "Invalid DDS header size: " << filepath;
        return nullptr;
    }

    // Check if this is a cubemap
    bool isCubemap = (header.caps2 & DDSCAPS2_CUBEMAP) != 0;
    if (!isCubemap) {
        METAGFX_ERROR << "DDS file is not a cubemap: " << filepath;
        return nullptr;
    }

    // Verify all 6 faces are present
    if ((header.caps2 & DDSCAPS2_CUBEMAP_ALLFACES) != DDSCAPS2_CUBEMAP_ALLFACES) {
        METAGFX_ERROR << "DDS cubemap does not contain all 6 faces: " << filepath;
        return nullptr;
    }

    // Determine format
    rhi::Format format = rhi::Format::R8G8B8A8_UNORM;
    uint32 bytesPerPixel = 4;
    bool isDXT10 = false;

    if (header.ddspf.flags & DDPF_FOURCC) {
        if (header.ddspf.fourCC == FOURCC_DX10) {
            // DX10 extended header
            DDSHeaderDXT10 dx10Header;
            file.read(reinterpret_cast<char*>(&dx10Header), sizeof(dx10Header));
            isDXT10 = true;

            // Map DXGI format to our format
            if (dx10Header.dxgiFormat == DXGI_FORMAT_R16G16B16A16_FLOAT) {
                format = rhi::Format::R16G16B16A16_SFLOAT;
                bytesPerPixel = 8; // 4 channels * 2 bytes (float16)
            } else if (dx10Header.dxgiFormat == DXGI_FORMAT_R32G32B32A32_FLOAT) {
                format = rhi::Format::R32G32B32A32_SFLOAT;
                bytesPerPixel = 16; // 4 channels * 4 bytes (float32)
            } else {
                METAGFX_ERROR << "Unsupported DXGI format in DDS file: " << dx10Header.dxgiFormat;
                return nullptr;
            }
        } else {
            METAGFX_ERROR << "Compressed DDS formats (DXT1/3/5) not yet supported for cubemaps";
            return nullptr;
        }
    } else if (header.ddspf.flags & DDPF_RGB) {
        // Uncompressed RGB/RGBA
        if (header.ddspf.RGBBitCount == 32) {
            format = rhi::Format::R8G8B8A8_UNORM;
            bytesPerPixel = 4;
        } else {
            METAGFX_ERROR << "Unsupported RGB bit count: " << header.ddspf.RGBBitCount;
            return nullptr;
        }
    }

    // Get dimensions and mip levels
    uint32 width = header.width;
    uint32 height = header.height;
    uint32 mipLevels = (header.flags & DDSD_MIPMAPCOUNT) ? header.mipMapCount : 1;

    METAGFX_INFO << "Loading DDS cubemap: " << filepath;
    METAGFX_INFO << "  Dimensions: " << width << "x" << height;
    METAGFX_INFO << "  Mip levels: " << mipLevels;
    METAGFX_INFO << "  Format: " << static_cast<int>(format);

    // Calculate total data size
    uint64 totalSize = 0;
    for (uint32 mip = 0; mip < mipLevels; ++mip) {
        uint32 mipWidth = std::max(1u, width >> mip);
        uint32 mipHeight = std::max(1u, height >> mip);
        uint64 mipSize = static_cast<uint64>(mipWidth) * mipHeight * bytesPerPixel;
        totalSize += mipSize * 6; // 6 faces
    }

    // Allocate buffer for all data
    std::vector<uint8> imageData(totalSize);
    file.read(reinterpret_cast<char*>(imageData.data()), totalSize);

    if (!file) {
        METAGFX_ERROR << "Failed to read DDS cubemap data from: " << filepath;
        return nullptr;
    }

    file.close();

    // Create texture descriptor
    rhi::TextureDesc desc;
    desc.type = rhi::TextureType::TextureCube;
    desc.width = width;
    desc.height = height;
    desc.mipLevels = mipLevels;
    desc.arrayLayers = 6; // Cubemap has 6 faces
    desc.format = format;
    desc.usage = rhi::TextureUsage::Sampled;

    // Create texture
    auto texture = device->CreateTexture(desc);

    // Upload data
    texture->UploadData(imageData.data(), totalSize);

    METAGFX_INFO << "Successfully loaded DDS cubemap: " << filepath;

    return texture;
}

} // namespace utils
} // namespace metagfx
