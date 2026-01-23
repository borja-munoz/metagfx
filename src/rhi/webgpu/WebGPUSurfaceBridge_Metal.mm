// ============================================================================
// src/rhi/webgpu/WebGPUSurfaceBridge_Metal.mm
// Platform-specific WebGPU surface creation for macOS
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUSurfaceBridge.h"
#include "metagfx/core/Logger.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

#ifdef __APPLE__
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace metagfx {
namespace rhi {

wgpu::Surface CreateWebGPUSurfaceFromWindow(SDL_Window* window, wgpu::Instance instance) {
#ifdef __APPLE__
    // Create Metal view from SDL window
    SDL_MetalView metalView = SDL_Metal_CreateView(window);
    if (!metalView) {
        METAGFX_ERROR << "Failed to create Metal view from SDL window: " << SDL_GetError();
        return nullptr;
    }

    // Get CAMetalLayer from Metal view
    CAMetalLayer* metalLayer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(metalView);
    if (!metalLayer) {
        METAGFX_ERROR << "Failed to get CAMetalLayer from Metal view";
        SDL_Metal_DestroyView(metalView);
        return nullptr;
    }

    // Create WebGPU surface descriptor for Metal
    wgpu::SurfaceDescriptorFromMetalLayer metalDesc{};
    metalDesc.layer = metalLayer;

    wgpu::SurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = &metalDesc;

    wgpu::Surface surface = instance.CreateSurface(&surfaceDesc);

    if (!surface) {
        METAGFX_ERROR << "Failed to create WebGPU surface from CAMetalLayer";
        SDL_Metal_DestroyView(metalView);
        return nullptr;
    }

    METAGFX_INFO << "Created WebGPU surface from CAMetalLayer";

    // Note: We don't destroy the Metal view here as it's needed for the surface lifetime
    // It should be destroyed when the window is destroyed

    return surface;
#else
    METAGFX_ERROR << "WebGPUSurfaceBridge_Metal.mm should only be compiled on macOS";
    return nullptr;
#endif
}

} // namespace rhi
} // namespace metagfx
