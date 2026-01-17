// ============================================================================
// include/metagfx/rhi/metal/MetalTexture.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Texture.h"
#include "MetalTypes.h"

namespace metagfx {
namespace rhi {

class MetalTexture : public Texture {
public:
    // For swap chain textures (borrowed, don't own)
    MetalTexture(MetalContext& context, MTL::Texture* texture,
                 uint32 width, uint32 height, MTL::PixelFormat format);

    // For created textures (owned)
    MetalTexture(MetalContext& context, const TextureDesc& desc);

    ~MetalTexture() override;

    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Format GetFormat() const override { return m_Format; }

    void UploadData(const void* data, uint64 size) override;

    // Metal-specific
    MTL::Texture* GetHandle() const { return m_Texture; }

private:
    MetalContext& m_Context;
    MTL::Texture* m_Texture = nullptr;

    uint32 m_Width = 0;
    uint32 m_Height = 0;
    uint32 m_MipLevels = 1;
    uint32 m_ArrayLayers = 1;
    Format m_Format = Format::Undefined;
    TextureType m_Type = TextureType::Texture2D;
    bool m_OwnsTexture = false;
};

} // namespace rhi
} // namespace metagfx
