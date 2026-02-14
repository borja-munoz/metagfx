# WebGPU Backend Implementation

The **WebGPU backend** provides cross-platform GPU rendering through Google's **Dawn** implementation, targeting native desktop (Windows, macOS, Linux) and web (Emscripten). It was implemented in **Milestone 4.2** as the third backend after Vulkan and Metal.

## Overview

### Why Dawn?

**Google's Dawn** was chosen over alternatives (wgpu-native, Emscripten's built-in WebGPU) because:

- **Pure C++ API** (`webgpu/webgpu_cpp.h`) — aligns with the C++20 codebase
- **Official W3C reference implementation** — actively maintained, used in Chrome
- **Multi-platform backends**: D3D12 (Windows), Metal (macOS), Vulkan (Linux)
- **No Rust dependency** — unlike wgpu-native

### Platform Support

| Platform | Underlying API | Surface Creation |
|----------|---------------|-----------------|
| Windows  | D3D12         | Win32 HWND      |
| macOS    | Metal         | CAMetalLayer    |
| Linux    | Vulkan        | X11/Wayland     |
| Web      | Browser WebGPU| HTML Canvas     |

### Shader Pipeline

```
GLSL 4.5 → glslangValidator → SPIR-V → Tint (Dawn built-in) → WGSL → Dawn
```

Dawn's **Tint** compiler handles SPIR-V → WGSL transpilation via `tint::SpirvToWgsl()`. Tint is bundled with Dawn and requires no additional dependencies. SPIRV-Cross is **not** used for WebGPU — it is Metal-only (SPIR-V → MSL).

---

## Architecture

### WebGPU Context

All WebGPU classes share a `WebGPUContext` struct passed by reference:

```cpp
// include/metagfx/rhi/webgpu/WebGPUTypes.h
struct WebGPUContext {
    wgpu::Instance instance = nullptr;
    wgpu::Adapter  adapter  = nullptr;
    wgpu::Device   device   = nullptr;
    wgpu::Queue    queue    = nullptr;
    wgpu::Surface  surface  = nullptr;

    // Queried device limits
    uint32 maxBindGroups                  = 4;
    uint32 maxUniformBufferBindingSize    = 65536;
    uint32 minUniformBufferOffsetAlignment = 256;

    // Feature flags
    bool supportsTimestampQueries  = false;
    bool supportsDepthClipControl  = false;
    bool supportsBGRA8UnormStorage = true;
};
```

This mirrors the pattern used by the Vulkan (`VulkanContext`) and Metal (`MetalContext`) backends.

### File Structure

```
include/metagfx/rhi/webgpu/
├── WebGPUTypes.h              # Context struct, format conversion declarations
├── WebGPUDevice.h             # GraphicsDevice implementation
├── WebGPUSwapChain.h          # Swap chain and presentation
├── WebGPUBuffer.h             # Vertex, index, uniform buffers
├── WebGPUTexture.h            # Textures and views (2D, cubemap, depth)
├── WebGPUSampler.h            # Sampler state
├── WebGPUShader.h             # SPIR-V → WGSL compilation (via Tint)
├── WebGPUPipeline.h           # Graphics pipeline state (render pipeline)
├── WebGPUCommandBuffer.h      # Command encoder wrapper
├── WebGPUFramebuffer.h        # Render target references
└── WebGPUDescriptorSet.h      # Bind group wrapper

src/rhi/webgpu/
├── WebGPUTypes.cpp            # Format conversion utilities
├── WebGPUDevice.cpp           # Device init, resource factory
├── WebGPUSwapChain.cpp        # Presentation and surface config
├── WebGPUBuffer.cpp           # Buffer allocation and upload
├── WebGPUTexture.cpp          # Texture creation and upload
├── WebGPUSampler.cpp          # Sampler creation
├── WebGPUShader.cpp           # Tint SPIR-V → WGSL compilation
├── WebGPUPipeline.cpp         # Render pipeline state objects
├── WebGPUCommandBuffer.cpp    # Command encoder implementation
├── WebGPUFramebuffer.cpp      # Framebuffer setup
├── WebGPUDescriptorSet.cpp    # Bind group management
├── WebGPUSurfaceBridge.cpp    # Platform surface abstraction
└── WebGPUSurfaceBridge_Metal.mm  # CAMetalLayer → WGPUSurface (macOS)
```

---

## Device Initialization

WebGPU uses an **async/callback model** for device creation. On native platforms, Dawn processes events synchronously using a polling loop:

