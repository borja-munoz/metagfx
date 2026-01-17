// ============================================================================
// src/rhi/metal/MetalDevice.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalDevice.h"
#include "metagfx/rhi/metal/MetalSwapChain.h"
#include "metagfx/rhi/metal/MetalBuffer.h"
#include "metagfx/rhi/metal/MetalTexture.h"
#include "metagfx/rhi/metal/MetalSampler.h"
#include "metagfx/rhi/metal/MetalShader.h"
#include "metagfx/rhi/metal/MetalPipeline.h"
#include "metagfx/rhi/metal/MetalCommandBuffer.h"
#include "metagfx/rhi/metal/MetalFramebuffer.h"
#include "metagfx/rhi/metal/MetalDescriptorSet.h"
#include "MetalSDLBridge.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

namespace metagfx {
namespace rhi {

MetalDevice::MetalDevice(SDL_Window* window) : m_Window(window) {
    METAGFX_INFO << "Initializing Metal device...";

    CreateDevice(window);
    CreateCommandQueue();

    // Create swap chain
    m_SwapChain = CreateRef<MetalSwapChain>(m_Context, window);

    // Fill device info - get device name using metal-cpp
    const char* deviceName = m_Context.device->name()->utf8String();
    m_DeviceInfo.deviceName = std::string(deviceName);
    m_DeviceInfo.api = GraphicsAPI::Metal;
    m_DeviceInfo.apiVersion = 0; // Metal doesn't have a version number like Vulkan

    METAGFX_INFO << "Metal device initialized: " << m_DeviceInfo.deviceName;
}

MetalDevice::~MetalDevice() {
    WaitIdle();

    m_SwapChain.reset();

    // Release Metal objects (metal-cpp uses manual retain/release)
    if (m_Context.commandQueue) {
        m_Context.commandQueue->release();
        m_Context.commandQueue = nullptr;
    }

    if (m_Context.device) {
        m_Context.device->release();
        m_Context.device = nullptr;
    }

    // Note: metalLayer is owned by SDL, don't release it
    m_Context.metalLayer = nullptr;

    if (m_Context.metalView) {
        SDL_Metal_DestroyView(m_Context.metalView);
        m_Context.metalView = nullptr;
    }

    METAGFX_INFO << "Metal device destroyed";
}

void MetalDevice::CreateDevice(SDL_Window* window) {
    // Create Metal view from SDL window
    m_Context.metalView = SDL_Metal_CreateView(window);
    if (!m_Context.metalView) {
        METAGFX_ERROR << "Failed to create Metal view: " << SDL_GetError();
        return;
    }

    // Get the CA::MetalLayer from SDL view (uses Obj-C bridging)
    m_Context.metalLayer = CreateMetalLayerFromSDL(m_Context.metalView);
    if (!m_Context.metalLayer) {
        METAGFX_ERROR << "Failed to get Metal layer from view";
        return;
    }

    // Create the default Metal device using metal-cpp
    m_Context.device = MTL::CreateSystemDefaultDevice();
    if (!m_Context.device) {
        METAGFX_ERROR << "Failed to create Metal device - Metal may not be supported";
        return;
    }

    // Configure the Metal layer (uses Obj-C bridging for property access)
    ConfigureMetalLayer(m_Context.metalLayer, m_Context.device);

    // Query device capabilities
    m_Context.supportsArgumentBuffers =
        m_Context.device->argumentBuffersSupport() != MTL::ArgumentBuffersTier1;

#if TARGET_OS_OSX
    m_Context.supportsRayTracing = m_Context.device->supportsRaytracing();
#else
    m_Context.supportsRayTracing = false;
#endif

    const char* deviceName = m_Context.device->name()->utf8String();
    METAGFX_INFO << "Metal device created: " << deviceName;
    METAGFX_INFO << "  Argument buffers tier: "
                 << (m_Context.device->argumentBuffersSupport() == MTL::ArgumentBuffersTier2
                     ? "Tier 2" : "Tier 1");
    METAGFX_INFO << "  Ray tracing supported: " << (m_Context.supportsRayTracing ? "Yes" : "No");
}

void MetalDevice::CreateCommandQueue() {
    m_Context.commandQueue = m_Context.device->newCommandQueue();
    if (!m_Context.commandQueue) {
        METAGFX_ERROR << "Failed to create Metal command queue";
        return;
    }

    METAGFX_DEBUG << "Metal command queue created";
}

Ref<Buffer> MetalDevice::CreateBuffer(const BufferDesc& desc) {
    return CreateRef<MetalBuffer>(m_Context, desc);
}

Ref<Texture> MetalDevice::CreateTexture(const TextureDesc& desc) {
    return CreateRef<MetalTexture>(m_Context, desc);
}

Ref<Sampler> MetalDevice::CreateSampler(const SamplerDesc& desc) {
    return CreateRef<MetalSampler>(m_Context, desc);
}

Ref<Shader> MetalDevice::CreateShader(const ShaderDesc& desc) {
    return CreateRef<MetalShader>(m_Context, desc);
}

Ref<Pipeline> MetalDevice::CreateGraphicsPipeline(const PipelineDesc& desc) {
    return CreateRef<MetalPipeline>(m_Context, desc);
}

Ref<Framebuffer> MetalDevice::CreateFramebuffer(const FramebufferDesc& desc) {
    return CreateRef<MetalFramebuffer>(m_Context, desc);
}

Ref<DescriptorSet> MetalDevice::CreateDescriptorSet(const DescriptorSetDesc& desc) {
    return CreateRef<MetalDescriptorSet>(m_Context, desc);
}

void MetalDevice::SetActiveDescriptorSetLayout(Ref<DescriptorSet> descriptorSet) {
    // Metal doesn't use descriptor set layouts like Vulkan
    // Resources are bound directly to the encoder, so this is a no-op
    (void)descriptorSet;
}

Ref<CommandBuffer> MetalDevice::CreateCommandBuffer() {
    return CreateRef<MetalCommandBuffer>(m_Context);
}

void MetalDevice::SubmitCommandBuffer(Ref<CommandBuffer> commandBuffer) {
    auto mtlCmd = std::static_pointer_cast<MetalCommandBuffer>(commandBuffer);
    auto swapChain = std::static_pointer_cast<MetalSwapChain>(m_SwapChain);

    // Get the underlying Metal command buffer
    MTL::CommandBuffer* cmdBuffer = mtlCmd->GetHandle();

    // DEBUG: Log command buffer state for first 5 frames
    static int submitFrameCount = 0;
    submitFrameCount++;
    if (submitFrameCount <= 5) {
        METAGFX_INFO << "Metal SubmitCommandBuffer frame " << submitFrameCount
                     << ": cmdBuffer=" << (cmdBuffer ? "valid" : "NULL");
    }

    // Schedule presentation of the drawable
    CA::MetalDrawable* drawable = swapChain->GetCurrentDrawable();
    if (drawable) {
        if (submitFrameCount <= 5) {
            // Get drawable's texture to compare with what we rendered to
            MTL::Texture* drawableTex = drawable->texture();
            METAGFX_INFO << "  Drawable texture: " << (drawableTex ? "valid" : "NULL");
            if (drawableTex) {
                METAGFX_INFO << "    Drawable tex ptr=" << (void*)drawableTex
                             << ", size=" << drawableTex->width() << "x" << drawableTex->height()
                             << ", pixelFormat=" << static_cast<int>(drawableTex->pixelFormat())
                             << ", storageMode=" << static_cast<int>(drawableTex->storageMode());
            }
        }
        cmdBuffer->presentDrawable(drawable);
        if (submitFrameCount <= 5) {
            METAGFX_INFO << "  presentDrawable called";
        }
    } else {
        METAGFX_ERROR << "Metal: No drawable to present!";
    }

    // Add completion handler for frame synchronization and error checking
    dispatch_semaphore_t frameSemaphore = swapChain->GetFrameSemaphore();
    static int handlerFrameCount = 0;
    cmdBuffer->addCompletedHandler([frameSemaphore](MTL::CommandBuffer* buffer) {
        handlerFrameCount++;
        // Check for errors
        if (buffer->status() == MTL::CommandBufferStatusError) {
            NS::Error* error = buffer->error();
            if (error) {
                METAGFX_ERROR << "Metal command buffer error: " << error->localizedDescription()->utf8String();
            } else {
                METAGFX_ERROR << "Metal command buffer error (no details)";
            }
        } else if (handlerFrameCount <= 5) {
            METAGFX_INFO << "  Command buffer completed successfully (status=" << static_cast<int>(buffer->status()) << ")";
        }
        dispatch_semaphore_signal(frameSemaphore);
    });

    // Commit the command buffer
    cmdBuffer->commit();

    // DEBUG: Log commit
    if (submitFrameCount <= 5) {
        METAGFX_INFO << "  Command buffer committed";
    }

    // Advance to next frame
    swapChain->AdvanceFrame();
}

void MetalDevice::WaitIdle() {
    // Create a temporary command buffer and wait for it to complete
    // This ensures all previously submitted work is finished
    if (m_Context.commandQueue) {
        MTL::CommandBuffer* cmdBuffer = m_Context.commandQueue->commandBuffer();
        cmdBuffer->commit();
        cmdBuffer->waitUntilCompleted();
    }
}

} // namespace rhi
} // namespace metagfx
