// ============================================================================
// src/rhi/vulkan/VulkanSwapChain.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanSwapChain.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"

namespace metagfx {
namespace rhi {

VulkanSwapChain::VulkanSwapChain(VulkanContext& context, SDL_Window* window)
    : m_Context(context), m_Window(window) {
    
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    m_Width = static_cast<uint32>(width);
    m_Height = static_cast<uint32>(height);
    
    CreateSwapChain();
    CreateImageViews();
    CreateSyncObjects();

    METAGFX_INFO << "Vulkan swap chain created: " << m_Width << "x" << m_Height;
}

VulkanSwapChain::~VulkanSwapChain() {
    Cleanup();
}

void VulkanSwapChain::CreateSwapChain() {
    // Query swap chain support
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Context.physicalDevice, m_Context.surface, &capabilities);
    
    uint32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_Context.physicalDevice, m_Context.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_Context.physicalDevice, m_Context.surface, &formatCount, formats.data());
    
    uint32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_Context.physicalDevice, m_Context.surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_Context.physicalDevice, m_Context.surface, &presentModeCount, presentModes.data());
    
    // Choose format
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }
    
    m_VkFormat = surfaceFormat.format;
    m_Format = FromVulkanFormat(m_VkFormat);
    
    // Choose present mode (prefer mailbox for triple buffering)
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }
    
    // Choose extent
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = m_Width;
        extent.height = m_Height;
        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
    }
    
    m_Width = extent.width;
    m_Height = extent.height;
    
    // Create swap chain
    uint32 imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_Context.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    uint32 queueFamilyIndices[] = { m_Context.graphicsQueueFamily, m_Context.presentQueueFamily };
    if (m_Context.graphicsQueueFamily != m_Context.presentQueueFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    
    VK_CHECK(vkCreateSwapchainKHR(m_Context.device, &createInfo, nullptr, &m_SwapChain));
    
    // Get swap chain images
    vkGetSwapchainImagesKHR(m_Context.device, m_SwapChain, &imageCount, nullptr);
    m_Images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Context.device, m_SwapChain, &imageCount, m_Images.data());
}

void VulkanSwapChain::CreateImageViews() {
    m_ImageViews.resize(m_Images.size());
    m_Textures.resize(m_Images.size());
    
    for (size_t i = 0; i < m_Images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_Images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_VkFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        
        VK_CHECK(vkCreateImageView(m_Context.device, &createInfo, nullptr, &m_ImageViews[i]));
        
        // Create texture wrapper (swap chain images are not owned by the texture)
        m_Textures[i] = CreateRef<VulkanTexture>(m_Context, m_Images[i], m_ImageViews[i], 
                                                   m_Width, m_Height, m_VkFormat);
    }
}

void VulkanSwapChain::CreateSyncObjects() {
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(m_Context.device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(m_Context.device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(m_Context.device, &fenceInfo, nullptr, &m_InFlightFences[i]));
    }
}

void VulkanSwapChain::Cleanup() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (m_ImageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Context.device, m_ImageAvailableSemaphores[i], nullptr);
        }
        if (m_RenderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Context.device, m_RenderFinishedSemaphores[i], nullptr);
        }
        if (m_InFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(m_Context.device, m_InFlightFences[i], nullptr);
        }
    }
    
    m_Textures.clear();
    
    for (auto imageView : m_ImageViews) {
        vkDestroyImageView(m_Context.device, imageView, nullptr);
    }
    
    if (m_SwapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_Context.device, m_SwapChain, nullptr);
    }
}

void VulkanSwapChain::Present() {
    vkWaitForFences(m_Context.device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    
    VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapChains[] = { m_SwapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &m_CurrentImageIndex;
    
    vkQueuePresentKHR(m_Context.presentQueue, &presentInfo);
    
    // Advance to next frame
    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    
    // Acquire next image
    vkAcquireNextImageKHR(m_Context.device, m_SwapChain, UINT64_MAX,
                         m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_CurrentImageIndex);
}

void VulkanSwapChain::Resize(uint32 width, uint32 height) {
    m_Width = width;
    m_Height = height;
    
    vkDeviceWaitIdle(m_Context.device);
    
    Cleanup();
    CreateSwapChain();
    CreateImageViews();
    CreateSyncObjects();
    
    // Acquire first image
    vkAcquireNextImageKHR(m_Context.device, m_SwapChain, UINT64_MAX,
                         m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_CurrentImageIndex);
    
    METAGFX_INFO << "Swap chain resized: " << m_Width << "x" << m_Height;
}

Ref<Texture> VulkanSwapChain::GetCurrentBackBuffer() {
    return m_Textures[m_CurrentImageIndex];
}

} // namespace rhi
} // namespace metagfx

