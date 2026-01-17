// ============================================================================
// include/metagfx/rhi/metal/MetalSwapChain.h
// ============================================================================
#pragma once

#include "metagfx/rhi/SwapChain.h"
#include "MetalTypes.h"

#include <dispatch/dispatch.h>

struct SDL_Window;

namespace metagfx {
namespace rhi {

class MetalSwapChain : public SwapChain {
public:
    MetalSwapChain(MetalContext& context, SDL_Window* window);
    ~MetalSwapChain() override;

    void Present() override;
    void Resize(uint32 width, uint32 height) override;

    Ref<Texture> GetCurrentBackBuffer() override;
    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Format GetFormat() const override { return m_Format; }

    // Metal-specific
    CA::MetalDrawable* GetCurrentDrawable() const { return m_CurrentDrawable; }
    dispatch_semaphore_t GetFrameSemaphore() const { return m_FrameSemaphore; }

    uint32 GetCurrentFrame() const { return m_CurrentFrame; }
    void AdvanceFrame();

private:
    void AcquireNextDrawable();
    void UpdateDrawableSize();

    MetalContext& m_Context;
    SDL_Window* m_Window;

    CA::MetalDrawable* m_CurrentDrawable = nullptr;
    dispatch_semaphore_t m_FrameSemaphore = nullptr;

    Ref<Texture> m_CurrentTexture;

    uint32 m_Width = 0;
    uint32 m_Height = 0;
    Format m_Format = Format::B8G8R8A8_UNORM;

    // Frame synchronization
    static constexpr uint32 MAX_FRAMES_IN_FLIGHT = 2;
    uint32 m_CurrentFrame = 0;
};

} // namespace rhi
} // namespace metagfx
