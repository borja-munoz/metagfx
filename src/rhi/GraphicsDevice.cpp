// ============================================================================
// src/rhi/GraphicsDevice.cpp 
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/GraphicsDevice.h"

#ifdef METAGFX_USE_VULKAN
#include "metagfx/rhi/vulkan/VulkanDevice.h"
#endif

#ifdef METAGFX_USE_METAL
#include "metagfx/rhi/metal/MetalDevice.h"
#endif

#ifdef METAGFX_USE_WEBGPU
#include "metagfx/rhi/webgpu/WebGPUDevice.h"
#endif

namespace metagfx {
namespace rhi {

Ref<GraphicsDevice> CreateGraphicsDevice(GraphicsAPI api, void* nativeWindowHandle) {
    switch (api) {
#ifdef METAGFX_USE_VULKAN
        case GraphicsAPI::Vulkan:
            METAGFX_INFO << "Creating Vulkan graphics device...";
            return CreateRef<VulkanDevice>(static_cast<SDL_Window*>(nativeWindowHandle));
#endif

#ifdef METAGFX_USE_D3D12
        case GraphicsAPI::Direct3D12:
            METAGFX_ERROR << "Direct3D 12 not yet implemented (Milestone 4.1)";
            return nullptr;
#endif

#ifdef METAGFX_USE_METAL
        case GraphicsAPI::Metal:
            METAGFX_INFO << "Creating Metal graphics device...";
            return CreateRef<MetalDevice>(static_cast<SDL_Window*>(nativeWindowHandle));
#endif

#ifdef METAGFX_USE_WEBGPU
        case GraphicsAPI::WebGPU:
            METAGFX_INFO << "Creating WebGPU graphics device...";
            return CreateRef<WebGPUDevice>(static_cast<SDL_Window*>(nativeWindowHandle));
#endif
            
        default:
            METAGFX_ERROR << "Unknown or unsupported graphics API";
            return nullptr;
    }
}

} // namespace rhi
} // namespace metagfx

