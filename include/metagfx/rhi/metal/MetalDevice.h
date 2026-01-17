// ============================================================================
// include/metagfx/rhi/metal/MetalDevice.h
// ============================================================================
#pragma once

#include "metagfx/rhi/GraphicsDevice.h"
#include "MetalTypes.h"

struct SDL_Window;

namespace metagfx {
namespace rhi {

class MetalDevice : public GraphicsDevice {
public:
    MetalDevice(SDL_Window* window);
    ~MetalDevice() override;

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

    // Descriptor set layout management (no-op for Metal - Metal doesn't use descriptor set layouts)
    void SetActiveDescriptorSetLayout(Ref<DescriptorSet> descriptorSet) override;

    Ref<SwapChain> GetSwapChain() override { return m_SwapChain; }

    // Metal-specific
    MetalContext& GetContext() { return m_Context; }
    const MetalContext& GetContext() const { return m_Context; }

private:
    void CreateDevice(SDL_Window* window);
    void CreateCommandQueue();

    MetalContext m_Context;
    DeviceInfo m_DeviceInfo;

    Ref<SwapChain> m_SwapChain;
    SDL_Window* m_Window = nullptr;
};

} // namespace rhi
} // namespace metagfx
