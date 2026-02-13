// ============================================================================
// src/rhi/webgpu/WebGPUSwapChain.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUSwapChain.h"
#include "metagfx/rhi/webgpu/WebGPUTexture.h"
#include "metagfx/core/Logger.h"

#include <SDL3/SDL.h>

namespace metagfx {
namespace rhi {

WebGPUSwapChain::WebGPUSwapChain(WebGPUContext& context, SDL_Window* window)
    : m_Context(context)
    , m_Window(window) {

    // Get window size
    int w, h;
    SDL_GetWindowSize(m_Window, &w, &h);
    m_Width = static_cast<uint32>(w);
    m_Height = static_cast<uint32>(h);

    ConfigureSurface();
}

WebGPUSwapChain::~WebGPUSwapChain() {
    // Surface configuration is managed by the device/surface
    // No explicit cleanup needed
}

void WebGPUSwapChain::ConfigureSurface() {
    // Modern Dawn API uses Surface::Configure instead of CreateSwapChain
    // Default to BGRA8Unorm which is widely supported
    wgpu::TextureFormat preferredFormat = wgpu::TextureFormat::BGRA8Unorm;
    m_Format = Format::B8G8R8A8_UNORM;

    // Configure surface using modern API
    WGPUSurfaceConfiguration config{};
    config.device = m_Context.device.Get();
    config.format = static_cast<WGPUTextureFormat>(preferredFormat);
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = m_Width;
    config.height = m_Height;
    config.presentMode = WGPUPresentMode_Fifo;  // VSync
    config.alphaMode = WGPUCompositeAlphaMode_Opaque;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;

    WGPUSurface surface = m_Context.surface.Get();
    wgpuSurfaceConfigure(surface, &config);

    METAGFX_INFO << "WebGPU surface configured: " << m_Width << "x" << m_Height
                    << ", format=" << static_cast<int>(preferredFormat);
}

Ref<Texture> WebGPUSwapChain::GetCurrentBackBuffer() {
    // Get current texture from surface using modern API
    WGPUSurfaceTexture surfaceTexture{};
    WGPUSurface surface = m_Context.surface.Get();
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

    // Check status - SuccessOptimal is the best case, Suboptimal is acceptable
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        METAGFX_ERROR << "Failed to get current surface texture, status=" << static_cast<int>(surfaceTexture.status);
        return nullptr;
    }

    if (!surfaceTexture.texture) {
        METAGFX_ERROR << "Surface texture is null";
        return nullptr;
    }

    // Wrap the surface texture in a WebGPUTexture
    // Note: The texture is owned by the surface and will be released on present
    TextureDesc desc{};
    desc.width = m_Width;
    desc.height = m_Height;
    desc.depth = 1;
    desc.mipLevels = 1;
    desc.format = m_Format;
    desc.type = TextureType::Texture2D;
    desc.usage = TextureUsage::ColorAttachment;
    desc.debugName = "SwapChain BackBuffer";

    // Wrap the surface texture (don't create a new one!)
    auto texture = CreateRef<WebGPUTexture>(m_Context, surfaceTexture.texture, desc);

    m_CurrentTexture = texture;
    m_CurrentSurfaceTexture = wgpu::Texture::Acquire(surfaceTexture.texture);
    return m_CurrentTexture;
}

void WebGPUSwapChain::Present() {
    // Modern Dawn API uses wgpuSurfacePresent to present the surface texture
    WGPUSurface surface = m_Context.surface.Get();
    WGPUStatus status = wgpuSurfacePresent(surface);

    if (status != WGPUStatus_Success) {
        METAGFX_ERROR << "Failed to present surface, status=" << static_cast<int>(status);
    }

    // Release the current surface texture references
    m_CurrentTexture = nullptr;  // Release our wrapper
    m_CurrentSurfaceTexture = nullptr;
}

void WebGPUSwapChain::Resize(uint32 width, uint32 height) {
    if (width == m_Width && height == m_Height) {
        return;  // No resize needed
    }

    m_Width = width;
    m_Height = height;

    // Release current texture references
    m_CurrentSurfaceTexture = nullptr;
    m_CurrentTexture = nullptr;

    // Reconfigure surface with new dimensions
    ConfigureSurface();

    METAGFX_INFO << "WebGPU surface resized: " << m_Width << "x" << m_Height;
}

} // namespace rhi
} // namespace metagfx
