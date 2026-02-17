// ============================================================================
// src/utils/TextureUtils.cpp
// ============================================================================
#include "metagfx/utils/TextureUtils.h"
#include "metagfx/core/Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

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

// ============================================================================
// PFM + Equirectangular-to-Cubemap
// ============================================================================

HDRImageData LoadPFMImage(const std::string& filepath) {
    HDRImageData data;
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        METAGFX_ERROR << "Failed to open PFM file: " << filepath;
        return data;
    }

    // Parse header (3 text lines)
    std::string magic, dims, scaleStr;
    std::getline(file, magic);
    std::getline(file, dims);
    std::getline(file, scaleStr);

    // Strip carriage returns (Windows line endings)
    auto strip = [](std::string& s) {
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    };
    strip(magic); strip(dims); strip(scaleStr);

    if (magic != "PF") {
        METAGFX_ERROR << "Unsupported PFM type '" << magic << "' (only color 'PF' supported): " << filepath;
        return data;
    }

    int width = 0, height = 0;
    std::istringstream dimStream(dims);
    dimStream >> width >> height;
    if (width <= 0 || height <= 0) {
        METAGFX_ERROR << "Invalid PFM dimensions in: " << filepath;
        return data;
    }

    // PFM stores rows bottom-to-top; negative scale = little-endian (native on x86)
    int numPixels = width * height;
    std::vector<float> rawRGB(static_cast<size_t>(numPixels) * 3);
    file.read(reinterpret_cast<char*>(rawRGB.data()),
              static_cast<std::streamsize>(numPixels) * 3 * static_cast<std::streamsize>(sizeof(float)));
    if (!file) {
        METAGFX_ERROR << "Failed to read PFM pixel data from: " << filepath;
        return data;
    }

    // Allocate RGBA float output (compatible with stbi_image_free / free())
    float* pixels = static_cast<float*>(malloc(static_cast<size_t>(numPixels) * 4 * sizeof(float)));
    if (!pixels) { METAGFX_ERROR << "Out of memory loading PFM"; return data; }

    // Flip vertically (PFM is bottom-to-top → top-to-bottom for GPU)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int src = ((height - 1 - y) * width + x) * 3;
            int dst = (y * width + x) * 4;
            pixels[dst + 0] = rawRGB[static_cast<size_t>(src + 0)];
            pixels[dst + 1] = rawRGB[static_cast<size_t>(src + 1)];
            pixels[dst + 2] = rawRGB[static_cast<size_t>(src + 2)];
            pixels[dst + 3] = 1.0f;
        }
    }

    data.pixels   = pixels;
    data.width    = static_cast<uint32>(width);
    data.height   = static_cast<uint32>(height);
    data.channels = 4;
    METAGFX_INFO << "Loaded PFM: " << filepath << " (" << width << "x" << height << ")";
    return data;
}

