// ============================================================================
// include/metagfx/rhi/vulkan/VulkanSwapChain.h
// ============================================================================
#pragma once

#include "metagfx/rhi/SwapChain.h"
#include "VulkanTypes.h"
#include <SDL3/SDL.h>

namespace metagfx {
namespace rhi {

class VulkanSwapChain : public SwapChain {
public:
    VulkanSwapChain(VulkanContext& context, SDL_Window* window);
    ~VulkanSwapChain() override;

    void Present() override;
    void Resize(uint32 width, uint32 height) override;
    
    Ref<Texture> GetCurrentBackBuffer() override;
    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Format GetFormat() const override { return m_Format; }
    
    // Vulkan-specific
    VkSwapchainKHR GetHandle() const { return m_SwapChain; }
    uint32 GetCurrentImageIndex() const { return m_CurrentImageIndex; }
    uint32 GetCurrentFrame() const { return m_CurrentFrame; }
    VkSemaphore GetImageAvailableSemaphore() const { return m_ImageAvailableSemaphores[m_CurrentFrame]; }
    VkSemaphore GetRenderFinishedSemaphore() const { return m_RenderFinishedSemaphores[m_CurrentFrame]; }
    VkFence GetInFlightFence() const { return m_InFlightFences[m_CurrentFrame]; }

private:
    void CreateSwapChain();
    void CreateImageViews();
    void CreateSyncObjects();
    void Cleanup();

    VulkanContext& m_Context;
    SDL_Window* m_Window;

    VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
    VkSwapchainKHR m_OldSwapChain = VK_NULL_HANDLE;  // For swap chain recreation
    std::vector<VkImage> m_Images;
    std::vector<VkImageView> m_ImageViews;
    std::vector<Ref<Texture>> m_Textures;

    uint32 m_Width = 0;
    uint32 m_Height = 0;
    Format m_Format = Format::Undefined;
    VkFormat m_VkFormat = VK_FORMAT_UNDEFINED;
    
    // Synchronization
    static constexpr uint32 MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    uint32 m_CurrentFrame = 0;
    uint32 m_CurrentImageIndex = 0;
};

} // namespace rhi
} // namespace metagfx