```cpp
// Request adapter (GPU selection)
m_Context.instance.RequestAdapter(
    &adapterOpts,
    [](WGPURequestAdapterStatus status, WGPUAdapter adapter, ...) {
        if (status == WGPURequestAdapterStatus_Success) {
            *data->adapter = wgpu::Adapter::Acquire(adapter);
        }
        data->done = true;
    },
    &data
);

// Block until callback fires (native only)
while (!data.done) {
    wgpuInstanceProcessEvents(instance);
}
```

The same pattern applies to device creation (`RequestDevice`). On web (Emscripten), the blocking loop is omitted and the application yields to the browser's event loop.

### Device Lost Callback

The device-lost callback distinguishes between **expected** and **unexpected** reasons:

```cpp
auto lostCallback = [](WGPUDevice const*, WGPUDeviceLostReason reason,
                       WGPUStringView message, void*, void*) {
    // Destroyed / CallbackCancelled are expected during normal shutdown
    if (reason == WGPUDeviceLostReason_Destroyed ||
        reason == WGPUDeviceLostReason_CallbackCancelled) {
        METAGFX_INFO << "WebGPU Device Lost [" << reasonStr << "]: " << msgStr;
    } else {
        METAGFX_ERROR << "WebGPU Device Lost [" << reasonStr << "]: " << msgStr;
    }
};
```

This prevents the misleading `[ERROR]: WebGPU Device Lost [Destroyed]` message that would otherwise appear during normal application shutdown.

---

## Shader Compilation (SPIR-V → WGSL)

`WebGPUShader` uses Dawn's **Tint** library (`tint::SpirvToWgsl`) to transpile SPIR-V to WGSL at runtime:

```cpp
// src/rhi/webgpu/WebGPUShader.cpp
tint::Initialize();  // idempotent

tint::wgsl::writer::Options wgslOptions{};
wgslOptions.allowed_features = tint::wgsl::AllowedFeatures::Everything();
auto wgslResult = tint::SpirvToWgsl(spirvData, wgslOptions);

m_WGSLSource = wgslResult.Get();

WGPUShaderSourceWGSL wgslSource{};
wgslSource.chain.sType = WGPUSType_ShaderSourceWGSL;
wgslSource.code.data   = m_WGSLSource.c_str();
wgslSource.code.length = m_WGSLSource.length();

WGPUShaderModuleDescriptor moduleDesc{};
moduleDesc.nextInChain = &wgslSource.chain;
m_Module = wgpu::ShaderModule::Acquire(wgpuDeviceCreateShaderModule(device, &moduleDesc));
```

WGSL source is stored in `m_WGSLSource` for debugging. Tint is bundled with Dawn and requires no separate dependency — contrast with Metal which uses SPIRV-Cross for SPIR-V → MSL.

### Binding Remapping

Tint imposes stricter rules than Vulkan (e.g., no gaps in binding indices within a group). The application uses per-API binding constants (`BINDING(api, slot)`) to account for these remappings in descriptor set layout construction.

---

## Resource Binding: Bind Groups

WebGPU uses **bind groups** (equivalent to Vulkan descriptor sets). `WebGPUDescriptorSet` wraps a `wgpu::BindGroup` + `wgpu::BindGroupLayout`:

### Layout Creation

```cpp
std::vector<wgpu::BindGroupLayoutEntry> entries;
for (auto& binding : desc.bindings) {
    wgpu::BindGroupLayoutEntry entry{};
    entry.binding    = binding.binding;
    entry.visibility = ToWGPUShaderStage(binding.stageFlags);

    if (binding.type == DescriptorType::UniformBuffer) {
        entry.buffer.type = wgpu::BufferBindingType::Uniform;
        entry.buffer.minBindingSize = binding.bufferSize;
    } else if (binding.type == DescriptorType::CombinedImageSampler) {
        if (isCubemap) {
            entry.texture.viewDimension = wgpu::TextureViewDimension::Cube;
        } else {
            entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
        }
        entry.texture.sampleType = wgpu::TextureSampleType::Float;
    } else if (binding.type == DescriptorType::Sampler) {
        entry.sampler.type = wgpu::SamplerBindingType::Filtering;
    }
    entries.push_back(entry);
}
```

### Bind Group Creation

After resources are bound via `BindUniformBuffer()` / `BindTexture()` / `BindSampler()`, calling `Update()` creates the `wgpu::BindGroup`:

