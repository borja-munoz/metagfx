// ============================================================================
// src/rhi/GraphicsDevice.cpp 
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/GraphicsDevice.h"

#ifdef METAGFX_USE_VULKAN
#include "metagfx/rhi/vulkan/VulkanDevice.h"
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
            METAGFX_ERROR << "Metal not yet implemented (Milestone 4.2)";
            return nullptr;
#endif

#ifdef METAGFX_USE_WEBGPU
        case GraphicsAPI::WebGPU:
            METAGFX_ERROR << "WebGPU not yet implemented (Milestone 4.3)";
            return nullptr;
#endif
            
        default:
            METAGFX_ERROR << "Unknown or unsupported graphics API";
            return nullptr;
    }
}

} // namespace rhi
} // namespace metagfx

