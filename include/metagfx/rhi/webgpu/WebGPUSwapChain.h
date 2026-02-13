// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUSwapChain.h
// ============================================================================
#pragma once

#include "metagfx/rhi/SwapChain.h"
#include "WebGPUTypes.h"

struct SDL_Window;

namespace metagfx {
namespace rhi {

class WebGPUSwapChain : public SwapChain {
public:
    WebGPUSwapChain(WebGPUContext& context, SDL_Window* window);
    ~WebGPUSwapChain() override;

    void Present() override;
    void Resize(uint32 width, uint32 height) override;

    Ref<Texture> GetCurrentBackBuffer() override;
    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Format GetFormat() const override { return m_Format; }

private:
    void ConfigureSurface();

    WebGPUContext& m_Context;
    SDL_Window* m_Window;

    // Modern Dawn API uses Surface::Configure instead of SwapChain
    wgpu::Texture m_CurrentSurfaceTexture = nullptr;
    Ref<Texture> m_CurrentTexture;

    uint32 m_Width = 0;
    uint32 m_Height = 0;
    Format m_Format = Format::B8G8R8A8_UNORM;
};

} // namespace rhi
} // namespace metagfx
