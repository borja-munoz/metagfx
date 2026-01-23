// ============================================================================
// src/rhi/webgpu/WebGPUSurfaceBridge_Web.cpp
// Platform-specific WebGPU surface creation for Web (Emscripten)
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUSurfaceBridge.h"
#include "metagfx/core/Logger.h"

#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#endif

namespace metagfx {
namespace rhi {

wgpu::Surface CreateWebGPUSurfaceFromWindow(SDL_Window* window, wgpu::Instance instance) {
#ifdef __EMSCRIPTEN__
    // For Emscripten, we create a surface from the canvas element
    // The canvas selector is typically "#canvas" but can be customized

    // Get canvas selector (default to #canvas)
    const char* canvasSelector = "#canvas";

    // Create WebGPU surface descriptor for canvas
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
    canvasDesc.selector = canvasSelector;

    wgpu::SurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = &canvasDesc;

    wgpu::Surface surface = instance.CreateSurface(&surfaceDesc);

    if (!surface) {
        METAGFX_ERROR << "Failed to create WebGPU surface from HTML canvas";
        return nullptr;
    }

    METAGFX_INFO << "Created WebGPU surface from HTML canvas: " << canvasSelector;
    return surface;
#else
    METAGFX_ERROR << "WebGPUSurfaceBridge_Web.cpp should only be compiled with Emscripten";
    return nullptr;
#endif
}

} // namespace rhi
} // namespace metagfx
