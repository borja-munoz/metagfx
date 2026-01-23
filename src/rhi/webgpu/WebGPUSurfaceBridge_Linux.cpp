// ============================================================================
// src/rhi/webgpu/WebGPUSurfaceBridge_Linux.cpp
// Platform-specific WebGPU surface creation for Linux (X11/Wayland)
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUSurfaceBridge.h"
#include "metagfx/core/Logger.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_properties.h>

#if defined(__linux__) && !defined(__ANDROID__)
// X11 support
#ifdef SDL_VIDEO_DRIVER_X11
#include <X11/Xlib.h>
#endif

// Wayland support
#ifdef SDL_VIDEO_DRIVER_WAYLAND
#include <wayland-client.h>
#endif
#endif

namespace metagfx {
namespace rhi {

wgpu::Surface CreateWebGPUSurfaceFromWindow(SDL_Window* window, wgpu::Instance instance) {
#if defined(__linux__) && !defined(__ANDROID__)
    SDL_PropertiesID props = SDL_GetWindowProperties(window);

    // Try X11 first
#ifdef SDL_VIDEO_DRIVER_X11
    if (SDL_GetProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr)) {
        Display* x11Display = (Display*)SDL_GetProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        Window x11Window = (Window)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

        if (x11Display && x11Window) {
            // Create WebGPU surface descriptor for X11
            wgpu::SurfaceDescriptorFromXlibWindow x11Desc{};
            x11Desc.display = x11Display;
            x11Desc.window = x11Window;

            wgpu::SurfaceDescriptor surfaceDesc{};
            surfaceDesc.nextInChain = &x11Desc;

            wgpu::Surface surface = instance.CreateSurface(&surfaceDesc);

            if (!surface) {
                METAGFX_ERROR << "Failed to create WebGPU surface from X11 window";
                return nullptr;
            }

            METAGFX_INFO << "Created WebGPU surface from X11 window";
            return surface;
        }
    }
#endif

    // Try Wayland if X11 failed
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    if (SDL_GetProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr)) {
        wl_display* waylandDisplay = (wl_display*)SDL_GetProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        wl_surface* waylandSurface = (wl_surface*)SDL_GetProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);

        if (waylandDisplay && waylandSurface) {
            // Create WebGPU surface descriptor for Wayland
            wgpu::SurfaceDescriptorFromWaylandSurface waylandDesc{};
            waylandDesc.display = waylandDisplay;
            waylandDesc.surface = waylandSurface;

            wgpu::SurfaceDescriptor surfaceDesc{};
            surfaceDesc.nextInChain = &waylandDesc;

            wgpu::Surface surface = instance.CreateSurface(&surfaceDesc);

            if (!surface) {
                METAGFX_ERROR << "Failed to create WebGPU surface from Wayland surface";
                return nullptr;
            }

            METAGFX_INFO << "Created WebGPU surface from Wayland surface";
            return surface;
        }
    }
#endif

    METAGFX_ERROR << "Failed to create WebGPU surface: no supported window system (X11/Wayland) found";
    return nullptr;
#else
    METAGFX_ERROR << "WebGPUSurfaceBridge_Linux.cpp should only be compiled on Linux";
    return nullptr;
#endif
}

} // namespace rhi
} // namespace metagfx
