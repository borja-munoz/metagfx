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
        METAGFX_ERROR << "Failed to create WebGPU instance";
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

    // Modern Dawn API uses callback info structure
    auto callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                       WGPUStringView message, void* userdata1, void* /* userdata2 */) {
        auto* data = static_cast<AdapterRequestData*>(userdata1);

        if (status == WGPURequestAdapterStatus_Success) {
            *data->adapter = wgpu::Adapter::Acquire(adapter);
            data->success = true;
        } else {
            const char* msgStr = message.data ? message.data : "Unknown error";
            METAGFX_ERROR << "Failed to request adapter: " << msgStr;
        }

        data->done = true;
    };

    WGPURequestAdapterCallbackInfo callbackInfo{};
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = callback;
    callbackInfo.userdata1 = &data;

    WGPUInstance instance = m_Context.instance.Get();
    wgpuInstanceRequestAdapter(instance, reinterpret_cast<const WGPURequestAdapterOptions*>(&adapterOpts), callbackInfo);

    // Wait for callback (blocking on native platforms)
#ifndef __EMSCRIPTEN__
    while (!data.done) {
        // Process events to allow callback to execute
        wgpuInstanceProcessEvents(instance);
    }
#endif

    if (!data.success || !m_Context.adapter) {
        METAGFX_ERROR << "Failed to obtain WebGPU adapter";
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
    // NOTE: WebGPU requires minUniformBufferOffsetAlignment to be at least 256
    WGPULimits requiredLimits{};
    requiredLimits.maxBindGroups = 4;
    requiredLimits.maxUniformBufferBindingSize = 65536;
    requiredLimits.maxStorageBufferBindingSize = 134217728;  // 128MB
    requiredLimits.maxBufferSize = 268435456;  // 256MB
    requiredLimits.maxVertexBuffers = 8;
    requiredLimits.maxVertexAttributes = 16;
    requiredLimits.minUniformBufferOffsetAlignment = 256;  // Required by WebGPU spec
    requiredLimits.minStorageBufferOffsetAlignment = 256;

    // Device lost callback
    auto lostCallback = [](WGPUDevice const * /* device */, WGPUDeviceLostReason reason,
                           WGPUStringView message, void* /* userdata1 */, void* /* userdata2 */) {
        const char* reasonStr = "Unknown";
        switch (reason) {
            case WGPUDeviceLostReason_Unknown: reasonStr = "Unknown"; break;
            case WGPUDeviceLostReason_Destroyed: reasonStr = "Destroyed"; break;
            case WGPUDeviceLostReason_CallbackCancelled: reasonStr = "Callback Cancelled"; break;
            case WGPUDeviceLostReason_FailedCreation: reasonStr = "Failed Creation"; break;
            default: break;
        }
        const char* msgStr = message.data ? message.data : "No message";
        // Destroyed and CallbackCancelled are expected during normal shutdown
        if (reason == WGPUDeviceLostReason_Destroyed || reason == WGPUDeviceLostReason_CallbackCancelled) {
            METAGFX_INFO << "WebGPU Device Lost [" << reasonStr << "]: " << msgStr;
        } else {
            METAGFX_ERROR << "WebGPU Device Lost [" << reasonStr << "]: " << msgStr;
        }
    };

    WGPUDeviceLostCallbackInfo lostCallbackInfo{};
    lostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    lostCallbackInfo.callback = lostCallback;

    // Uncaptured error callback
    auto errorCallback = [](WGPUDevice const * /* device */, WGPUErrorType type,
                            WGPUStringView message, void* /* userdata1 */, void* /* userdata2 */) {
        const char* typeStr = "Unknown";
        switch (type) {
            case WGPUErrorType_NoError: typeStr = "No Error"; break;
            case WGPUErrorType_Validation: typeStr = "Validation"; break;
            case WGPUErrorType_OutOfMemory: typeStr = "Out of Memory"; break;
            case WGPUErrorType_Internal: typeStr = "Internal"; break;
            case WGPUErrorType_Unknown: typeStr = "Unknown"; break;
            default: break;
        }
        const char* msgStr = message.data ? message.data : "No message";
        METAGFX_ERROR << "WebGPU Error [" << typeStr << "]: " << msgStr;
    };

    WGPUUncapturedErrorCallbackInfo errorCallbackInfo{};
    errorCallbackInfo.callback = errorCallback;

    // Create device descriptor
    WGPUQueueDescriptor queueDesc{};
    queueDesc.label = WGPU_STRING_VIEW_INIT;

    WGPUDeviceDescriptor deviceDesc{};
    deviceDesc.requiredLimits = &requiredLimits;
    deviceDesc.defaultQueue = queueDesc;
    deviceDesc.deviceLostCallbackInfo = lostCallbackInfo;
    deviceDesc.uncapturedErrorCallbackInfo = errorCallbackInfo;

    // Modern Dawn API uses callback info structure
    auto callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                       WGPUStringView message, void* userdata1, void* /* userdata2 */) {
        auto* data = static_cast<DeviceRequestData*>(userdata1);

        if (status == WGPURequestDeviceStatus_Success) {
            *data->device = wgpu::Device::Acquire(device);
            data->success = true;
        } else {
            const char* msgStr = message.data ? message.data : "Unknown error";
            METAGFX_ERROR << "Failed to request device: " << msgStr;
        }

        data->done = true;
    };

    WGPURequestDeviceCallbackInfo callbackInfo{};
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = callback;
    callbackInfo.userdata1 = &data;

    WGPUAdapter adapter = m_Context.adapter.Get();
    wgpuAdapterRequestDevice(adapter, &deviceDesc, callbackInfo);

    // Wait for callback (blocking on native platforms)
#ifndef __EMSCRIPTEN__
    WGPUInstance instance = m_Context.instance.Get();
    while (!data.done) {
        wgpuInstanceProcessEvents(instance);
    }
#endif

    if (!data.success || !m_Context.device) {
        METAGFX_ERROR << "Failed to obtain WebGPU device";
        throw std::runtime_error("Failed to obtain WebGPU device");
    }

    // Get the queue from the device
    m_Context.queue = m_Context.device.GetQueue();

    if (!m_Context.queue) {
        METAGFX_ERROR << "Failed to obtain device queue";
        throw std::runtime_error("Failed to obtain device queue");
    }

    METAGFX_INFO << "WebGPU device and queue created";
}

void WebGPUDevice::CreateSurface(SDL_Window* window) {
    m_Context.surface = CreateWebGPUSurfaceFromWindow(window, m_Context.instance);

    if (!m_Context.surface) {
        METAGFX_ERROR << "Failed to create WebGPU surface";
        throw std::runtime_error("Failed to create WebGPU surface");
    }

    METAGFX_INFO << "WebGPU surface created";
}

void WebGPUDevice::QueryDeviceCapabilities() {
    // Query supported limits using modern Dawn API
    WGPULimits limits{};
    WGPUDevice device = m_Context.device.Get();
    WGPUStatus status = wgpuDeviceGetLimits(device, &limits);

    if (status != WGPUStatus_Success) {
        METAGFX_ERROR << "Failed to query device limits";
        // Use default conservative limits
        m_Context.maxBindGroups = 4;
        m_Context.maxUniformBufferBindingSize = 65536;
        m_Context.minUniformBufferOffsetAlignment = 256;
    } else {
        m_Context.maxBindGroups = limits.maxBindGroups;
        m_Context.maxUniformBufferBindingSize = static_cast<uint32>(limits.maxUniformBufferBindingSize);
        m_Context.minUniformBufferOffsetAlignment = limits.minUniformBufferOffsetAlignment;
    }

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
