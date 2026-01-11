// ============================================================================
// include/metagfx/rhi/vulkan/VulkanDevice.h
// ============================================================================
#pragma once

#include "metagfx/rhi/GraphicsDevice.h"
#include "VulkanTypes.h"
#include <SDL3/SDL.h>

namespace metagfx {
namespace rhi {

class VulkanDevice : public GraphicsDevice {
public:
    VulkanDevice(SDL_Window* window);
    ~VulkanDevice() override;

    // GraphicsDevice interface
    const DeviceInfo& GetDeviceInfo() const override { return m_DeviceInfo; }
    
    Ref<Buffer> CreateBuffer(const BufferDesc& desc) override;
    Ref<Texture> CreateTexture(const TextureDesc& desc) override;
    Ref<Sampler> CreateSampler(const SamplerDesc& desc) override;
    Ref<Shader> CreateShader(const ShaderDesc& desc) override;
    Ref<Pipeline> CreateGraphicsPipeline(const PipelineDesc& desc) override;
    Ref<Framebuffer> CreateFramebuffer(const FramebufferDesc& desc) override;
    
    Ref<CommandBuffer> CreateCommandBuffer() override;
    void SubmitCommandBuffer(Ref<CommandBuffer> commandBuffer) override;
    
    void WaitIdle() override;
    
    Ref<SwapChain> GetSwapChain() override { return m_SwapChain; }
    
    // Vulkan-specific
    VulkanContext& GetContext() { return m_Context; }
    uint32 FindMemoryType(uint32 typeFilter, VkMemoryPropertyFlags properties);

    // Descriptor set layout management
    void SetDescriptorSetLayout(VkDescriptorSetLayout layout) { m_DescriptorSetLayout = layout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }

private:
    void CreateInstance(SDL_Window* window);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateCommandPool();

    VulkanContext m_Context;
    DeviceInfo m_DeviceInfo;
    
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    
    Ref<SwapChain> m_SwapChain;
    SDL_Window* m_Window = nullptr;

    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
};

} // namespace rhi
} // namespace metagfx
