// ============================================================================
// src/rhi/metal/MetalSwapChain.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalSwapChain.h"
#include "metagfx/rhi/metal/MetalTexture.h"
#include "MetalSDLBridge.h"

#include <SDL3/SDL.h>

namespace metagfx {
namespace rhi {

MetalSwapChain::MetalSwapChain(MetalContext& context, SDL_Window* window)
    : m_Context(context)
    , m_Window(window) {

    // Create semaphore for limiting frames in flight
    m_FrameSemaphore = dispatch_semaphore_create(MAX_FRAMES_IN_FLIGHT);

    // Get initial window size
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    m_Width = static_cast<uint32>(w);
    m_Height = static_cast<uint32>(h);

    // Configure the Metal layer
    UpdateDrawableSize();

    // Set format based on layer pixel format (via bridge function)
    m_Format = FromMetalPixelFormat(GetMetalLayerPixelFormat(m_Context.metalLayer));

    METAGFX_INFO << "Metal swap chain created: " << m_Width << "x" << m_Height;
}

MetalSwapChain::~MetalSwapChain() {
    // Wait for all frames to complete
    for (uint32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        dispatch_semaphore_wait(m_FrameSemaphore, DISPATCH_TIME_FOREVER);
    }

    // Release the semaphore signals we just acquired
    for (uint32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        dispatch_semaphore_signal(m_FrameSemaphore);
    }

    m_CurrentDrawable = nullptr;
    m_CurrentTexture.reset();

    METAGFX_INFO << "Metal swap chain destroyed";
}

void MetalSwapChain::Present() {
    // Presentation is handled by MetalDevice::SubmitCommandBuffer
    // which calls presentDrawable on the command buffer

    // The drawable is automatically released after presentation
    m_CurrentDrawable = nullptr;
    m_CurrentTexture.reset();
}

void MetalSwapChain::Resize(uint32 width, uint32 height) {
    if (width == 0 || height == 0) {
        return;
    }

    m_Width = width;
    m_Height = height;

    UpdateDrawableSize();

    // Clear current drawable as it's now invalid
    m_CurrentDrawable = nullptr;
    m_CurrentTexture.reset();

    METAGFX_DEBUG << "Metal swap chain resized: " << m_Width << "x" << m_Height;
}

Ref<Texture> MetalSwapChain::GetCurrentBackBuffer() {
    // Acquire drawable if we don't have one
    if (!m_CurrentDrawable) {
        AcquireNextDrawable();
    }

    // Create texture wrapper if needed
    if (!m_CurrentTexture && m_CurrentDrawable) {
        MTL::Texture* texture = GetDrawableTexture(m_CurrentDrawable);
        MTL::PixelFormat pixelFormat = GetMetalLayerPixelFormat(m_Context.metalLayer);
        m_CurrentTexture = CreateRef<MetalTexture>(
            m_Context,
            texture,
            m_Width,
            m_Height,
            pixelFormat
        );
    }

    return m_CurrentTexture;
}

void MetalSwapChain::AcquireNextDrawable() {
    // Wait for a frame slot to be available
    dispatch_semaphore_wait(m_FrameSemaphore, DISPATCH_TIME_FOREVER);

    // Get the next drawable from the layer (via bridge function)
    m_CurrentDrawable = GetNextDrawable(m_Context.metalLayer);

    if (!m_CurrentDrawable) {
        METAGFX_ERROR << "Failed to acquire next drawable";
        // Signal the semaphore back since we didn't use a frame slot
        dispatch_semaphore_signal(m_FrameSemaphore);
    }
}

void MetalSwapChain::UpdateDrawableSize() {
    // Update the drawable size on the Metal layer (via bridge function)
    SetMetalLayerDrawableSize(m_Context.metalLayer, m_Width, m_Height);
}

void MetalSwapChain::AdvanceFrame() {
    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace rhi
} // namespace metagfx
