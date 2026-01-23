// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUTexture.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Texture.h"
#include "WebGPUTypes.h"

namespace metagfx {
namespace rhi {

class WebGPUTexture : public Texture {
public:
    WebGPUTexture(WebGPUContext& context, const TextureDesc& desc);
    ~WebGPUTexture() override;

    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Format GetFormat() const override { return m_Format; }

    void UploadData(const void* data, uint64 size) override;

    // WebGPU-specific
    wgpu::Texture GetHandle() const { return m_Texture; }
    wgpu::TextureView GetView() const { return m_TextureView; }

private:
    void CreateTexture(const TextureDesc& desc);
    void CreateTextureView();

    WebGPUContext& m_Context;
    wgpu::Texture m_Texture = nullptr;
    wgpu::TextureView m_TextureView = nullptr;

    uint32 m_Width = 0;
    uint32 m_Height = 0;
    uint32 m_Depth = 1;
    uint32 m_MipLevels = 1;
    Format m_Format = Format::Undefined;
    TextureType m_Type = TextureType::Texture2D;
    TextureUsage m_Usage;
};

} // namespace rhi
} // namespace metagfx
