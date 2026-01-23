// ============================================================================
// src/rhi/webgpu/WebGPUDevice.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUDevice.h"
#include "metagfx/rhi/webgpu/WebGPUSwapChain.h"
#include "metagfx/rhi/webgpu/WebGPUBuffer.h"
#include "metagfx/rhi/webgpu/WebGPUTexture.h"
#include "metagfx/rhi/webgpu/WebGPUSampler.h"
#include "metagfx/rhi/webgpu/WebGPUShader.h"
#include "metagfx/rhi/webgpu/WebGPUPipeline.h"
#include "metagfx/rhi/webgpu/WebGPUCommandBuffer.h"
#include "metagfx/rhi/webgpu/WebGPUFramebuffer.h"
#include "metagfx/rhi/webgpu/WebGPUDescriptorSet.h"
#include "metagfx/rhi/webgpu/WebGPUSurfaceBridge.h"
#include "metagfx/core/Logger.h"

#include <SDL3/SDL.h>

namespace metagfx {
namespace rhi {

WebGPUDevice::WebGPUDevice(SDL_Window* window)
    : m_Window(window) {

    METAGFX_INFO << "Initializing WebGPU device...";

    // Create WebGPU instance
    CreateInstance();

    // Request adapter (GPU selection)
    RequestAdapter();

    // Request device (logical device)
    RequestDevice();

    // Create surface from SDL window
    CreateSurface(window);

    // Query device capabilities and limits
    QueryDeviceCapabilities();

    // Create swap chain
    m_SwapChain = CreateRef<WebGPUSwapChain>(m_Context, window);

    // Fill device info
    m_DeviceInfo.deviceName = "WebGPU Device (Dawn)";
    m_DeviceInfo.api = GraphicsAPI::WebGPU;
    m_DeviceInfo.apiVersion = 1;  // WebGPU version
    m_DeviceInfo.deviceMemory = 0;  // Not easily queryable in WebGPU

    METAGFX_INFO << "WebGPU device initialized successfully";
    METAGFX_INFO << "  Device: " << m_DeviceInfo.deviceName;
}

WebGPUDevice::~WebGPUDevice() {
    WaitIdle();

    // Release resources in reverse order
    m_SwapChain.reset();
    m_ActiveDescriptorSetLayout.reset();

    // Release WebGPU objects (handled by wgpu::RefCounted)
    m_Context.surface = nullptr;
    m_Context.queue = nullptr;
    m_Context.device = nullptr;
    m_Context.adapter = nullptr;
    m_Context.instance = nullptr;

    METAGFX_INFO << "WebGPU device destroyed";
}

void WebGPUDevice::CreateInstance() {
    wgpu::InstanceDescriptor instanceDesc{};
    m_Context.instance = wgpu::CreateInstance(&instanceDesc);

    if (!m_Context.instance) {
        WEBGPU_LOG_ERROR("Failed to create WebGPU instance");
        throw std::runtime_error("Failed to create WebGPU instance");
    }

    METAGFX_INFO << "WebGPU instance created";
}

void WebGPUDevice::RequestAdapter() {
    // Adapter request callback structure
    struct AdapterRequestData {
        wgpu::Adapter* adapter;
        bool done = false;
        bool success = false;
    };

    AdapterRequestData data;
    data.adapter = &m_Context.adapter;

    // Request adapter with high performance preference
    wgpu::RequestAdapterOptions adapterOpts{};
    adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;

    auto callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                       char const* message, void* userdata) {
        auto* data = static_cast<AdapterRequestData*>(userdata);

        if (status == WGPURequestAdapterStatus_Success) {
            *data->adapter = wgpu::Adapter::Acquire(adapter);
            data->success = true;
        } else {
            METAGFX_ERROR << "Failed to request adapter: " << (message ? message : "Unknown error");
        }

        data->done = true;
    };

    m_Context.instance.RequestAdapter(&adapterOpts, callback, &data);

    // Wait for callback (blocking on native platforms)
#ifndef __EMSCRIPTEN__
    while (!data.done) {
        // Process events to allow callback to execute
#ifdef __EMSCRIPTEN__
        emscripten_sleep(1);
#endif
    }
#endif

    if (!data.success || !m_Context.adapter) {
        WEBGPU_LOG_ERROR("Failed to obtain WebGPU adapter");
        throw std::runtime_error("Failed to obtain WebGPU adapter");
    }

    METAGFX_INFO << "WebGPU adapter acquired";
}

void WebGPUDevice::RequestDevice() {
    // Device request callback structure
    struct DeviceRequestData {
        wgpu::Device* device;
        bool done = false;
        bool success = false;
    };

    DeviceRequestData data;
    data.device = &m_Context.device;

    // Configure required device limits
    wgpu::RequiredLimits requiredLimits{};
    requiredLimits.limits.maxBindGroups = 4;
    requiredLimits.limits.maxUniformBufferBindingSize = 65536;
    requiredLimits.limits.maxStorageBufferBindingSize = 134217728;  // 128MB
    requiredLimits.limits.maxBufferSize = 268435456;  // 256MB
    requiredLimits.limits.maxVertexBuffers = 8;
    requiredLimits.limits.maxVertexAttributes = 16;

    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.requiredLimits = &requiredLimits;
    deviceDesc.defaultQueue.label = "Default Queue";

    auto callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                       char const* message, void* userdata) {
        auto* data = static_cast<DeviceRequestData*>(userdata);

        if (status == WGPURequestDeviceStatus_Success) {
            *data->device = wgpu::Device::Acquire(device);
            data->success = true;
        } else {
            METAGFX_ERROR << "Failed to request device: " << (message ? message : "Unknown error");
        }

        data->done = true;
    };

