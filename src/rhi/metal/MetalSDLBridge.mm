// ============================================================================
// src/rhi/metal/MetalSDLBridge.mm
// ============================================================================
// This file contains the Objective-C++ bridging code needed to convert
// between SDL's Objective-C CAMetalLayer* and metal-cpp's CA::MetalLayer*.
// ============================================================================
#include "MetalSDLBridge.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <SDL3/SDL_metal.h>

namespace metagfx {
namespace rhi {

CA::MetalLayer* CreateMetalLayerFromSDL(SDL_MetalView view) {
    // SDL_Metal_GetLayer returns an Objective-C CAMetalLayer*
    // We bridge cast it to metal-cpp's CA::MetalLayer* (same underlying type)
    CAMetalLayer* objcLayer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(view);
    return (__bridge CA::MetalLayer*)objcLayer;
}

void ConfigureMetalLayer(CA::MetalLayer* layer, MTL::Device* device) {
    // Bridge to Objective-C to configure the layer
    CAMetalLayer* objcLayer = (__bridge CAMetalLayer*)layer;
    id<MTLDevice> objcDevice = (__bridge id<MTLDevice>)device;

    objcLayer.device = objcDevice;
    objcLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    objcLayer.framebufferOnly = YES;
    objcLayer.opaque = YES;  // Mark as opaque for better performance

    // Disable display sync to help with debugging - may cause tearing but ensures immediate present
    if (@available(macOS 10.13, *)) {
        objcLayer.displaySyncEnabled = YES;  // Keep VSync enabled
    }

    // Debug logging
    NSLog(@"Metal layer configured:");
    NSLog(@"  device: %@", objcLayer.device.name);
    NSLog(@"  pixelFormat: %lu", (unsigned long)objcLayer.pixelFormat);
    NSLog(@"  framebufferOnly: %d", objcLayer.framebufferOnly);
    NSLog(@"  opaque: %d", objcLayer.opaque);
    NSLog(@"  drawableSize: %.0fx%.0f", objcLayer.drawableSize.width, objcLayer.drawableSize.height);
    NSLog(@"  contentsScale: %f", objcLayer.contentsScale);
    NSLog(@"  bounds: %.0fx%.0f", objcLayer.bounds.size.width, objcLayer.bounds.size.height);
    NSLog(@"  frame: %.0fx%.0f at (%.0f, %.0f)", objcLayer.frame.size.width, objcLayer.frame.size.height,
          objcLayer.frame.origin.x, objcLayer.frame.origin.y);
}

MTL::PixelFormat GetMetalLayerPixelFormat(CA::MetalLayer* layer) {
    CAMetalLayer* objcLayer = (__bridge CAMetalLayer*)layer;
    return static_cast<MTL::PixelFormat>(objcLayer.pixelFormat);
}

void SetMetalLayerDrawableSize(CA::MetalLayer* layer, uint32_t width, uint32_t height) {
    CAMetalLayer* objcLayer = (__bridge CAMetalLayer*)layer;
    objcLayer.drawableSize = CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height));
}

CA::MetalDrawable* GetNextDrawable(CA::MetalLayer* layer) {
    CAMetalLayer* objcLayer = (__bridge CAMetalLayer*)layer;
    id<CAMetalDrawable> drawable = [objcLayer nextDrawable];
    return (__bridge CA::MetalDrawable*)drawable;
}

MTL::Texture* GetDrawableTexture(CA::MetalDrawable* drawable) {
    id<CAMetalDrawable> objcDrawable = (__bridge id<CAMetalDrawable>)drawable;
    return (__bridge MTL::Texture*)objcDrawable.texture;
}

} // namespace rhi
} // namespace metagfx
