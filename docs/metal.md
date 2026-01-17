# Metal Backend Implementation

The **Metal backend** provides native graphics rendering on macOS and iOS using Apple's Metal API through the **metal-cpp** C++ wrapper. This implementation delivers high-performance rendering with a pure C++ codebase.

## Overview

MetaGFX's Metal backend was implemented in **Milestone 4.1** as part of the multi-API support initiative. The implementation uses **metal-cpp**, Apple's official C++ bindings for Metal, enabling a pure C++ codebase without Objective-C++ (with one small exception for SDL integration).

### Key Features

- **Pure C++ Implementation**: Uses metal-cpp for type-safe Metal API access
- **Full Feature Parity**: Matches Vulkan backend capabilities
- **Native Performance**: Direct Metal API access with minimal overhead
- **Clean Headers**: No `#ifdef __OBJC__` conditionals in public interfaces
- **SDL3 Integration**: Window and input handling with minimal Objective-C bridging

## metal-cpp Integration

### What is metal-cpp?

**metal-cpp** is Apple's official C++ wrapper for the Metal API, providing:
- Header-only library (easy integration)
- Type-safe C++ bindings matching Metal's Objective-C API
- Explicit retain/release memory management
- Full Metal API coverage
- Always up-to-date with latest Metal features

Download: https://developer.apple.com/metal/cpp/

### Type Mappings

| Objective-C Metal | metal-cpp | Usage |
|-------------------|-----------|-------|
| `id<MTLDevice>` | `MTL::Device*` | GPU device handle |
| `id<MTLCommandQueue>` | `MTL::CommandQueue*` | Command submission |
| `id<MTLCommandBuffer>` | `MTL::CommandBuffer*` | Command recording |
| `id<MTLRenderCommandEncoder>` | `MTL::RenderCommandEncoder*` | Render pass encoding |
| `id<MTLBuffer>` | `MTL::Buffer*` | GPU buffers |
| `id<MTLTexture>` | `MTL::Texture*` | Textures and images |
| `id<MTLSamplerState>` | `MTL::SamplerState*` | Texture sampling |
| `id<MTLRenderPipelineState>` | `MTL::RenderPipelineState*` | Pipeline state objects |
| `id<MTLDepthStencilState>` | `MTL::DepthStencilState*` | Depth/stencil state |
| `CAMetalLayer*` | `CA::MetalLayer*` | Presentation layer |
| `MTLPixelFormat` | `MTL::PixelFormat` | Texture formats |
| `nil` | `nullptr` | Null pointer |

### Memory Management

metal-cpp uses **manual reference counting** (not ARC):

```cpp
// Creating objects (returns +1 retained object)
MTL::Device* device = MTL::CreateSystemDefaultDevice();

// Factory methods also return retained objects
MTL::Buffer* buffer = device->newBuffer(size, options);

// Release when done
buffer->release();
device->release();
```

**Important**: All Metal objects must be explicitly released in destructors to prevent memory leaks.

## Architecture

### MetalContext Structure

The `MetalContext` struct holds core Metal state shared across all components:

```cpp
struct MetalContext {
    MTL::Device* device = nullptr;              // GPU device
    MTL::CommandQueue* commandQueue = nullptr;  // Command submission queue
    CA::MetalLayer* metalLayer = nullptr;       // Presentation layer
    SDL_MetalView metalView = nullptr;          // SDL window handle

    bool supportsArgumentBuffers = false;       // Feature detection
    bool supportsRayTracing = false;
};
```

### Backend Components

All Metal backend classes are in `src/rhi/metal/` and `include/metagfx/rhi/metal/`:

| Component | File | Purpose |
|-----------|------|---------|
| **MetalDevice** | `MetalDevice.cpp` | Device creation, resource factory |
| **MetalSwapChain** | `MetalSwapChain.cpp` | Presentation and back buffer management |
| **MetalCommandBuffer** | `MetalCommandBuffer.cpp` | Command recording and encoding |
| **MetalBuffer** | `MetalBuffer.cpp` | GPU buffers (vertex, index, uniform) |
| **MetalTexture** | `MetalTexture.cpp` | Textures and render targets |
| **MetalSampler** | `MetalSampler.cpp` | Texture sampling configuration |
| **MetalShader** | `MetalShader.cpp` | SPIR-V to MSL shader compilation |
| **MetalPipeline** | `MetalPipeline.cpp` | Graphics pipeline state objects |
| **MetalDescriptorSet** | `MetalDescriptorSet.cpp` | Resource binding |
| **MetalFramebuffer** | `MetalFramebuffer.cpp` | Render target management |
| **MetalTypes** | `MetalTypes.cpp` | Format conversion utilities |
| **MetalSDLBridge** | `MetalSDLBridge.mm` | SDL integration (Obj-C++) |

### File Extensions

