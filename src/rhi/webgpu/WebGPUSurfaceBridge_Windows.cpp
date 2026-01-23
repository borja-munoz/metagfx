// ============================================================================
// src/rhi/webgpu/WebGPUSurfaceBridge_Windows.cpp
// Platform-specific WebGPU surface creation for Windows
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUSurfaceBridge.h"
#include "metagfx/core/Logger.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace metagfx {
namespace rhi {

wgpu::Surface CreateWebGPUSurfaceFromWindow(SDL_Window* window, wgpu::Instance instance) {
#ifdef _WIN32
    // Get Win32 HWND from SDL window
    HWND hwnd = (HWND)SDL_GetProperty(SDL_GetWindowProperties(window),
                                       SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

    if (!hwnd) {
        METAGFX_ERROR << "Failed to get Win32 HWND from SDL window";
        return nullptr;
    }

    // Get HINSTANCE
    HINSTANCE hinstance = GetModuleHandle(nullptr);

    // Create WebGPU surface descriptor for Windows
    wgpu::SurfaceDescriptorFromWindowsHWND windowsDesc{};
    windowsDesc.hwnd = hwnd;
    windowsDesc.hinstance = hinstance;

    wgpu::SurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = &windowsDesc;

    wgpu::Surface surface = instance.CreateSurface(&surfaceDesc);

    if (!surface) {
        METAGFX_ERROR << "Failed to create WebGPU surface from Windows HWND";
        return nullptr;
    }

    METAGFX_INFO << "Created WebGPU surface from Windows HWND";
    return surface;
#else
    METAGFX_ERROR << "WebGPUSurfaceBridge_Windows.cpp should only be compiled on Windows";
    return nullptr;
#endif
}

} // namespace rhi
} // namespace metagfx
