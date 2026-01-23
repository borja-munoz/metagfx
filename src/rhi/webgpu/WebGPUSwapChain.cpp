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

    CreateSwapChain();
}

WebGPUSwapChain::~WebGPUSwapChain() {
    m_SwapChain = nullptr;
}

void WebGPUSwapChain::CreateSwapChain() {
    // Query preferred swap chain format
    wgpu::TextureFormat preferredFormat = m_Context.surface.GetPreferredFormat(m_Context.adapter);

    // Map to MetaGFX format
    if (preferredFormat == wgpu::TextureFormat::BGRA8Unorm ||
        preferredFormat == wgpu::TextureFormat::BGRA8UnormSrgb) {
        m_Format = Format::B8G8R8A8_UNORM;
    } else if (preferredFormat == wgpu::TextureFormat::RGBA8Unorm ||
               preferredFormat == wgpu::TextureFormat::RGBA8UnormSrgb) {
        m_Format = Format::R8G8B8A8_UNORM;
    } else {
        WEBGPU_LOG_WARNING("Unsupported swap chain format, defaulting to BGRA8");
        preferredFormat = wgpu::TextureFormat::BGRA8Unorm;
        m_Format = Format::B8G8R8A8_UNORM;
    }

    // Create swap chain descriptor
    wgpu::SwapChainDescriptor swapChainDesc{};
    swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
    swapChainDesc.format = preferredFormat;
    swapChainDesc.width = m_Width;
    swapChainDesc.height = m_Height;
    swapChainDesc.presentMode = wgpu::PresentMode::Fifo;  // VSync

    m_SwapChain = m_Context.device.CreateSwapChain(m_Context.surface, &swapChainDesc);

    if (!m_SwapChain) {
        WEBGPU_LOG_ERROR("Failed to create swap chain");
        throw std::runtime_error("Failed to create WebGPU swap chain");
    }

    WEBGPU_LOG_INFO("WebGPU swap chain created: " << m_Width << "x" << m_Height
                    << ", format=" << static_cast<int>(preferredFormat));
}

Ref<Texture> WebGPUSwapChain::GetCurrentBackBuffer() {
    // Get current texture view from swap chain
    wgpu::TextureView currentView = m_SwapChain.GetCurrentTextureView();

    if (!currentView) {
        WEBGPU_LOG_ERROR("Failed to get current swap chain texture view");
        return nullptr;
    }

    // Wrap in a WebGPU texture
    // Note: We need to create a temporary texture descriptor
    // The actual texture is owned by the swap chain
    TextureDesc desc{};
    desc.width = m_Width;
    desc.height = m_Height;
    desc.depth = 1;
    desc.mipLevels = 1;
    desc.format = m_Format;
    desc.type = TextureType::Texture2D;
    desc.usage = TextureUsage::ColorAttachment;
    desc.debugName = "SwapChain BackBuffer";

    // Create a wrapper texture that uses the swap chain's texture view
    // This is a bit of a hack, but WebGPU's swap chain texture is transient
    auto texture = CreateRef<WebGPUTexture>(m_Context, desc);

    // Note: Ideally we'd have a way to set the texture view directly
    // For now, this creates a new texture which won't match the swap chain
    // TODO: Refactor WebGPUTexture to support external texture views

    m_CurrentTexture = texture;
    return m_CurrentTexture;
}

void WebGPUSwapChain::Present() {
    // WebGPU presents automatically when command buffer is submitted
    // The swap chain's GetCurrentTextureView() automatically queues presentation
    // No explicit present call needed (unlike Vulkan/D3D12)

    // However, we should call present() on the swap chain to advance to next frame
    m_SwapChain.Present();
}

void WebGPUSwapChain::Resize(uint32 width, uint32 height) {
    if (width == m_Width && height == m_Height) {
        return;  // No resize needed
    }

    m_Width = width;
    m_Height = height;

    // Destroy old swap chain
    m_SwapChain = nullptr;
    m_CurrentTexture = nullptr;

    // Create new swap chain with updated dimensions
    CreateSwapChain();

    WEBGPU_LOG_INFO("WebGPU swap chain resized: " << m_Width << "x" << m_Height);
}

} // namespace rhi
} // namespace metagfx