    m_Context.adapter.RequestDevice(&deviceDesc, callback, &data);

    // Wait for callback (blocking on native platforms)
#ifndef __EMSCRIPTEN__
    while (!data.done) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(1);
#endif
    }
#endif

    if (!data.success || !m_Context.device) {
        WEBGPU_LOG_ERROR("Failed to obtain WebGPU device");
        throw std::runtime_error("Failed to obtain WebGPU device");
    }

    // Get the queue from the device
    m_Context.queue = m_Context.device.GetQueue();

    if (!m_Context.queue) {
        WEBGPU_LOG_ERROR("Failed to obtain device queue");
        throw std::runtime_error("Failed to obtain device queue");
    }

    // Set error callback for device
    m_Context.device.SetUncapturedErrorCallback(
        [](WGPUErrorType type, char const* message, void* userdata) {
            const char* typeStr = "Unknown";
            switch (type) {
                case WGPUErrorType_Validation: typeStr = "Validation"; break;
                case WGPUErrorType_OutOfMemory: typeStr = "Out of Memory"; break;
                case WGPUErrorType_DeviceLost: typeStr = "Device Lost"; break;
                default: break;
            }
            METAGFX_ERROR << "WebGPU Error [" << typeStr << "]: " << message;
        },
        nullptr
    );

    METAGFX_INFO << "WebGPU device and queue created";
}

void WebGPUDevice::CreateSurface(SDL_Window* window) {
    m_Context.surface = CreateWebGPUSurfaceFromWindow(window, m_Context.instance);

    if (!m_Context.surface) {
        WEBGPU_LOG_ERROR("Failed to create WebGPU surface");
        throw std::runtime_error("Failed to create WebGPU surface");
    }

    METAGFX_INFO << "WebGPU surface created";
}

void WebGPUDevice::QueryDeviceCapabilities() {
    // Query supported limits
    wgpu::SupportedLimits limits{};
    m_Context.device.GetLimits(&limits);

    m_Context.maxBindGroups = limits.limits.maxBindGroups;
    m_Context.maxUniformBufferBindingSize = limits.limits.maxUniformBufferBindingSize;
    m_Context.minUniformBufferOffsetAlignment = limits.limits.minUniformBufferOffsetAlignment;

    // Query features (these would need to be checked individually in Dawn)
    // For now, we'll set conservative defaults
    m_Context.supportsTimestampQueries = false;
    m_Context.supportsDepthClipControl = false;
    m_Context.supportsBGRA8UnormStorage = true;

    METAGFX_INFO << "Device capabilities:";
    METAGFX_INFO << "  Max bind groups: " << m_Context.maxBindGroups;
    METAGFX_INFO << "  Max uniform buffer size: " << m_Context.maxUniformBufferBindingSize;
    METAGFX_INFO << "  Min uniform buffer alignment: " << m_Context.minUniformBufferOffsetAlignment;
}

Ref<Buffer> WebGPUDevice::CreateBuffer(const BufferDesc& desc) {
    return CreateRef<WebGPUBuffer>(m_Context, desc);
}

Ref<Texture> WebGPUDevice::CreateTexture(const TextureDesc& desc) {
    return CreateRef<WebGPUTexture>(m_Context, desc);
}

Ref<Sampler> WebGPUDevice::CreateSampler(const SamplerDesc& desc) {
    return CreateRef<WebGPUSampler>(m_Context, desc);
}

Ref<Shader> WebGPUDevice::CreateShader(const ShaderDesc& desc) {
    return CreateRef<WebGPUShader>(m_Context, desc);
}

Ref<Pipeline> WebGPUDevice::CreateGraphicsPipeline(const PipelineDesc& desc) {
    return CreateRef<WebGPUPipeline>(m_Context, desc);
}

Ref<Framebuffer> WebGPUDevice::CreateFramebuffer(const FramebufferDesc& desc) {
    return CreateRef<WebGPUFramebuffer>(m_Context, desc);
}

Ref<DescriptorSet> WebGPUDevice::CreateDescriptorSet(const DescriptorSetDesc& desc) {
    return CreateRef<WebGPUDescriptorSet>(m_Context, desc);
}

Ref<CommandBuffer> WebGPUDevice::CreateCommandBuffer() {
    return CreateRef<WebGPUCommandBuffer>(m_Context);
}

void WebGPUDevice::SubmitCommandBuffer(Ref<CommandBuffer> commandBuffer) {
    auto webgpuCmd = std::static_pointer_cast<WebGPUCommandBuffer>(commandBuffer);
    wgpu::CommandBuffer cmd = webgpuCmd->GetHandle();

    if (cmd) {
        m_Context.queue.Submit(1, &cmd);
    }
}

void WebGPUDevice::WaitIdle() {
    // WebGPU doesn't have an explicit WaitIdle, but we can submit an empty command buffer
    // and wait for it via a fence-like mechanism (or just rely on queue completion)
    // For simplicity, we'll do nothing here as Dawn handles synchronization internally
}

void WebGPUDevice::SetActiveDescriptorSetLayout(Ref<DescriptorSet> descriptorSet) {
    m_ActiveDescriptorSetLayout = descriptorSet;
}

} // namespace rhi
} // namespace metagfx
