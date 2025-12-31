// ============================================================================
// src/rhi/vulkan/VulkanDevice.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/vulkan/VulkanDevice.h"
#include "metagfx/rhi/vulkan/VulkanSwapChain.h"
#include "metagfx/rhi/vulkan/VulkanBuffer.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"
#include "metagfx/rhi/vulkan/VulkanSampler.h"
#include "metagfx/rhi/vulkan/VulkanShader.h"
#include "metagfx/rhi/vulkan/VulkanPipeline.h"
#include "metagfx/rhi/vulkan/VulkanCommandBuffer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace metagfx {
namespace rhi {

VulkanDevice::VulkanDevice(SDL_Window* window) : m_Window(window) {
    METAGFX_INFO << "Initializing Vulkan device...";

    CreateInstance(window);
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateCommandPool();
    
    // Create swap chain
    m_SwapChain = CreateRef<VulkanSwapChain>(m_Context, window);
    
    // Fill device info
    m_DeviceInfo.deviceName = std::string(m_Context.deviceProperties.deviceName);
    m_DeviceInfo.api = GraphicsAPI::Vulkan;
    m_DeviceInfo.apiVersion = m_Context.deviceProperties.apiVersion;

    METAGFX_INFO << "Vulkan device initialized: " << m_DeviceInfo.deviceName;
}

VulkanDevice::~VulkanDevice() {
    WaitIdle();
    
    m_SwapChain.reset();
    
    if (m_CommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_Context.device, m_CommandPool, nullptr);
    }
    
    if (m_Context.device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_Context.device, nullptr);
    }
    
    if (m_Context.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_Context.instance, m_Context.surface, nullptr);
    }
    
    if (m_Context.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_Context.instance, nullptr);
    }
    
    METAGFX_INFO << "Vulkan device destroyed";
}

void VulkanDevice::CreateInstance(SDL_Window* window) {
    // Get required extensions from SDL
    uint32_t sdlExtensionCount = 0;
    char const* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    
    if (!sdlExtensions) {
        METAGFX_ERROR << "Failed to get Vulkan instance extensions from SDL: " << SDL_GetError();
        return;
    }
    
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "PBR Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "PBR Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;
    
    // Enable portability enumeration for macOS
    #ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    createInfo.enabledExtensionCount = static_cast<uint32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    #endif
    
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_Context.instance));
    
    // Create surface
    if (!SDL_Vulkan_CreateSurface(window, m_Context.instance, nullptr, &m_Context.surface)) {
        METAGFX_ERROR << "Failed to create Vulkan surface: " << SDL_GetError();
    }
}

void VulkanDevice::PickPhysicalDevice() {
    uint32 deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Context.instance, &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        METAGFX_ERROR << "Failed to find GPUs with Vulkan support";
        return;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Context.instance, &deviceCount, devices.data());
    
    // Just pick the first device for now
    m_Context.physicalDevice = devices[0];
    
    vkGetPhysicalDeviceProperties(m_Context.physicalDevice, &m_Context.deviceProperties);
    vkGetPhysicalDeviceFeatures(m_Context.physicalDevice, &m_Context.deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(m_Context.physicalDevice, &m_Context.memoryProperties);

    METAGFX_INFO << "Selected GPU: " << m_Context.deviceProperties.deviceName;
}

void VulkanDevice::CreateLogicalDevice() {
    // Find queue families
    uint32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context.physicalDevice, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_Context.physicalDevice, &queueFamilyCount, queueFamilies.data());
    
    // Find graphics and present queue families
    for (uint32 i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_Context.graphicsQueueFamily = i;
        }
        
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_Context.physicalDevice, i, m_Context.surface, &presentSupport);
        if (presentSupport) {
            m_Context.presentQueueFamily = i;
        }
    }
    
    // Create queue create infos
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_Context.graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
    
    // Device features
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.fillModeNonSolid = VK_TRUE; // For wireframe mode

    // NOTE: Dynamic rendering causes issues with MoltenVK on macOS
    // We'll use traditional render passes for now

    // Device extensions
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    #ifdef __APPLE__
    deviceExtensions.push_back("VK_KHR_portability_subset");
    #endif

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    
    VK_CHECK(vkCreateDevice(m_Context.physicalDevice, &createInfo, nullptr, &m_Context.device));
    
    vkGetDeviceQueue(m_Context.device, m_Context.graphicsQueueFamily, 0, &m_Context.graphicsQueue);
    vkGetDeviceQueue(m_Context.device, m_Context.presentQueueFamily, 0, &m_Context.presentQueue);
}

void VulkanDevice::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_Context.graphicsQueueFamily;

    VK_CHECK(vkCreateCommandPool(m_Context.device, &poolInfo, nullptr, &m_CommandPool));
    m_Context.commandPool = m_CommandPool;
}

Ref<Buffer> VulkanDevice::CreateBuffer(const BufferDesc& desc) {
    return CreateRef<VulkanBuffer>(m_Context, desc);
}

Ref<Texture> VulkanDevice::CreateTexture(const TextureDesc& desc) {
    return CreateRef<VulkanTexture>(m_Context, desc);
}

Ref<Sampler> VulkanDevice::CreateSampler(const SamplerDesc& desc) {
    return CreateRef<VulkanSampler>(m_Context, desc);
}

Ref<Shader> VulkanDevice::CreateShader(const ShaderDesc& desc) {
    return CreateRef<VulkanShader>(m_Context, desc);
}

Ref<Pipeline> VulkanDevice::CreateGraphicsPipeline(const PipelineDesc& desc) {
    auto swapChain = std::static_pointer_cast<VulkanSwapChain>(m_SwapChain);
    
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = ToVulkanFormat(swapChain->GetFormat());
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    
    VkRenderPass renderPass;
    VK_CHECK(vkCreateRenderPass(m_Context.device, &renderPassInfo, nullptr, &renderPass));
    
    // Create pipeline with descriptor set layout
    // Note: m_DescriptorSetLayout should be set by the application before creating the pipeline
    return CreateRef<VulkanPipeline>(m_Context, desc, renderPass, m_DescriptorSetLayout);
}

Ref<CommandBuffer> VulkanDevice::CreateCommandBuffer() {
    return CreateRef<VulkanCommandBuffer>(m_Context, m_CommandPool);
}

void VulkanDevice::SubmitCommandBuffer(Ref<CommandBuffer> commandBuffer) {
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
    auto swapChain = std::static_pointer_cast<VulkanSwapChain>(m_SwapChain);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = { swapChain->GetImageAvailableSemaphore() };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    
    VkCommandBuffer cmdBuffer = vkCmd->GetHandle();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    
    VkSemaphore signalSemaphores[] = { swapChain->GetRenderFinishedSemaphore() };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    VkFence fence = swapChain->GetInFlightFence();

    // NO fence wait/reset here - that will be done in Present before acquiring next image
    VkResult result = vkQueueSubmit(m_Context.graphicsQueue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS) {
        METAGFX_ERROR << "Failed to submit command buffer: " << result;
    }
}

void VulkanDevice::WaitIdle() {
    if (m_Context.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_Context.device);
    }
}

uint32 VulkanDevice::FindMemoryType(uint32 typeFilter, VkMemoryPropertyFlags properties) {
    for (uint32 i = 0; i < m_Context.memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && 
            (m_Context.memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    METAGFX_ERROR << "Failed to find suitable memory type";
    return 0;
}

} // namespace rhi
} // namespace metagfx