- **`.cpp` files**: Pure C++ implementation using metal-cpp (11 files)
- **`.mm` file**: Only `MetalSDLBridge.mm` uses Objective-C++ for SDL layer bridging

## Key Implementation Details

### 1. Device Creation

```cpp
// MetalDevice.cpp
m_Context.device = MTL::CreateSystemDefaultDevice();
m_Context.commandQueue = m_Context.device->newCommandQueue();

// Feature detection
m_Context.supportsArgumentBuffers =
    m_Context.device->supportsFamily(MTL::GPUFamilyApple4);
```

### 2. Shader Compilation (SPIR-V → MSL)

MetaGFX uses SPIR-V as the intermediate shader format. For Metal, shaders are transpiled to MSL (Metal Shading Language):

```cpp
// MetalShader.cpp uses SPIRV-Cross
spirv_cross::CompilerMSL compiler(spirvData);
spirv_cross::CompilerMSL::Options options;
options.platform = spirv_cross::CompilerMSL::Options::macOS;
compiler.set_msl_options(options);

std::string msl = compiler.compile();
```

**Binding Remapping**: Metal doesn't have separate binding namespaces like Vulkan. Uniform buffers are offset by 10 to avoid conflicts with vertex buffers:

```cpp
// Remap Vulkan binding → Metal buffer index
uint32 metalBufferIndex = binding + 10;  // BUFFER_OFFSET
```

### 3. Push Constants

Metal doesn't have native push constants. They're emulated using `setVertexBytes` / `setFragmentBytes`:

```cpp
// MetalCommandBuffer.cpp
void MetalCommandBuffer::PushConstants(Ref<Pipeline> pipeline, ShaderStage stages,
                                       uint32 offset, uint32 size, const void* data) {
    // Metal's setBytes replaces entire buffer, so accumulate all data
    memcpy(m_PushConstantBuffer + offset, data, size);
    m_PushConstantSize = std::max(m_PushConstantSize, offset + size);
    m_PushConstantStages |= stages;
}

void MetalCommandBuffer::FlushPushConstants() {
    const uint32 pushConstantBufferIndex = 30;
    if (m_PushConstantStages & ShaderStage::Vertex) {
        m_RenderEncoder->setVertexBytes(m_PushConstantBuffer,
                                       m_PushConstantSize,
                                       pushConstantBufferIndex);
    }
    if (m_PushConstantStages & ShaderStage::Fragment) {
        m_RenderEncoder->setFragmentBytes(m_PushConstantBuffer,
                                         m_PushConstantSize,
                                         pushConstantBufferIndex);
    }
}
```

Push constants are flushed before every draw call.

### 4. Command Buffer Recording

Metal uses command encoders for different operations:

```cpp
// Begin render pass
MTL::RenderPassDescriptor* renderPass = MTL::RenderPassDescriptor::alloc()->init();
// ... configure attachments ...
m_RenderEncoder = m_CommandBuffer->renderCommandEncoder(renderPass);

// Record commands
m_RenderEncoder->setRenderPipelineState(pipelineState);
m_RenderEncoder->setVertexBuffer(buffer, 0, 0);
m_RenderEncoder->drawIndexedPrimitives(type, indexCount, indexType, indexBuffer, 0);

// End render pass
m_RenderEncoder->endEncoding();
m_RenderEncoder = nullptr;
```

### 5. Synchronization

Metal uses **semaphores** for frame pacing:

```cpp
// MetalSwapChain.cpp
dispatch_semaphore_t m_FrameSemaphore;

// Acquire next drawable
dispatch_semaphore_wait(m_FrameSemaphore, DISPATCH_TIME_FOREVER);
m_CurrentDrawable = m_MetalLayer->nextDrawable();

// Present with completion handler
commandBuffer->addScheduledHandler([semaphore](MTL::CommandBuffer*) {
    dispatch_semaphore_signal(semaphore);
});
commandBuffer->presentDrawable(m_CurrentDrawable);
```

### 6. Texture and Sampler Creation

```cpp
// MetalTexture.cpp
MTL::TextureDescriptor* texDesc = MTL::TextureDescriptor::alloc()->init();
texDesc->setTextureType(MTL::TextureType2D);
texDesc->setPixelFormat(ToMetalPixelFormat(desc.format));
texDesc->setWidth(desc.width);
texDesc->setHeight(desc.height);
texDesc->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);

m_Texture = m_Context.device->newTexture(texDesc);
texDesc->release();

// MetalSampler.cpp
MTL::SamplerDescriptor* samplerDesc = MTL::SamplerDescriptor::alloc()->init();
samplerDesc->setMinFilter(ToMetalFilter(desc.minFilter));
samplerDesc->setMagFilter(ToMetalFilter(desc.magFilter));
samplerDesc->setSAddressMode(ToMetalAddressMode(desc.addressModeU));

m_Sampler = m_Context.device->newSamplerState(samplerDesc);
samplerDesc->release();
```

