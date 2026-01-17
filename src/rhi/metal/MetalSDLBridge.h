// ============================================================================
// src/rhi/metal/MetalSDLBridge.h
// ============================================================================
// This header provides the C++ interface for SDL/Metal bridging.
// The implementation is in MetalSDLBridge.mm (Objective-C++) because
// SDL_Metal_GetLayer returns an Objective-C CAMetalLayer*.
// ============================================================================
#pragma once

#include "metagfx/rhi/metal/MetalTypes.h"
#include <SDL3/SDL.h>

namespace metagfx {
namespace rhi {

// Bridge functions for SDL Metal integration
// These functions encapsulate the Objective-C bridging required by SDL3

// Create Metal view and get the CA::MetalLayer from SDL
// Returns nullptr on failure
CA::MetalLayer* CreateMetalLayerFromSDL(SDL_MetalView view);

// Configure the Metal layer with device and pixel format
void ConfigureMetalLayer(CA::MetalLayer* layer, MTL::Device* device);

// Get the pixel format from the Metal layer
MTL::PixelFormat GetMetalLayerPixelFormat(CA::MetalLayer* layer);

// Set the drawable size on the Metal layer
void SetMetalLayerDrawableSize(CA::MetalLayer* layer, uint32_t width, uint32_t height);

// Get the next drawable from the Metal layer
CA::MetalDrawable* GetNextDrawable(CA::MetalLayer* layer);

// Get the texture from a drawable
MTL::Texture* GetDrawableTexture(CA::MetalDrawable* drawable);

} // namespace rhi
} // namespace metagfx
