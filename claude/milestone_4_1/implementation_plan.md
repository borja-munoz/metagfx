# Milestone 4.1: Metal Backend Implementation (metal-cpp)

## Status: COMPLETED

## Overview

Metal graphics API backend for MetaGFX using **metal-cpp** (Apple's official C++ wrapper), enabling native rendering on macOS with pure C++ code. This implementation follows the same RHI abstraction pattern as the existing Vulkan backend.

## Key Design Decision: metal-cpp vs Objective-C++

The original plan used Objective-C++ (`.mm` files) with `#ifdef __OBJC__` conditionals. The final implementation uses **metal-cpp** for these benefits:

- **Pure C++**: All Metal code is standard C++ (`.cpp` files)
- **No ARC**: Manual retain/release matches our explicit resource management
- **Clean headers**: No `#ifdef __OBJC__` conditionals needed
- **Apple-maintained**: Official wrapper, always current with Metal API
- **Single bridging point**: Only one `.mm` file for SDL layer access

## Architecture

### Directory Structure

```
external/metal-cpp/                    - Apple's C++ Metal wrapper (header-only)
├── Foundation/Foundation.hpp
├── Metal/Metal.hpp
└── QuartzCore/QuartzCore.hpp

include/metagfx/rhi/metal/
├── MetalTypes.h           - Context struct, format conversions
├── MetalDevice.h          - GraphicsDevice implementation
├── MetalBuffer.h          - Buffer implementation
├── MetalTexture.h         - Texture implementation
├── MetalSampler.h         - Sampler implementation
├── MetalShader.h          - Shader (SPIR-V → MSL via SPIRV-Cross)
├── MetalPipeline.h        - Render pipeline state
├── MetalCommandBuffer.h   - Command encoder wrapper
├── MetalSwapChain.h       - CAMetalLayer presentation
├── MetalFramebuffer.h     - Render target management
└── MetalDescriptorSet.h   - Resource binding stub

src/rhi/metal/
├── MetalTypes.cpp         - Format conversions, PRIVATE_IMPLEMENTATION
├── MetalDevice.cpp        - Device creation
├── MetalBuffer.cpp        - GPU buffer management
├── MetalTexture.cpp       - Texture creation and upload
├── MetalSampler.cpp       - Sampler state configuration
├── MetalShader.cpp        - SPIRV-Cross MSL translation
├── MetalPipeline.cpp      - Pipeline state creation
├── MetalCommandBuffer.cpp - Command recording
├── MetalSwapChain.cpp     - Drawable management
├── MetalFramebuffer.cpp   - Attachment management
├── MetalDescriptorSet.cpp - Stub (Metal binds directly)
└── MetalSDLBridge.mm      - ONLY Obj-C++ file (SDL bridging)
```

### metal-cpp Type Mappings

| Objective-C | metal-cpp |
|-------------|-----------|
| `id<MTLDevice>` | `MTL::Device*` |
| `id<MTLCommandQueue>` | `MTL::CommandQueue*` |
| `id<MTLCommandBuffer>` | `MTL::CommandBuffer*` |
| `id<MTLRenderCommandEncoder>` | `MTL::RenderCommandEncoder*` |
| `id<MTLBuffer>` | `MTL::Buffer*` |
| `id<MTLTexture>` | `MTL::Texture*` |
| `id<MTLSamplerState>` | `MTL::SamplerState*` |
| `id<MTLRenderPipelineState>` | `MTL::RenderPipelineState*` |
| `id<MTLDepthStencilState>` | `MTL::DepthStencilState*` |
| `id<MTLLibrary>` | `MTL::Library*` |
| `id<MTLFunction>` | `MTL::Function*` |
| `CAMetalLayer*` | `CA::MetalLayer*` |
| `id<CAMetalDrawable>` | `CA::MetalDrawable*` |
| `NSString*` | `NS::String*` |
| `NSError*` | `NS::Error*` |
| `nil` | `nullptr` |

### Memory Management

metal-cpp uses manual retain/release (not ARC):

```cpp
// Creating objects (returns retained object)
MTL::Device* device = MTL::CreateSystemDefaultDevice();  // +1 retain

// Factory methods (also return retained objects)
MTL::Buffer* buffer = device->newBuffer(size, options);  // +1 retain

// Release when done (in destructor)
buffer->release();
device->release();
```

### MetalContext (Shared State)

```cpp
struct MetalContext {
    MTL::Device* device = nullptr;
    MTL::CommandQueue* commandQueue = nullptr;
    CA::MetalLayer* metalLayer = nullptr;
    SDL_MetalView metalView = nullptr;

    bool supportsArgumentBuffers = false;
    bool supportsRayTracing = false;
};
```

## Implementation Details

### SDL Bridging (MetalSDLBridge.mm)

The only Objective-C++ file, providing C++ functions to access SDL's CAMetalLayer:

```cpp
// Header (MetalSDLBridge.h)
CA::MetalLayer* CreateMetalLayerFromSDL(SDL_MetalView view);
void ConfigureMetalLayer(CA::MetalLayer* layer, MTL::Device* device);
MTL::PixelFormat GetMetalLayerPixelFormat(CA::MetalLayer* layer);
void SetMetalLayerDrawableSize(CA::MetalLayer* layer, uint32_t width, uint32_t height);
CA::MetalDrawable* GetNextDrawable(CA::MetalLayer* layer);
MTL::Texture* GetDrawableTexture(CA::MetalDrawable* drawable);

// Implementation uses (__bridge) cast
CA::MetalLayer* CreateMetalLayerFromSDL(SDL_MetalView view) {
    return (__bridge CA::MetalLayer*)SDL_Metal_GetLayer(view);
}
```

### Shader Translation (SPIRV-Cross)

```cpp
#include <spirv_msl.hpp>

// In MetalShader constructor
std::vector<uint32_t> spirvData(/*...*/);
spirv_cross::CompilerMSL mslCompiler(spirvData);

spirv_cross::CompilerMSL::Options mslOptions;
mslOptions.platform = spirv_cross::CompilerMSL::Options::macOS;
mslOptions.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);
mslCompiler.set_msl_options(mslOptions);

std::string mslSource = mslCompiler.compile();

// Create Metal library from MSL
NS::String* source = NS::String::string(mslSource.c_str(), NS::UTF8StringEncoding);
m_Library = m_Context.device->newLibrary(source, compileOptions, &error);

// SPIRV-Cross renames "main" to "main0"
m_Function = m_Library->newFunction(NS::String::string("main0", NS::UTF8StringEncoding));
```

### PRIVATE_IMPLEMENTATION Macros

Defined exactly once in MetalTypes.cpp before includes:

```cpp
// MetalTypes.cpp
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include "metagfx/rhi/metal/MetalTypes.h"
```

## Vulkan → Metal Mapping Reference

| Vulkan Concept | Metal Equivalent |
|----------------|------------------|
| VkInstance | N/A (implicit) |
| VkPhysicalDevice | MTL::Device |
| VkDevice | MTL::Device |
| VkQueue | MTL::CommandQueue |
| VkCommandBuffer | MTL::CommandBuffer |
| VkRenderPass | MTL::RenderPassDescriptor |
| VkFramebuffer | Render pass attachments |
| VkPipeline | MTL::RenderPipelineState + MTL::DepthStencilState |
| VkPipelineLayout | Implicit in shader reflection |
| VkDescriptorSet | Direct binding (setBuffer/setTexture) |
| VkBuffer | MTL::Buffer |
| VkImage/VkImageView | MTL::Texture |
| VkSampler | MTL::SamplerState |
| VkShaderModule | MTL::Library + MTL::Function |
| VkFence | waitUntilCompleted or completion handler |
| VkSemaphore | dispatch_semaphore_t |
| Push constants | setBytes:length:atIndex: |

## Format Conversion Notes

### Pixel Formats
- Most formats map directly (RGBA8 → MTL::PixelFormatRGBA8Unorm)
- RGB32 formats don't exist in Metal; mapped to RGBA32 equivalents
- D24S8: macOS has MTL::PixelFormatDepth24Unorm_Stencil8, iOS uses D32S8

### Vertex Formats
- Float/Float2/Float3/Float4 → MTL::VertexFormatFloat/Float2/Float3/Float4
- Integer formats supported (Int/Int2/Int3/Int4, UInt variants)
- Normalized formats (UChar4Normalized, etc.)

### Storage Modes
- GPUOnly → MTL::ResourceStorageModePrivate
- CPUToGPU → MTL::ResourceStorageModeShared + WriteCombined
- GPUToCPU → MTL::ResourceStorageModeShared
- macOS textures: MTL::StorageModeManaged for CPU upload

## CMake Configuration

### external/CMakeLists.txt
```cmake
# SPIRV-Cross for Metal shader translation
if(METAGFX_USE_METAL OR METAGFX_USE_WEBGPU)
    set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)   # Required for MSL
    set(SPIRV_CROSS_ENABLE_MSL ON CACHE BOOL "" FORCE)
    # ... other options OFF
    add_subdirectory(SPIRV-Cross)
endif()
```

### src/rhi/CMakeLists.txt
```cmake
if(METAGFX_USE_METAL AND APPLE)
    list(APPEND RHI_SOURCES
        metal/MetalTypes.cpp
        metal/MetalDevice.cpp
        metal/MetalSwapChain.cpp
        metal/MetalSDLBridge.mm      # Only Obj-C++ file
        metal/MetalBuffer.cpp
        metal/MetalTexture.cpp
        metal/MetalSampler.cpp
        metal/MetalShader.cpp
        metal/MetalPipeline.cpp
        metal/MetalCommandBuffer.cpp
        metal/MetalDescriptorSet.cpp
        metal/MetalFramebuffer.cpp
    )

    target_include_directories(metagfx_rhi PRIVATE
        ${CMAKE_SOURCE_DIR}/external/metal-cpp
        ${CMAKE_SOURCE_DIR}/external/SPIRV-Cross
    )

    target_link_libraries(metagfx_rhi PUBLIC
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    PRIVATE
        spirv-cross-msl
        spirv-cross-glsl
    )
endif()
```

## Files Summary

### Files Created (23 total)

**Headers (11 files)** in `include/metagfx/rhi/metal/`:
- MetalTypes.h
- MetalDevice.h
- MetalSwapChain.h
- MetalBuffer.h
- MetalTexture.h
- MetalSampler.h
- MetalShader.h
- MetalPipeline.h
- MetalCommandBuffer.h
- MetalFramebuffer.h
- MetalDescriptorSet.h

**Source files (12 files)** in `src/rhi/metal/`:
- MetalTypes.cpp
- MetalDevice.cpp
- MetalSwapChain.cpp
- MetalSDLBridge.h (bridge header)
- MetalSDLBridge.mm (only Obj-C++ file)
- MetalBuffer.cpp
- MetalTexture.cpp
- MetalSampler.cpp
- MetalShader.cpp
- MetalPipeline.cpp
- MetalCommandBuffer.cpp
- MetalFramebuffer.cpp
- MetalDescriptorSet.cpp

### Files Modified
- `external/CMakeLists.txt` - SPIRV-Cross configuration (GLSL required for MSL)
- `src/rhi/CMakeLists.txt` - Metal sources and framework linking

### Dependencies Added
- **metal-cpp** (downloaded to `external/metal-cpp/`)
- **SPIRV-Cross** (git submodule at `external/SPIRV-Cross`)
- **Metal.framework** (system)
- **MetalKit.framework** (system)
- **QuartzCore.framework** (system)
- **Foundation.framework** (system)

## Build Verification

```bash
# Configure with Metal enabled
cmake .. -DMETAGFX_USE_METAL=ON -DMETAGFX_USE_VULKAN=ON

# Build
make -j$(sysctl -n hw.ncpu)

# Result: [100%] Built target metagfx
```

## Next Steps

1. **Runtime Testing**: Run application with Metal backend selection
2. **Backend Selection**: Update Application to choose backend at runtime
3. **Visual Verification**: Compare rendering output with Vulkan backend
4. **ImGui Integration**: Wire up ImGui Metal backend for UI
