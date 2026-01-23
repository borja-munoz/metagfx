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

    // WebGPU-specific
    wgpu::SwapChain GetHandle() const { return m_SwapChain; }

private:
    void CreateSwapChain();

    WebGPUContext& m_Context;
    SDL_Window* m_Window;

    wgpu::SwapChain m_SwapChain = nullptr;
    Ref<Texture> m_CurrentTexture;

    uint32 m_Width = 0;
    uint32 m_Height = 0;
    Format m_Format = Format::B8G8R8A8_UNORM;
};

} // namespace rhi
} // namespace metagfx