## Coordinate System Differences

### Clip Space Convention

- **Vulkan**: Y points down, depth range [0, 1]
- **Metal**: Y points up (OpenGL-style), depth range [0, 1]

The projection matrix requires a Y-flip adjustment:

```cpp
// Application.cpp
ubo.projection = m_Camera->GetProjectionMatrix();

// Camera.cpp flips Y for Vulkan
// Metal uses OpenGL convention, so undo the flip
if (m_Device->GetDeviceInfo().api == GraphicsAPI::Metal) {
    ubo.projection[1][1] *= -1.0f;
}
```

### Texture Coordinates

Metal texture coordinates match Vulkan (origin at top-left), so no adjustments needed.

### Depth Convention

Both Vulkan and Metal use **reversed depth** (1.0 = near, 0.0 = far) for better precision:

```cpp
// Shadow mapping depth comparison
depthStencilDesc->setDepthCompareFunction(MTL::CompareFunctionGreater);
depthClear.depthStencil.depth = 0.0f;  // Far plane
```

## SDL3 Integration

SDL3 provides the Metal layer, but requires a small Objective-C++ bridge:

```cpp
// MetalSDLBridge.mm (ONLY .mm file in the entire Metal backend)
#import <Metal/Metal.hpp>
#import <QuartzCore/QuartzCore.hpp>
#include <SDL3/SDL_metal.h>

CA::MetalLayer* GetMetalLayerFromSDL(SDL_MetalView view) {
    // SDL returns Objective-C CAMetalLayer*, need __bridge cast
    return (__bridge CA::MetalLayer*)SDL_Metal_GetLayer(view);
}
```

This is the **only** place where Objective-C++ is used. Everything else is pure C++.

## ImGui Integration

ImGui supports Metal through the **ImGui Metal C++ backend**:

```cpp
// Application.cpp - Initialization
#define IMGUI_IMPL_METAL_CPP  // Use C++ bindings, not Objective-C
#include <imgui_impl_metal.h>

ImGui_ImplSDL3_InitForMetal(window);
ImGui_ImplMetal_Init(context.device);
ImGui_ImplMetal_CreateFontsTexture(context.device);

// Rendering
ImGui_ImplMetal_NewFrame(renderPassDescriptor);
ImGui::NewFrame();
// ... UI code ...
ImGui::Render();
ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(),
                               commandBuffer,
                               renderEncoder);
```

**Critical**: ImGui must render **before** `EndRendering()` while the render encoder is active.

## Format Conversion

Metal uses different format enums than Vulkan:

```cpp
// MetalTypes.cpp
MTL::PixelFormat ToMetalPixelFormat(Format format) {
    switch (format) {
        case Format::R8G8B8A8_UNORM:    return MTL::PixelFormatRGBA8Unorm;
        case Format::B8G8R8A8_UNORM:    return MTL::PixelFormatBGRA8Unorm;
        case Format::D32_SFLOAT:        return MTL::PixelFormatDepth32Float;
        case Format::BC1_RGB_UNORM:     return MTL::PixelFormatBC1_RGBA;
        case Format::BC3_UNORM:         return MTL::PixelFormatBC3_RGBA;
        // ... more formats
    }
}
```

## Performance Considerations

### 1. Memory Allocation

Use **managed** or **shared** storage modes for buffers that CPU writes to:

```cpp
MTL::ResourceOptions options = MTL::ResourceStorageModeShared |
                                MTL::ResourceCPUCacheModeWriteCombined;
```

For GPU-only buffers, use **private** storage mode for best performance.

### 2. Buffer Reuse

Metal buffers can be mapped persistently. MetaGFX uses `CopyData()` for simplicity:

```cpp
void* MetalBuffer::Map() {
    return m_Buffer->contents();  // Persistent mapping
}

void MetalBuffer::CopyData(const void* data, size_t size, size_t offset) {
    void* mapped = static_cast<uint8_t*>(m_Buffer->contents()) + offset;
    memcpy(mapped, data, size);
    // No explicit flush needed for shared storage mode
}
```

### 3. Command Buffer Optimization

Metal command buffers are lightweight and can be created per-frame. MetaGFX creates one command buffer per frame.

### 4. Pipeline State Objects

Metal PSOs are expensive to create. MetaGFX caches them (one per shader combination).

## Debugging

### Metal Frame Capture

Use Xcode's **Metal Frame Debugger**:
1. Run app in Xcode: `open -a Xcode build/bin/MetaGFX.app`
2. Debug → Capture GPU Frame (Cmd+Option+G)
3. Inspect draw calls, textures, buffers, and shader execution

### Validation Layers

