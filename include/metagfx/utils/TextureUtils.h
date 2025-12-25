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

// Load image using stb_image
// desiredChannels: Number of channels to force (0 = use image's channels, 4 = force RGBA)
ImageData LoadImage(const std::string& filepath, int desiredChannels = 4);

// Load image from memory buffer (for embedded textures)
ImageData LoadImageFromMemory(const uint8* buffer, uint32 bufferSize, int desiredChannels = 4);

// Free stb_image data
void FreeImage(ImageData& data);

// Create texture from image data (loads to GPU)
Ref<rhi::Texture> CreateTextureFromImage(
    rhi::GraphicsDevice* device,
    const ImageData& imageData,
    rhi::Format format = rhi::Format::R8G8B8A8_SRGB
);

// Convenience: load file and create texture
Ref<rhi::Texture> LoadTexture(
    rhi::GraphicsDevice* device,
    const std::string& filepath
);

} // namespace utils
} // namespace metagfx
