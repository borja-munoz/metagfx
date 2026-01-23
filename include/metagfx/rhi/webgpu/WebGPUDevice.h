// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUDevice.h
// ============================================================================
#pragma once

#include "metagfx/rhi/GraphicsDevice.h"
#include "WebGPUTypes.h"

struct SDL_Window;

namespace metagfx {
namespace rhi {

class WebGPUDevice : public GraphicsDevice {
public:
    WebGPUDevice(SDL_Window* window);
    ~WebGPUDevice() override;

    // GraphicsDevice interface
    const DeviceInfo& GetDeviceInfo() const override { return m_DeviceInfo; }

    Ref<Buffer> CreateBuffer(const BufferDesc& desc) override;
    Ref<Texture> CreateTexture(const TextureDesc& desc) override;
    Ref<Sampler> CreateSampler(const SamplerDesc& desc) override;
    Ref<Shader> CreateShader(const ShaderDesc& desc) override;
    Ref<Pipeline> CreateGraphicsPipeline(const PipelineDesc& desc) override;
    Ref<Framebuffer> CreateFramebuffer(const FramebufferDesc& desc) override;
    Ref<DescriptorSet> CreateDescriptorSet(const DescriptorSetDesc& desc) override;

    Ref<CommandBuffer> CreateCommandBuffer() override;
    void SubmitCommandBuffer(Ref<CommandBuffer> commandBuffer) override;

    void WaitIdle() override;

    // Descriptor set layout management (used for pipeline creation in WebGPU)
    void SetActiveDescriptorSetLayout(Ref<DescriptorSet> descriptorSet) override;

    Ref<SwapChain> GetSwapChain() override { return m_SwapChain; }

    // WebGPU-specific
    WebGPUContext& GetContext() { return m_Context; }
    const WebGPUContext& GetContext() const { return m_Context; }

private:
    void CreateInstance();
    void RequestAdapter();
    void RequestDevice();
    void CreateSurface(SDL_Window* window);
    void QueryDeviceCapabilities();

    WebGPUContext m_Context;
    DeviceInfo m_DeviceInfo;

    Ref<SwapChain> m_SwapChain;
    Ref<DescriptorSet> m_ActiveDescriptorSetLayout;  // For pipeline creation
    SDL_Window* m_Window = nullptr;
};

} // namespace rhi
} // namespace metagfx