Enable Metal validation in Xcode scheme:
- Edit Scheme → Run → Diagnostics → Metal API Validation

### Shader Debugging

View compiled MSL shaders:
```cpp
// Add to MetalShader.cpp
METAGFX_INFO << "Compiled MSL:\n" << msl;
```

## Limitations and Differences from Vulkan

| Feature | Vulkan | Metal | Notes |
|---------|--------|-------|-------|
| **Push Constants** | Native | Emulated with `setBytes` | Limited to 128 bytes |
| **Descriptor Sets** | Explicit layout | Buffer/texture binding | Binding namespace offset |
| **Render Passes** | Explicit | Implicit via encoders | Simplified API |
| **Memory Barriers** | Explicit | Automatic coherence | Metal handles synchronization |
| **Subpasses** | Supported | Not supported | Use multiple encoders |
| **Pipeline Cache** | Explicit | Automatic | Metal caches PSOs internally |
| **Timestamp Queries** | Supported | Supported | Metal GPU counters |

## Tested Features

All features verified on macOS (Metal 3):

- ✅ **PBR Rendering**: Cook-Torrance BRDF with full material system
- ✅ **Shadow Mapping**: Directional shadows with PCF filtering
- ✅ **Image-Based Lighting (IBL)**: Environment maps and diffuse/specular IBL
- ✅ **Skybox Rendering**: Cubemap backgrounds with LOD control
- ✅ **Normal Mapping**: Tangent-space normal maps with derivative-based TBN
- ✅ **Model Loading**: OBJ, FBX, glTF, COLLADA via Assimp
- ✅ **Texture System**: Albedo, normal, metallic-roughness, AO, emissive maps
- ✅ **Ground Plane**: Shadow reception on infinite ground
- ✅ **ImGui Integration**: Full UI controls for all rendering features
- ✅ **Multi-Light Support**: Up to 16 lights (directional, point, spot)
- ✅ **ACES Tone Mapping**: Filmic tone mapping with exposure control

## CMake Configuration

```cmake
# Enable Metal backend
cmake .. -DMETAGFX_USE_METAL=ON

# Metal-specific setup
if(METAGFX_USE_METAL)
    # Add metal-cpp include path
    target_include_directories(metagfx_rhi PRIVATE
        ${CMAKE_SOURCE_DIR}/external/metal-cpp)

    # Link Metal frameworks
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

    target_link_libraries(metagfx_rhi PUBLIC
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
    )

    # Enable Metal C++ for ImGui
    target_compile_definitions(imgui_backends PUBLIC IMGUI_IMPL_METAL_CPP)
endif()
```

## Platform Support

| Platform | Support | Notes |
|----------|---------|-------|
| **macOS** | ✅ Full | Tested on macOS 14+ with Metal 3 |
| **iOS** | 🔄 Planned | Metal code compatible, needs iOS window integration |
| **Windows** | ❌ N/A | Metal is Apple-exclusive |
| **Linux** | ❌ N/A | Metal is Apple-exclusive |

## Future Enhancements

Potential Metal-specific optimizations:

- **Argument Buffers**: More efficient resource binding for complex scenes
- **Tile Shading**: Mobile GPU optimization for deferred rendering
- **Metal Performance Shaders (MPS)**: Compute shader optimizations
- **Ray Tracing**: Metal ray tracing API integration (macOS 12+)
- **Variable Rate Rasterization**: Adaptive shading for VR/AR

## Resources

- **metal-cpp**: https://developer.apple.com/metal/cpp/
- **Metal Documentation**: https://developer.apple.com/documentation/metal
- **Metal Shading Language**: https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
- **SPIRV-Cross**: https://github.com/KhronosGroup/SPIRV-Cross (SPIR-V → MSL transpiler)

## Comparison with Vulkan Backend

Both backends provide identical functionality through the RHI abstraction:

| Aspect | Vulkan | Metal |
|--------|--------|-------|
| **API Style** | Explicit, verbose | Streamlined, opinionated |
| **Memory Management** | VMA (Vulkan Memory Allocator) | Automatic with resource modes |
| **Shader Format** | SPIR-V (native) | MSL (transpiled from SPIR-V) |
| **Synchronization** | Explicit (fences, semaphores) | Automatic coherence + semaphores |
| **Platform Support** | Windows, Linux, macOS, Android | macOS, iOS only |
| **Debugging Tools** | RenderDoc, Validation layers | Xcode Metal Debugger |
| **Code Complexity** | Higher (more boilerplate) | Lower (simpler API) |

The Metal backend demonstrates the RHI's effectiveness—despite fundamental API differences, both backends share the same high-level rendering code with zero changes to the application layer.

---

**Implementation Status**: ✅ **Complete** (Milestone 4.1)
**Last Updated**: January 2026
**Metal Version**: Metal 3 (macOS 14+)
