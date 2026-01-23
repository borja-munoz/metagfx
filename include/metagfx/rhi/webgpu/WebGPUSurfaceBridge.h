// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUSurfaceBridge.h
// ============================================================================
#pragma once

#include <webgpu/webgpu_cpp.h>

struct SDL_Window;

namespace metagfx {
namespace rhi {

/**
 * @brief Platform-agnostic interface for creating WebGPU surfaces from SDL windows
 *
 * This function abstracts the platform-specific surface creation logic.
 * Implementation is delegated to platform-specific bridge files:
 * - Windows: WebGPUSurfaceBridge_Windows.cpp (Win32 HWND → WGPUSurface)
 * - macOS: WebGPUSurfaceBridge_Metal.mm (CAMetalLayer → WGPUSurface)
 * - Linux: WebGPUSurfaceBridge_Linux.cpp (X11/Wayland → WGPUSurface)
 * - Web: WebGPUSurfaceBridge_Web.cpp (HTML Canvas → WGPUSurface)
 *
 * @param window SDL window handle
 * @param instance WebGPU instance
 * @return Platform-specific WebGPU surface
 */
wgpu::Surface CreateWebGPUSurfaceFromWindow(SDL_Window* window, wgpu::Instance instance);

} // namespace rhi
} // namespace metagfx