```cpp
wgpu::BindGroupDescriptor groupDesc{};
groupDesc.layout     = m_Layout;
groupDesc.entryCount = entries.size();
groupDesc.entries    = entries.data();
m_BindGroup = m_Context.device.CreateBindGroup(&groupDesc);
```

### Binding During Rendering

```cpp
m_RenderPass.SetBindGroup(groupIndex, bindGroup, 0, nullptr);
```

---

## Push Constants Emulation

WebGPU has no native push constants. MetaGFX emulates them using a small uniform buffer uploaded each frame via `wgpu::Queue::WriteBuffer`:

```cpp
// WebGPUCommandBuffer.h
static constexpr uint32 MAX_PUSH_CONSTANT_SIZE = 128;
uint8  m_PushConstantBuffer[MAX_PUSH_CONSTANT_SIZE] = {};
uint32 m_PushConstantSize = 0;
wgpu::Buffer    m_PushConstantGPUBuffer;
wgpu::BindGroup m_PushConstantBindGroup;
```

Before every draw call, staged data is flushed:

```cpp
void WebGPUCommandBuffer::FlushPushConstants() {
    if (m_PushConstantSize == 0) return;
    m_Context.queue.WriteBuffer(m_PushConstantGPUBuffer, 0,
                                m_PushConstantBuffer, m_PushConstantSize);
    m_RenderPass.SetBindGroup(PUSH_CONSTANT_GROUP_INDEX,
                              m_PushConstantBindGroup, 0, nullptr);
}
```

This is architecturally similar to Metal's `setBytes` approach, which also writes small data directly without a staging buffer.

---

## Cubemap Texture Upload

WebGPU requires each **face × mip level** to be uploaded separately using `wgpuQueueWriteTexture`. The origin's `z` component selects the array layer (cubemap face):

```cpp
// src/rhi/webgpu/WebGPUTexture.cpp
for (uint32 face = 0; face < m_ArrayLayers; ++face) {       // 6 faces
    for (uint32 mip = 0; mip < m_MipLevels; ++mip) {
        // Compute mip dimensions and align bytesPerRow to 256
        uint32 unalignedBytesPerRow = mipWidth * bytesPerPixel;
        uint32 alignedBytesPerRow   = (unalignedBytesPerRow + 255) & ~255;

        WGPUOrigin3D origin{};
        origin.z = face;  // Cubemap face as array layer

        WGPUTexelCopyTextureInfo destination{};
        destination.mipLevel = mip;
        destination.origin   = origin;

        WGPUExtent3D writeSize{};
        writeSize.depthOrArrayLayers = 1;  // One face at a time

        wgpuQueueWriteTexture(queue, &destination, uploadData,
                              uploadSize, &dataLayout, &writeSize);

        srcOffset += faceSize;  // DDS is face-major: face 0 all mips, face 1 all mips...
    }
}
```

**DDS face layout**: DDS stores cubemaps face-major — all mips for face 0, then all mips for face 1, etc. The loop structure matches this.

**Row alignment**: WebGPU requires `bytesPerRow` to be a multiple of 256. When the natural row size is not aligned, rows are repacked into a temporary buffer with padding.

### Cubemap Texture View

The texture view uses `Cube` dimension so the WGSL `textureSample` call receives it as a `texture_cube`:

```cpp
wgpu::TextureViewDescriptor viewDesc{};
viewDesc.dimension      = wgpu::TextureViewDimension::Cube;
viewDesc.arrayLayerCount = 6;
```

---

## Swap Chain and Presentation

`WebGPUSwapChain` wraps a `wgpu::Surface` and configures it via `wgpu::SurfaceConfiguration`:

```cpp
wgpu::SurfaceConfiguration config{};
config.device      = m_Context.device;
config.format      = m_SurfaceFormat;  // Queried via GetCapabilities
config.usage       = wgpu::TextureUsage::RenderAttachment;
config.width       = width;
config.height      = height;
config.presentMode = wgpu::PresentMode::Fifo;  // VSync
m_Context.surface.Configure(&config);
```

**Getting the back buffer** per frame:

```cpp
wgpu::SurfaceTexture surfaceTexture{};
m_Context.surface.GetCurrentTexture(&surfaceTexture);
// Wrap in WebGPUTexture for RHI interface
```

**Presentation** is triggered by `surface.Present()` after command submission.

---

## Surface Creation (Platform Bridge)

The `CreateWebGPUSurfaceFromWindow` function in `WebGPUSurfaceBridge` handles platform-specific surface creation.

### macOS (Metal Layer)