namespace {

// Convert IEEE 754 float32 to float16 (round towards zero, clamp to ±inf).
static uint16_t FloatToHalf(float f) {
    uint32_t x;
    memcpy(&x, &f, sizeof(x));
    uint32_t sign     = (x >> 31) & 1u;
    int32_t  exp32    = static_cast<int32_t>((x >> 23) & 0xFFu) - 127;
    uint32_t mantissa = x & 0x7FFFFFu;
    if (exp32 >= 16)  return static_cast<uint16_t>((sign << 15) | 0x7C00u);  // overflow → ±inf
    if (exp32 < -14)  return static_cast<uint16_t>(sign << 15);              // underflow → ±0
    int32_t exp16 = exp32 + 15;
    return static_cast<uint16_t>((sign << 15) | (static_cast<uint32_t>(exp16) << 10) | (mantissa >> 13));
}

// Sample equirectangular RGBA float image at a world-space direction.
static glm::vec3 SampleEquirect(const float* pixels, uint32 w, uint32 h, glm::vec3 dir) {
    constexpr float PI = 3.14159265358979323846f;
    float phi   = std::atan2(dir.z, dir.x);                             // [-pi, pi]
    float theta = std::acos(glm::clamp(dir.y, -1.0f, 1.0f));           // [0, pi]
    float u     = phi / (2.0f * PI) + 0.5f;                            // [0, 1]
    float v     = theta / PI;                                           // [0, 1]

    float px = u * static_cast<float>(w - 1);
    float py = v * static_cast<float>(h - 1);
    int x0 = static_cast<int>(px);
    int y0 = static_cast<int>(py);
    int x1 = (x0 + 1) % static_cast<int>(w);   // horizontal wrap
    int y1 = std::min(y0 + 1, static_cast<int>(h) - 1);
    float fx = px - static_cast<float>(x0);
    float fy = py - static_cast<float>(y0);

    auto get = [&](int x, int y) -> glm::vec3 {
        size_t idx = static_cast<size_t>(y * static_cast<int>(w) + x) * 4;
        return glm::vec3(pixels[idx], pixels[idx + 1], pixels[idx + 2]);
    };
    return glm::mix(glm::mix(get(x0, y0), get(x1, y0), fx),
                    glm::mix(get(x0, y1), get(x1, y1), fx), fy);
}

// Map cubemap face + pixel UV → world-space direction (OpenGL / DDS convention: +X,-X,+Y,-Y,+Z,-Z).
static glm::vec3 CubemapFaceDir(int face, float u, float v) {
    float s = 2.0f * u - 1.0f;
    float t = 2.0f * v - 1.0f;
    switch (face) {
        case 0: return glm::normalize(glm::vec3( 1.0f, -t,   -s  ));  // +X
        case 1: return glm::normalize(glm::vec3(-1.0f, -t,    s  ));  // -X
        case 2: return glm::normalize(glm::vec3( s,     1.0f,  t ));  // +Y
        case 3: return glm::normalize(glm::vec3( s,    -1.0f, -t ));  // -Y
        case 4: return glm::normalize(glm::vec3( s,    -t,    1.0f)); // +Z
        default:return glm::normalize(glm::vec3(-s,    -t,   -1.0f)); // -Z
    }
}

} // anonymous namespace

Ref<rhi::Texture> LoadCubemapFromEquirectangular(
    rhi::GraphicsDevice* device,
    const HDRImageData&  equirectangular,
    uint32               faceSize
) {
    if (!equirectangular.pixels || equirectangular.width == 0 || equirectangular.height == 0) {
        METAGFX_ERROR << "LoadCubemapFromEquirectangular: invalid source image";
        return nullptr;
    }

    // Rasterise all 6 faces into a contiguous RGBA16F buffer (face-major order).
    // R16G16B16A16_SFLOAT is filterable on all backends including WebGPU (no optional feature needed).
    const size_t halfsPerFace = static_cast<size_t>(faceSize) * faceSize * 4;
    std::vector<uint16_t> cubeData(6 * halfsPerFace);

    for (int face = 0; face < 6; ++face) {
        uint16_t* dst = cubeData.data() + face * halfsPerFace;
        for (uint32 y = 0; y < faceSize; ++y) {
            for (uint32 x = 0; x < faceSize; ++x) {
                float u   = (static_cast<float>(x) + 0.5f) / static_cast<float>(faceSize);
                float v   = (static_cast<float>(y) + 0.5f) / static_cast<float>(faceSize);
                glm::vec3 dir = CubemapFaceDir(face, u, v);
                glm::vec3 col = SampleEquirect(
                    equirectangular.pixels, equirectangular.width, equirectangular.height, dir);
                size_t idx = (static_cast<size_t>(y) * faceSize + x) * 4;
                dst[idx + 0] = FloatToHalf(col.r);
                dst[idx + 1] = FloatToHalf(col.g);
                dst[idx + 2] = FloatToHalf(col.b);
                dst[idx + 3] = FloatToHalf(1.0f);
            }
        }
    }

    rhi::TextureDesc desc;
    desc.type        = rhi::TextureType::TextureCube;
    desc.width       = faceSize;
    desc.height      = faceSize;
    desc.mipLevels   = 1;
    desc.arrayLayers = 6;
    desc.format      = rhi::Format::R16G16B16A16_SFLOAT;
    desc.usage       = rhi::TextureUsage::Sampled;

    auto texture = device->CreateTexture(desc);
    texture->UploadData(cubeData.data(), cubeData.size() * sizeof(uint16_t));

    METAGFX_INFO << "Created " << faceSize << "x" << faceSize
                 << " cubemap from equirectangular env map";
    return texture;
}

