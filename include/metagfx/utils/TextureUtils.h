// ============================================================================
// include/metagfx/utils/TextureUtils.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Texture.h"
#include <string>

namespace metagfx {
namespace utils {

struct ImageData {
    uint8* pixels = nullptr;
    uint32 width = 0;
    uint32 height = 0;
    uint32 channels = 0;
};

struct HDRImageData {
    float* pixels = nullptr;
    uint32 width = 0;
    uint32 height = 0;
    uint32 channels = 0;
};

// Load image using stb_image
// desiredChannels: Number of channels to force (0 = use image's channels, 4 = force RGBA)
ImageData LoadImage(const std::string& filepath, int desiredChannels = 4);

// Load HDR image using stb_image (floating-point format for .hdr files)
// desiredChannels: Number of channels to force (0 = use image's channels, 4 = force RGBA)
HDRImageData LoadHDRImage(const std::string& filepath, int desiredChannels = 4);

// Load image from memory buffer (for embedded textures)
ImageData LoadImageFromMemory(const uint8* buffer, uint32 bufferSize, int desiredChannels = 4);

// Free stb_image data
void FreeImage(ImageData& data);

// Free stb_image HDR data
void FreeHDRImage(HDRImageData& data);

// Create texture from image data (loads to GPU)
Ref<rhi::Texture> CreateTextureFromImage(
    rhi::GraphicsDevice* device,
    const ImageData& imageData,
    rhi::Format format = rhi::Format::R8G8B8A8_SRGB
);

// Create texture from HDR image data (floating-point format)
Ref<rhi::Texture> CreateTextureFromHDRImage(
    rhi::GraphicsDevice* device,
    const HDRImageData& imageData,
    rhi::Format format = rhi::Format::R16G16B16A16_SFLOAT
);

// Convenience: load file and create texture
Ref<rhi::Texture> LoadTexture(
    rhi::GraphicsDevice* device,
    const std::string& filepath
);

// Convenience: load HDR file and create HDR texture
Ref<rhi::Texture> LoadHDRTexture(
    rhi::GraphicsDevice* device,
    const std::string& filepath
);

// DDS 2D texture loading (for IBL BRDF LUT)
// Supports 2D textures in DDS format with float16/float32 formats
Ref<rhi::Texture> LoadDDS2DTexture(
    rhi::GraphicsDevice* device,
    const std::string& filepath
);

// DDS cubemap loading (for IBL pre-baked textures)
// Supports cubemaps with mipmap chains in DDS format
Ref<rhi::Texture> LoadDDSCubemap(
    rhi::GraphicsDevice* device,
    const std::string& filepath
);

} // namespace utils
} // namespace metagfx