```objc
// src/rhi/webgpu/WebGPUSurfaceBridge_Metal.mm
SDL_MetalView view = SDL_Metal_CreateView(window);
CA::MetalLayer* layer = (CA::MetalLayer*)SDL_Metal_GetLayer(view);

WGPUSurfaceSourceMetalLayer chainedDesc{};
chainedDesc.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
chainedDesc.layer = layer;

WGPUSurfaceDescriptor surfaceDesc{};
surfaceDesc.nextInChain = &chainedDesc.chain;
return wgpuInstanceCreateSurface(instance.Get(), &surfaceDesc);
```

The bridge is a `.mm` file (Objective-C++) since SDL's Metal view API requires Objective-C calls. The public interface (`WebGPUSurfaceBridge.h`) is pure C++.

---

## ImGui Integration

ImGui's WebGPU backend (`imgui_impl_wgpu`) is initialized with the Dawn device:

```cpp
ImGui_ImplSDL3_InitForOther(m_Window);

ImGui_ImplWGPU_InitInfo initInfo{};
initInfo.Device              = context.device.Get();
initInfo.NumFramesInFlight   = 2;
initInfo.RenderTargetFormat  = WGPUTextureFormat_BGRA8Unorm;
initInfo.DepthStencilFormat  = WGPUTextureFormat_Depth32Float;
ImGui_ImplWGPU_Init(&initInfo);
```

ImGui renders into a `WGPURenderPassEncoder` provided by the application's command buffer.

---

## Coordinate System

WebGPU uses the same clip space as Vulkan:
- **Y axis**: points down in NDC
- **Depth range**: [0, 1]
- **Winding**: Front face configurable

The existing GLSL shaders and projection matrices require **no modification** when targeting WebGPU — unlike Metal which requires a Y-flip.

---

## Format Conversion

Key format mappings (`WebGPUTypes.cpp`):

| MetaGFX Format | WebGPU Format |
|---------------|---------------|
| `R8G8B8A8_UNORM` | `RGBA8Unorm` |
| `B8G8R8A8_UNORM` | `BGRA8Unorm` |
| `R8G8B8A8_SRGB`  | `RGBA8UnormSrgb` |
| `R16G16B16A16_SFLOAT` | `RGBA16Float` |
| `R32G32B32A32_SFLOAT` | `RGBA32Float` |
| `D32_SFLOAT`  | `Depth32Float` |
| `D24_UNORM_S8_UINT` | `Depth24PlusStencil8` |

---

## Build Configuration

Enable the WebGPU backend with:

```bash
cmake .. -DMETAGFX_USE_WEBGPU=ON
```

Dawn is fetched via CMake's `FetchContent`. The build compiles all WebGPU sources conditionally under `METAGFX_USE_WEBGPU`.

---

## Debugging

### Dawn Validation Errors

Dawn prints WebGPU validation errors to the uncaptured error callback:

```
[ERROR]: WebGPU Error [Validation]: ...
```

Enable Dawn's debug output for more detail by building Dawn in Debug mode.

### WGSL Inspection

The compiled WGSL source is stored in `WebGPUShader::m_WGSLSource`. Add a temporary log statement to inspect it:

```cpp
METAGFX_INFO << "WGSL:\n" << m_WGSLSource;
```

This is useful for diagnosing binding remapping issues.

### RenderDoc

RenderDoc 1.26+ supports WebGPU captures on supported platforms.

---

## Comparison with Other Backends

| Aspect | Vulkan | Metal | WebGPU |
|--------|--------|-------|--------|
| Resource binding | Descriptor sets | Argument buffers / direct | Bind groups |
| Shaders | SPIR-V (direct) | MSL (via SPIRV-Cross) | WGSL (via Tint)        |
| Push constants | Native | `setBytes` | Uniform buffer emulation |
| Memory management | Explicit (VMA) | Automatic (resource modes) | Automatic |
| Synchronization | Fences + semaphores | Semaphores | Promises (async API) |
| Surface | VkSurface | CAMetalLayer | WGPUSurface |
| Presentation | vkQueuePresentKHR | presentDrawable | surface.Present() |
| Clip space Y | Down | Up (Y-flip required) | Down (no adjustment) |

---

**See Also**:
- [RHI Design](rhi.md) - Abstract interfaces
- [Metal Implementation](metal.md) - Metal backend (similar push constants pattern)
- [Vulkan Implementation](vulkan.md) - Vulkan backend
- [Skybox System](skybox_system.md) - Cubemap rendering
