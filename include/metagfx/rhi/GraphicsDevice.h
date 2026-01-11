// ============================================================================
// include/metagfx/rhi/GraphicsDevice.h 
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

namespace metagfx {
namespace rhi {

// Forward declarations
class Buffer;
class Texture;
class Sampler;
class Shader;
class Pipeline;
class CommandBuffer;
class SwapChain;
class Framebuffer;

class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;
    
    // Device information
    virtual const DeviceInfo& GetDeviceInfo() const = 0;
    
    // Resource creation
    virtual Ref<Buffer> CreateBuffer(const BufferDesc& desc) = 0;
    virtual Ref<Texture> CreateTexture(const TextureDesc& desc) = 0;
    virtual Ref<Sampler> CreateSampler(const SamplerDesc& desc) = 0;
    virtual Ref<Shader> CreateShader(const ShaderDesc& desc) = 0;
    virtual Ref<Pipeline> CreateGraphicsPipeline(const PipelineDesc& desc) = 0;
    virtual Ref<Framebuffer> CreateFramebuffer(const FramebufferDesc& desc) = 0;
    
    // Command buffer management
    virtual Ref<CommandBuffer> CreateCommandBuffer() = 0;
    virtual void SubmitCommandBuffer(Ref<CommandBuffer> commandBuffer) = 0;
    
    // Synchronization
    virtual void WaitIdle() = 0;
    
    // Swap chain
    virtual Ref<SwapChain> GetSwapChain() = 0;
    
protected:
    GraphicsDevice() = default;
};

Ref<GraphicsDevice> CreateGraphicsDevice(GraphicsAPI api, void* nativeWindowHandle);

} // namespace rhi
} // namespace metagfx