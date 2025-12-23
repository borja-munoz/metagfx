// ============================================================================
// src/utils/TextureUtils.cpp
// ============================================================================
#include "metagfx/utils/TextureUtils.h"
#include "metagfx/core/Logger.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

namespace metagfx {
namespace utils {

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

void FreeImage(ImageData& data) {
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

} // namespace utils
} // namespace metagfx