Ref<rhi::Texture> ComputeIrradianceCubemap(
    rhi::GraphicsDevice* device,
    const HDRImageData&  equirectangular,
    uint32               faceSize
) {
    if (!equirectangular.pixels || equirectangular.width == 0 || equirectangular.height == 0) {
        METAGFX_ERROR << "ComputeIrradianceCubemap: invalid source image";
        return nullptr;
    }

    // Cosine-weighted hemisphere sampling using the Fibonacci spiral.
    // E(n) ≈ (π / N) * Σ L(wᵢ)  for N cosine-weighted samples.
    constexpr int   N    = 128;
    constexpr float PI   = 3.14159265358979323846f;
    constexpr float PHI  = 1.61803398874989485f;   // golden ratio

    // Store as RGBA16F — filterable on all WebGPU devices without optional features.
    const size_t halfsPerFace = static_cast<size_t>(faceSize) * faceSize * 4;
    std::vector<uint16_t> cubeData(6 * halfsPerFace, 0u);
    // Compute into a temporary float buffer then convert to fp16 per face.
    const size_t floatsPerFace = halfsPerFace;
    std::vector<float> floatFace(floatsPerFace);

    for (int face = 0; face < 6; ++face) {
        uint16_t* dst = cubeData.data() + face * halfsPerFace;

        for (uint32 y = 0; y < faceSize; ++y) {
            for (uint32 x = 0; x < faceSize; ++x) {
                // World-space normal for this texel
                float uv = (static_cast<float>(x) + 0.5f) / static_cast<float>(faceSize);
                float vv = (static_cast<float>(y) + 0.5f) / static_cast<float>(faceSize);
                glm::vec3 N_dir = CubemapFaceDir(face, uv, vv);

                // Build an orthonormal tangent frame around N_dir
                glm::vec3 up    = (std::abs(N_dir.y) < 0.999f)
                                      ? glm::vec3(0.0f, 1.0f, 0.0f)
                                      : glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 right = glm::normalize(glm::cross(up, N_dir));
                up              = glm::cross(N_dir, right);

                // Accumulate cosine-weighted hemisphere samples
                glm::vec3 irradiance(0.0f);
                for (int i = 0; i < N; ++i) {
                    // Cosine-weighted distribution: cosTheta = sqrt(1 - ξ)
                    float xi       = (static_cast<float>(i) + 0.5f) / static_cast<float>(N);
                    float cosTheta = std::sqrt(1.0f - xi);
                    float sinTheta = std::sqrt(xi);
                    float phi      = 2.0f * PI * static_cast<float>(i) / PHI;

                    glm::vec3 localDir = glm::vec3(
                        sinTheta * std::cos(phi),
                        sinTheta * std::sin(phi),
                        cosTheta);
                    glm::vec3 worldDir = glm::normalize(
                        localDir.x * right + localDir.y * up + localDir.z * N_dir);

                    irradiance += SampleEquirect(
                        equirectangular.pixels,
                        equirectangular.width,
                        equirectangular.height,
                        worldDir);
                }
                // For cosine-weighted sampling: E = (π / N) * Σ L
                irradiance *= PI / static_cast<float>(N);

                size_t idx       = (static_cast<size_t>(y) * faceSize + x) * 4;
                floatFace[idx + 0] = irradiance.r;
                floatFace[idx + 1] = irradiance.g;
                floatFace[idx + 2] = irradiance.b;
                floatFace[idx + 3] = 1.0f;
            }
        }
        // Convert float32 face to float16
        for (size_t i = 0; i < floatsPerFace; ++i)
            dst[i] = FloatToHalf(floatFace[i]);
    }

    rhi::TextureDesc desc;
    desc.type        = rhi::TextureType::TextureCube;
    desc.width       = faceSize;
    desc.height      = faceSize;
    desc.mipLevels   = 1;
    desc.arrayLayers = 6;
    desc.format      = rhi::Format::R16G16B16A16_SFLOAT;
    desc.usage       = rhi::TextureUsage::Sampled;

    auto texture = device->CreateTexture(desc);
    texture->UploadData(cubeData.data(), cubeData.size() * sizeof(uint16_t));

    METAGFX_INFO << "Computed irradiance cubemap (" << faceSize << "x" << faceSize
                 << " per face, " << N << " samples/texel)";
    return texture;
}

} // namespace utils
} // namespace metagfx
