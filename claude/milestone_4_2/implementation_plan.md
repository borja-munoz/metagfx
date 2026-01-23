# Plan: Milestone 4.2 - WebGPU Backend Implementation

## Summary

Implement a WebGPU backend for MetaGFX using **Google's Dawn** (C++ native implementation), supporting both **native desktop** (Windows/macOS/Linux) and **web** (Emscripten) platforms. The implementation will follow the **Metal backend's architectural pattern** (simple, direct bindings) and use **SPIRV-Cross** (already integrated) for SPIR-V → WGSL shader compilation.

This milestone validates the RHI's multi-API design with a third backend and enables cross-platform support including web deployment.

## Design Decisions

### 1. Library Choice: Google's Dawn
- **Pure C++ API** (`webgpu/webgpu_cpp.h`) - perfect alignment with C++20 codebase
- **Official W3C WebGPU implementation** - actively maintained, used in Chrome
- **Multi-platform support**:
  - Windows (D3D12 backend)
  - macOS (Metal backend)
  - Linux (Vulkan backend)
  - Web (Emscripten)
- **Already proven** in your `external/imgui/examples/example_glfw_wgpu/` integration
- No Rust dependency (unlike wgpu-native)

### 2. Platform Strategy: Simultaneous Native + Web Support
- Develop with **both targets** in mind from the start
- Use **conditional compilation** for platform-specific code
- Native desktop as primary development target for debugging
- Web builds for deployment validation

**Platform-specific surface creation**:
- Windows: D3D12 surface via `SDL_GetProperty(SDL_PROP_WINDOW_WIN32_HWND_POINTER)`
- macOS: Metal surface via `SDL_Metal_GetLayer()`
- Linux: Vulkan/X11 surface via `SDL_GetProperty(SDL_PROP_WINDOW_X11_WINDOW_NUMBER)`
- Web: HTML Canvas via Emscripten

### 3. Shader Pipeline: SPIRV-Cross Extension
- **Leverage existing SPIRV-Cross integration** (already configured for Metal)
- Add WGSL backend support: `spirv_cross::CompilerWGSL`
- No additional dependencies needed
- Consistent with Metal's SPIR-V → MSL transpilation pattern

**Shader workflow**:
```
GLSL 4.5 → glslangValidator → SPIR-V → SPIRV-Cross → WGSL → Dawn
```

### 4. API Design: Metal-Like Pattern
- WebGPU API is closer to Metal than Vulkan (encoders, bind groups, automatic memory management)
- Follow Metal's **direct resource binding** approach (no explicit descriptor pools)
- Use **encoder-based command recording** (GPURenderPassEncoder)
- Simplified memory model (GPU-managed, staging buffers for CPU→GPU)

**Key similarities to Metal**:
- Bind groups ≈ Argument buffers (structured resource binding)
- Pipeline layout integrated into pipeline (not separate object)
- Automatic memory coherence (no explicit flushing)
- Command encoding via render/compute pass encoders

## Architecture

### WebGPU Context Structure

```cpp
// include/metagfx/rhi/webgpu/WebGPUTypes.h
struct WebGPUContext {
    wgpu::Instance instance = nullptr;
    wgpu::Adapter adapter = nullptr;
    wgpu::Device device = nullptr;
    wgpu::Queue queue = nullptr;
    wgpu::Surface surface = nullptr;

    // Device capabilities (queried at init)
    bool supportsTimestampQueries = false;
    bool supportsDepthClipControl = false;
    bool supportsBGRA8UnormStorage = false;

    // Limits
    uint32_t maxBindGroups = 4;
    uint32_t maxUniformBufferBindingSize = 65536;
};
```

All WebGPU backend classes receive a reference to this context and pass it through the hierarchy (same pattern as Metal/Vulkan).

### File Organization

Following the proven Metal backend structure:

```
include/metagfx/rhi/webgpu/
├── WebGPUTypes.h              # Context, enums, format conversions
├── WebGPUDevice.h             # GraphicsDevice implementation
├── WebGPUSwapChain.h          # Presentation surface management
├── WebGPUBuffer.h             # GPU buffer (vertex, index, uniform)
├── WebGPUTexture.h            # Textures and views
├── WebGPUSampler.h            # Sampler state
├── WebGPUShader.h             # SPIR-V → WGSL compilation
├── WebGPUPipeline.h           # Graphics pipeline state
├── WebGPUCommandBuffer.h      # Command recording (encoder pattern)
├── WebGPUFramebuffer.h        # Render target management
├── WebGPUDescriptorSet.h      # Bind group wrapper
└── WebGPUSurfaceBridge.h      # Platform surface creation (C++ interface)

src/rhi/webgpu/
├── WebGPUTypes.cpp            # Format conversion utilities
├── WebGPUDevice.cpp           # Device init, resource factory
├── WebGPUSwapChain.cpp        # Presentation and frame pacing
├── WebGPUBuffer.cpp           # Buffer allocation, staging
├── WebGPUTexture.cpp          # Texture creation, views
├── WebGPUSampler.cpp          # Sampler creation
├── WebGPUShader.cpp           # Shader compilation (SPIRV-Cross)
├── WebGPUPipeline.cpp         # Pipeline state objects
├── WebGPUCommandBuffer.cpp    # Command encoder implementation
├── WebGPUFramebuffer.cpp      # Framebuffer setup
├── WebGPUDescriptorSet.cpp    # Bind group management
├── WebGPUSurfaceBridge.cpp    # Platform abstraction layer
├── WebGPUSurfaceBridge_Windows.cpp  # Win32 HWND → WGPUSurface
├── WebGPUSurfaceBridge_Metal.mm     # CAMetalLayer → WGPUSurface
├── WebGPUSurfaceBridge_Linux.cpp    # X11/Wayland → WGPUSurface
└── WebGPUSurfaceBridge_Web.cpp      # Emscripten Canvas → WGPUSurface
```

**Total**: 11 header files + 15 implementation files (12 .cpp, 1 .mm for macOS, 4 platform-specific bridge files)

## Implementation Phases

### Phase 1: Infrastructure Setup (Week 1)

**Goal**: Set up Dawn dependency and basic WebGPU context creation

#### Files to Create/Modify:

1. **external/CMakeLists.txt**
   - Add Dawn as FetchContent dependency
   - Configure Dawn build options
   - Link Dawn libraries to metagfx_rhi

2. **CMakeLists.txt** (root)
   - Add `option(METAGFX_USE_WEBGPU "Enable WebGPU support" OFF)`
   - Add platform detection for web builds (Emscripten)

3. **src/rhi/CMakeLists.txt**
   - Add WebGPU source files with conditional compilation
   - Link Dawn libraries
   - Add compile definition `METAGFX_USE_WEBGPU`

4. **include/metagfx/rhi/webgpu/WebGPUTypes.h**
   - Define `WebGPUContext` struct
   - Format conversion declarations:
     - `wgpu::TextureFormat ToWebGPUTextureFormat(Format format)`
     - `wgpu::IndexFormat ToWebGPUIndexFormat(Format format)`
     - `wgpu::BufferUsage ToWebGPUBufferUsage(BufferUsage usage)`
     - `wgpu::AddressMode ToWebGPUAddressMode(SamplerAddressMode mode)`
     - `wgpu::FilterMode ToWebGPUFilterMode(Filter filter)`
     - `wgpu::CompareFunction ToWebGPUCompareFunction(CompareOp op)`
     - `wgpu::PrimitiveTopology ToWebGPUPrimitiveTopology(PrimitiveTopology topology)`
     - `wgpu::CullMode ToWebGPUCullMode(CullMode mode)`
     - `wgpu::FrontFace ToWebGPUFrontFace(FrontFace face)`

5. **src/rhi/webgpu/WebGPUTypes.cpp**
   - Implement all format conversion functions
   - Reference Metal implementation pattern for consistency

6. **src/rhi/webgpu/WebGPUSurfaceBridge.h/.cpp**
   - C++ interface: `wgpu::Surface CreateWebGPUSurfaceFromWindow(SDL_Window* window, wgpu::Instance instance)`
   - Platform detection and delegation to platform-specific implementations

7. **Platform-specific surface bridges**:
   - `WebGPUSurfaceBridge_Windows.cpp`: Win32 HWND → WGPUSurface
   - `WebGPUSurfaceBridge_Metal.mm`: CAMetalLayer → WGPUSurface (Objective-C++)
   - `WebGPUSurfaceBridge_Linux.cpp`: X11/Wayland → WGPUSurface
   - `WebGPUSurfaceBridge_Web.cpp`: HTML Canvas (Emscripten-specific)

**Deliverable**: Build system configured, Dawn integrated, basic surface creation working

---

### Phase 2: Device and Resource Foundation (Week 2)

**Goal**: Implement core device and buffer/texture resources

#### Files to Create:

1. **include/metagfx/rhi/webgpu/WebGPUDevice.h**
   - Inherits from `GraphicsDevice` abstract interface
   - Stores `WebGPUContext`
   - Resource factory methods (CreateBuffer, CreateTexture, CreateShader, etc.)
   - Device capabilities querying

2. **src/rhi/webgpu/WebGPUDevice.cpp**
   - **Device Initialization**:
     - Create `wgpu::Instance`
     - Request `wgpu::Adapter` with power preference
     - Request `wgpu::Device` with required limits/features
     - Create `wgpu::Queue`
     - Create `wgpu::Surface` via platform bridge
   - **Factory Implementation**:
     - Instantiate WebGPU resource classes
     - Return via `Ref<>` smart pointers

3. **include/metagfx/rhi/webgpu/WebGPUBuffer.h**
   - Implements `Buffer` abstract interface
   - Stores `wgpu::Buffer` handle
   - Staging buffer support for CPU→GPU transfers

4. **src/rhi/webgpu/WebGPUBuffer.cpp**
   - **Buffer Creation**:
     - Map BufferUsage to `wgpu::BufferUsage` flags
     - Create buffer with appropriate size and usage
   - **Data Upload**:
     - Use `queue.writeBuffer()` for small updates
     - Use staging buffer + copy command for large transfers
   - **Mapping** (read-only, for debugging):
     - Use `buffer.mapAsync()` with callback pattern

5. **include/metagfx/rhi/webgpu/WebGPUTexture.h**
   - Implements `Texture` abstract interface
   - Stores `wgpu::Texture` and `wgpu::TextureView`
   - Supports 2D textures, cubemaps, depth textures

6. **src/rhi/webgpu/WebGPUTexture.cpp**
   - **Texture Creation**:
     - Map Format to `wgpu::TextureFormat`
     - Create texture descriptor with dimensions, mip levels, usage
     - Create default texture view
   - **Data Upload**:
     - Use `queue.writeTexture()` for image data
     - Support mipmap generation if needed

7. **include/metagfx/rhi/webgpu/WebGPUSampler.h**
   - Implements `Sampler` abstract interface
   - Stores `wgpu::Sampler` handle

8. **src/rhi/webgpu/WebGPUSampler.cpp**
   - **Sampler Creation**:
     - Map filter modes (linear, nearest)
     - Map address modes (repeat, clamp, mirror)
     - Support comparison samplers for shadow mapping

**Deliverable**: Device creation, buffer/texture/sampler resources functional

---

### Phase 3: Shader Compilation and Pipelines (Week 3)

**Goal**: Implement shader compilation (SPIR-V → WGSL) and graphics pipelines

#### Files to Create:

1. **include/metagfx/rhi/webgpu/WebGPUShader.h**
   - Implements `Shader` abstract interface
   - Stores `wgpu::ShaderModule`
   - Stores WGSL source code (for debugging)

2. **src/rhi/webgpu/WebGPUShader.cpp**
   - **SPIR-V to WGSL Compilation**:
     ```cpp
     spirv_cross::CompilerWGSL compiler(spirvData);
     spirv_cross::CompilerWGSL::Options options;
     // Configure options (binding remapping, etc.)
     compiler.set_wgsl_options(options);
     std::string wgslSource = compiler.compile();
     ```
   - **Shader Module Creation**:
     ```cpp
     wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
     wgslDesc.code = wgslSource.c_str();
     wgpu::ShaderModuleDescriptor moduleDesc{};
     moduleDesc.nextInChain = &wgslDesc;
     m_Module = device.createShaderModule(moduleDesc);
     ```
   - **Entry Point**: Store entry point name (usually "main")

3. **include/metagfx/rhi/webgpu/WebGPUPipeline.h**
   - Implements `Pipeline` abstract interface
   - Stores `wgpu::RenderPipeline`
   - Stores `wgpu::PipelineLayout`

4. **src/rhi/webgpu/WebGPUPipeline.cpp**
   - **Pipeline Layout Creation** (if using bind groups):
     ```cpp
     std::vector<wgpu::BindGroupLayout> layouts = { /* from descriptor set */ };
     wgpu::PipelineLayoutDescriptor layoutDesc{};
     layoutDesc.bindGroupLayoutCount = layouts.size();
     layoutDesc.bindGroupLayouts = layouts.data();
     m_Layout = device.createPipelineLayout(layoutDesc);
     ```
   - **Graphics Pipeline Creation**:
     - Vertex state (buffer layout, attributes)
     - Primitive state (topology, culling, front face)
     - Depth/stencil state
     - Multisample state
     - Fragment state (color targets, blend modes)
     - Pipeline layout
   - **Unified Pipeline Object** (similar to Metal's combined PSO)

**Deliverable**: Shader compilation working, graphics pipelines created

---

### Phase 4: Command Recording and Rendering (Week 4)

**Goal**: Implement command buffer recording, render pass management, and drawing

#### Files to Create:

1. **include/metagfx/rhi/webgpu/WebGPUCommandBuffer.h**
   - Implements `CommandBuffer` abstract interface
   - Stores `wgpu::CommandEncoder`
   - Stores `wgpu::RenderPassEncoder` (active during rendering)
   - Push constant staging buffer (similar to Metal)

2. **src/rhi/webgpu/WebGPUCommandBuffer.cpp**
   - **Begin()**: Create `wgpu::CommandEncoder`
   - **BeginRendering()**:
     - Create `wgpu::RenderPassDescriptor` with color/depth attachments
     - Configure load/store ops and clear values
     - Create `wgpu::RenderPassEncoder`
   - **BindPipeline()**: Set render pipeline on encoder
   - **BindVertexBuffer()**: `setVertexBuffer(slot, buffer, offset, size)`
   - **BindIndexBuffer()**: `setIndexBuffer(buffer, format, offset, size)`
   - **SetViewport()**: `setViewport(x, y, w, h, minDepth, maxDepth)`
   - **SetScissor()**: `setScissorRect(x, y, w, h)`
   - **Draw()**: `draw(vertexCount, instanceCount, firstVertex, firstInstance)`
   - **DrawIndexed()**: `drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance)`
   - **EndRendering()**: `renderPass.end()`, set encoder to nullptr
   - **End()**: `m_CommandBuffer = encoder.finish()`
   - **Push Constants**:
     - WebGPU doesn't have native push constants
     - Use small uniform buffer with dynamic offset (similar to Metal's `setBytes` pattern)
     - Allocate buffer, update via `queue.writeBuffer()`, bind as dynamic uniform

3. **include/metagfx/rhi/webgpu/WebGPUDescriptorSet.h**
   - Implements `DescriptorSet` abstract interface
   - Stores `wgpu::BindGroup`
   - Stores `wgpu::BindGroupLayout`

4. **src/rhi/webgpu/WebGPUDescriptorSet.cpp**
   - **Bind Group Layout Creation**:
     ```cpp
     std::vector<wgpu::BindGroupLayoutEntry> entries;
     for (auto& binding : m_Bindings) {
         wgpu::BindGroupLayoutEntry entry{};
         entry.binding = binding.binding;
         entry.visibility = ToWGPUShaderStage(binding.stageFlags);
         // Configure buffer/texture/sampler based on type
         entries.push_back(entry);
     }
     wgpu::BindGroupLayoutDescriptor layoutDesc{};
     layoutDesc.entryCount = entries.size();
     layoutDesc.entries = entries.data();
     m_Layout = device.createBindGroupLayout(layoutDesc);
     ```
   - **Bind Group Creation**:
     ```cpp
     std::vector<wgpu::BindGroupEntry> entries;
     for (auto& binding : m_Bindings) {
         wgpu::BindGroupEntry entry{};
         entry.binding = binding.binding;
         if (binding.buffer) {
             entry.buffer = webgpuBuffer->GetHandle();
             entry.offset = 0;
             entry.size = buffer->GetSize();
         } else if (binding.texture) {
             entry.textureView = webgpuTexture->GetView();
         } else if (binding.sampler) {
             entry.sampler = webgpuSampler->GetHandle();
         }
         entries.push_back(entry);
     }
     wgpu::BindGroupDescriptor groupDesc{};
     groupDesc.layout = m_Layout;
     groupDesc.entryCount = entries.size();
     groupDesc.entries = entries.data();
     m_BindGroup = device.createBindGroup(groupDesc);
     ```
   - **Binding During Rendering**:
     ```cpp
     renderPass.setBindGroup(groupIndex, bindGroup, dynamicOffsets);
     ```

5. **include/metagfx/rhi/webgpu/WebGPUFramebuffer.h**
   - Implements `Framebuffer` abstract interface
   - Stores color/depth texture references
   - WebGPU doesn't have explicit framebuffer objects (render pass creates them)

6. **src/rhi/webgpu/WebGPUFramebuffer.cpp**
   - **Minimal Implementation**:
     - Store texture references
     - Validate attachment formats
     - Used during `BeginRendering()` to create render pass descriptor

**Deliverable**: Command recording functional, drawing works

---

### Phase 5: Swap Chain and Presentation (Week 5)

**Goal**: Implement swap chain management and frame presentation

#### Files to Create:

1. **include/metagfx/rhi/webgpu/WebGPUSwapChain.h**
   - Implements `SwapChain` abstract interface
   - Stores `wgpu::SwapChain`
   - Stores current back buffer texture

2. **src/rhi/webgpu/WebGPUSwapChain.cpp**
   - **Swap Chain Creation**:
     ```cpp
     wgpu::SwapChainDescriptor swapChainDesc{};
     swapChainDesc.usage = wgpu::TextureUsage::RenderAttachment;
     swapChainDesc.format = preferredFormat;  // BGRA8Unorm or RGBA8Unorm
     swapChainDesc.width = width;
     swapChainDesc.height = height;
     swapChainDesc.presentMode = wgpu::PresentMode::Fifo;  // VSync
     m_SwapChain = device.createSwapChain(surface, swapChainDesc);
     ```
   - **Get Current Back Buffer**:
     ```cpp
     wgpu::TextureView view = m_SwapChain.getCurrentTextureView();
     // Wrap in WebGPUTexture for RHI interface
     return CreateRef<WebGPUTexture>(m_Context, view);
     ```
   - **Present**:
     ```cpp
     // WebGPU presents automatically when command buffer is submitted
     // No explicit present call needed (unlike Vulkan/Metal)
     ```
   - **Resize**:
     - Destroy old swap chain
     - Create new swap chain with updated dimensions

**Deliverable**: Swap chain functional, rendering displays on screen

---

### Phase 6: Integration and Testing (Week 6)

**Goal**: Integrate WebGPU backend with Application, test all features

#### Files to Modify:

1. **src/rhi/GraphicsDevice.cpp**
   - Add WebGPU case to factory function:
     ```cpp
     #ifdef METAGFX_USE_WEBGPU
     case GraphicsAPI::WebGPU:
         return CreateRef<WebGPUDevice>(static_cast<SDL_Window*>(nativeWindowHandle));
     #endif
     ```

2. **src/app/Application.cpp**
   - **ImGui Integration**:
     ```cpp
     #ifdef METAGFX_USE_WEBGPU
     if (api == GraphicsAPI::WebGPU) {
         auto webgpuDevice = std::static_pointer_cast<rhi::WebGPUDevice>(m_Device);
         auto& context = webgpuDevice->GetContext();

         ImGui_ImplSDL3_InitForOther(m_Window);
         ImGui_ImplWGPU_InitInfo initInfo{};
         initInfo.Device = context.device.Get();
         initInfo.NumFramesInFlight = 2;
         initInfo.RenderTargetFormat = WGPUTextureFormat_BGRA8Unorm;
         initInfo.DepthStencilFormat = WGPUTextureFormat_Depth32Float;
         ImGui_ImplWGPU_Init(&initInfo);
         return;
     }
     #endif
     ```
   - **Rendering Loop**:
     - No changes needed (RHI abstraction handles backend differences)
   - **Coordinate System**:
     - WebGPU uses same clip space as Vulkan (Y down, depth [0,1])
     - No projection matrix adjustment needed (unlike Metal)

3. **external/CMakeLists.txt**
   - Add ImGui WebGPU backend:
     ```cmake
     if(METAGFX_USE_WEBGPU)
         list(APPEND IMGUI_BACKEND_SOURCES
             ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp)
         target_link_libraries(imgui_backends PUBLIC dawn::dawn_cpp)
     endif()
     ```

#### Testing Tasks:

1. **Triangle Rendering Test**
   - Verify basic vertex/fragment shader pipeline
   - Confirm viewport, scissor, and clear color work

2. **Buffer and Texture Tests**
   - Upload vertex/index buffer data
   - Load textures from images (albedo, normal maps)
   - Verify sampler filtering modes

3. **PBR Material Test**
   - Load a complex model (e.g., DamagedHelmet.gltf)
   - Verify all texture slots work (albedo, normal, metallic-roughness, AO, emissive)
   - Check lighting calculations match Vulkan/Metal output

4. **Shadow Mapping Test**
   - Verify depth texture creation
   - Render shadow pass to depth texture
   - Sample shadow map in fragment shader
   - Confirm PCF filtering works

5. **IBL and Skybox Test**
   - Load environment cubemap
   - Render skybox with LOD control
   - Verify IBL diffuse/specular contributions

6. **ImGui Test**
   - Confirm UI panels render correctly
   - Test all controls (model selection, exposure, shadow bias, etc.)
   - Verify mouse/keyboard input works

7. **Cross-Platform Validation**
   - **Windows**: Test with D3D12 backend
   - **macOS**: Test with Metal backend
   - **Linux**: Test with Vulkan backend
   - **Web** (Emscripten): Build and test in browser

**Deliverable**: WebGPU backend fully functional, all features working, cross-platform validated

---

## CMake Configuration

### Root CMakeLists.txt

```cmake
option(METAGFX_USE_WEBGPU "Enable WebGPU support" OFF)

# Emscripten detection
if(EMSCRIPTEN)
    set(METAGFX_PLATFORM_WEB TRUE)
    set(METAGFX_USE_WEBGPU ON CACHE BOOL "" FORCE)
endif()
```

### external/CMakeLists.txt

```cmake
if(METAGFX_USE_WEBGPU)
    # Fetch Dawn from Google's repository
    include(FetchContent)
    FetchContent_Declare(
        dawn
        GIT_REPOSITORY https://dawn.googlesource.com/dawn
        GIT_TAG        chromium/6312  # Stable release tag
    )

    # Configure Dawn build options
    set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(DAWN_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(dawn)

    # ImGui WebGPU backend
    list(APPEND IMGUI_BACKEND_SOURCES
        ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp)

    target_link_libraries(imgui_backends PUBLIC
        dawn::webgpu_dawn
        dawn::dawn_cpp
    )
    target_include_directories(imgui_backends PRIVATE
        ${dawn_SOURCE_DIR}/include
    )
endif()
```

### src/rhi/CMakeLists.txt

```cmake
if(METAGFX_USE_WEBGPU)
    list(APPEND RHI_SOURCES
        webgpu/WebGPUTypes.cpp
        webgpu/WebGPUDevice.cpp
        webgpu/WebGPUSwapChain.cpp
        webgpu/WebGPUBuffer.cpp
        webgpu/WebGPUTexture.cpp
        webgpu/WebGPUSampler.cpp
        webgpu/WebGPUShader.cpp
        webgpu/WebGPUPipeline.cpp
        webgpu/WebGPUCommandBuffer.cpp
        webgpu/WebGPUFramebuffer.cpp
        webgpu/WebGPUDescriptorSet.cpp
        webgpu/WebGPUSurfaceBridge.cpp
    )

    # Platform-specific surface bridges
    if(WIN32)
        list(APPEND RHI_SOURCES webgpu/WebGPUSurfaceBridge_Windows.cpp)
    elseif(APPLE)
        list(APPEND RHI_SOURCES webgpu/WebGPUSurfaceBridge_Metal.mm)
    elseif(UNIX)
        list(APPEND RHI_SOURCES webgpu/WebGPUSurfaceBridge_Linux.cpp)
    elseif(EMSCRIPTEN)
        list(APPEND RHI_SOURCES webgpu/WebGPUSurfaceBridge_Web.cpp)
    endif()

    target_link_libraries(metagfx_rhi
        PUBLIC
            dawn::webgpu_dawn
            dawn::dawn_cpp
        PRIVATE
            spirv-cross-wgsl  # SPIR-V to WGSL compiler
    )

    target_compile_definitions(metagfx_rhi PUBLIC METAGFX_USE_WEBGPU)
endif()
```

## Key Implementation Details

### 1. Shader Compilation (SPIR-V → WGSL)

```cpp
// WebGPUShader.cpp
#include <spirv_wgsl.hpp>

WebGPUShader::WebGPUShader(WebGPUContext& context, const ShaderDesc& desc)
    : m_Context(context), m_Stage(desc.stage), m_EntryPoint(desc.entryPoint) {

    // SPIR-V bytecode from descriptor
    std::vector<uint32_t> spirvData(desc.code.size() / 4);
    std::memcpy(spirvData.data(), desc.code.data(), desc.code.size());

    // Compile SPIR-V to WGSL using SPIRV-Cross
    spirv_cross::CompilerWGSL compiler(spirvData);

    // Configure WGSL options
    spirv_cross::CompilerWGSL::Options options;
    options.emit_vertex_input_struct = true;
    compiler.set_wgsl_options(options);

    // Remap bindings if needed (WebGPU has different binding model)
    // For example, offset uniform buffers to avoid conflicts
    auto resources = compiler.get_shader_resources();
    for (auto& ubo : resources.uniform_buffers) {
        uint32_t binding = compiler.get_decoration(ubo.id, spv::DecorationBinding);
        compiler.set_decoration(ubo.id, spv::DecorationBinding, binding);  // Keep as-is or remap
    }

    // Compile to WGSL source
    std::string wgslSource = compiler.compile();

    // Create WebGPU shader module
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = wgslSource.c_str();

    wgpu::ShaderModuleDescriptor moduleDesc{};
    moduleDesc.nextInChain = &wgslDesc;
    moduleDesc.label = desc.debugName.c_str();

    m_Module = m_Context.device.CreateShaderModule(&moduleDesc);

    // Store WGSL source for debugging
    m_WGSLSource = std::move(wgslSource);
}
```

### 2. Push Constants Emulation

WebGPU doesn't have native push constants. Emulate using small dynamic uniform buffer:

```cpp
// WebGPUCommandBuffer.h
static constexpr uint32 MAX_PUSH_CONSTANT_SIZE = 128;
uint8 m_PushConstantBuffer[MAX_PUSH_CONSTANT_SIZE] = {};
uint32 m_PushConstantSize = 0;
wgpu::Buffer m_PushConstantGPUBuffer = nullptr;  // Small uniform buffer

// WebGPUCommandBuffer.cpp
void WebGPUCommandBuffer::PushConstants(Ref<Pipeline> pipeline, ShaderStage stages,
                                        uint32 offset, uint32 size, const void* data) {
    // Accumulate data in staging buffer (same pattern as Metal)
    memcpy(m_PushConstantBuffer + offset, data, size);
    m_PushConstantSize = std::max(m_PushConstantSize, offset + size);
}

void WebGPUCommandBuffer::FlushPushConstants() {
    if (!m_RenderPass || m_PushConstantSize == 0) return;

    // Write to GPU buffer
    m_Context.queue.WriteBuffer(m_PushConstantGPUBuffer, 0,
                                 m_PushConstantBuffer, m_PushConstantSize);

    // Bind as dynamic uniform buffer (binding 30, following Metal's pattern)
    m_RenderPass.SetBindGroup(1, m_PushConstantBindGroup, {0});  // Dynamic offset 0
}

// Call FlushPushConstants() before every draw call
void WebGPUCommandBuffer::Draw(uint32 vertexCount, ...) {
    FlushPushConstants();
    m_RenderPass.Draw(vertexCount, ...);
}
```

### 3. Synchronization (Async/Promise Model)

WebGPU uses callbacks for async operations. Wrap in synchronous API for RHI:

```cpp
// WebGPUDevice.cpp - Device creation
void WebGPUDevice::Initialize(SDL_Window* window) {
    // Create instance
    wgpu::InstanceDescriptor instanceDesc{};
    m_Context.instance = wgpu::CreateInstance(&instanceDesc);

    // Request adapter (async)
    struct UserData { wgpu::Adapter* adapter; bool done; };
    UserData userData = { &m_Context.adapter, false };

    wgpu::RequestAdapterOptions adapterOpts{};
    adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;

    m_Context.instance.RequestAdapter(
        &adapterOpts,
        [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
           const char* message, void* userdata) {
            auto* data = static_cast<UserData*>(userdata);
            if (status == WGPURequestAdapterStatus_Success) {
                *data->adapter = wgpu::Adapter::Acquire(adapter);
            }
            data->done = true;
        },
        &userData
    );

    // Wait for callback (native platforms)
#ifndef __EMSCRIPTEN__
    while (!userData.done) {
        m_Context.instance.ProcessEvents();
    }
#endif

    // Request device (async)
    struct DeviceData { wgpu::Device* device; bool done; };
    DeviceData deviceData = { &m_Context.device, false };

    wgpu::DeviceDescriptor deviceDesc{};
    m_Context.adapter.RequestDevice(
        &deviceDesc,
        [](WGPURequestDeviceStatus status, WGPUDevice device,
           const char* message, void* userdata) {
            auto* data = static_cast<DeviceData*>(userdata);
            if (status == WGPURequestDeviceStatus_Success) {
                *data->device = wgpu::Device::Acquire(device);
            }
            data->done = true;
        },
        &deviceData
    );

#ifndef __EMSCRIPTEN__
    while (!deviceData.done) {
        m_Context.adapter.ProcessEvents();
    }
#endif

    // Get queue
    m_Context.queue = m_Context.device.GetQueue();

    // Create surface
    m_Context.surface = CreateWebGPUSurfaceFromWindow(window, m_Context.instance);
}
```

### 4. Format Conversions

```cpp
// WebGPUTypes.cpp
wgpu::TextureFormat ToWebGPUTextureFormat(Format format) {
    switch (format) {
        case Format::R8G8B8A8_UNORM:    return wgpu::TextureFormat::RGBA8Unorm;
        case Format::B8G8R8A8_UNORM:    return wgpu::TextureFormat::BGRA8Unorm;
        case Format::R8G8B8A8_SRGB:     return wgpu::TextureFormat::RGBA8UnormSrgb;
        case Format::B8G8R8A8_SRGB:     return wgpu::TextureFormat::BGRA8UnormSrgb;
        case Format::D32_SFLOAT:        return wgpu::TextureFormat::Depth32Float;
        case Format::D24_UNORM_S8_UINT: return wgpu::TextureFormat::Depth24PlusStencil8;
        case Format::BC1_RGB_UNORM:     return wgpu::TextureFormat::BC1RGBAUnorm;
        case Format::BC3_UNORM:         return wgpu::TextureFormat::BC3RGBAUnorm;
        case Format::BC5_UNORM:         return wgpu::TextureFormat::BC5RGUnorm;
        // ... more formats
        default:
            WEBGPU_LOG_ERROR("Unsupported format: " << static_cast<int>(format));
            return wgpu::TextureFormat::RGBA8Unorm;
    }
}

wgpu::BufferUsage ToWebGPUBufferUsage(BufferUsage usage) {
    wgpu::BufferUsage result = wgpu::BufferUsage::None;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::Vertex))
        result |= wgpu::BufferUsage::Vertex;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::Index))
        result |= wgpu::BufferUsage::Index;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::Uniform))
        result |= wgpu::BufferUsage::Uniform;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::Storage))
        result |= wgpu::BufferUsage::Storage;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::TransferSrc))
        result |= wgpu::BufferUsage::CopySrc;
    if (static_cast<int>(usage) & static_cast<int>(BufferUsage::TransferDst))
        result |= wgpu::BufferUsage::CopyDst;
    return result;
}
```

## Verification

After implementation, verify the following:

### Functional Tests

1. **Build Test**
   ```bash
   # Native build
   mkdir build && cd build
   cmake .. -DMETAGFX_USE_WEBGPU=ON
   make -j$(nproc)

   # Web build (Emscripten)
   emcmake cmake .. -DMETAGFX_USE_WEBGPU=ON
   emmake make -j$(nproc)
   ```

2. **Triangle Rendering**
   ```bash
   ./metagfx
   # Verify colored triangle appears on screen
   ```

3. **PBR Model Rendering**
   - Load DamagedHelmet.gltf
   - Verify all textures load (albedo, normal, metallic-roughness, AO, emissive)
   - Compare visual output to Vulkan/Metal backends (should be identical)

4. **Shadow Mapping**
   - Enable shadow mapping in ImGui
   - Verify shadows render correctly
   - Adjust shadow bias, confirm updates work

5. **IBL and Skybox**
   - Enable IBL in ImGui
   - Adjust IBL intensity
   - Verify skybox renders
   - Adjust LOD, confirm changes

6. **ImGui Interaction**
   - Test all UI controls
   - Verify mouse/keyboard input
   - Switch models dynamically
   - Adjust exposure, shadow settings

### Cross-Platform Validation

1. **Windows (D3D12 Backend)**
   - Run on Windows 10/11
   - Verify D3D12 validation layer has no errors
   - Profile with PIX

2. **macOS (Metal Backend)**
   - Run on macOS 12+
   - Verify Metal API Validation passes
   - Capture GPU frame in Xcode

3. **Linux (Vulkan Backend)**
   - Run on Ubuntu/Fedora
   - Verify with Vulkan validation layers
   - Profile with RenderDoc

4. **Web (Emscripten)**
   - Open in Chrome/Firefox with WebGPU enabled
   - Verify rendering matches native
   - Test input handling in browser

### Performance Comparison

Compare frame times across backends:
- Vulkan (baseline)
- Metal
- WebGPU (native)
- WebGPU (web)

All should be within 10% of each other for the same scene.

## Risks and Mitigations

### Risk 1: Dawn Build Complexity
**Issue**: Dawn has many dependencies (depot_tools, Python, etc.)
**Mitigation**: Use FetchContent with DAWN_FETCH_DEPENDENCIES=ON

### Risk 2: SPIRV-Cross WGSL Support
**Issue**: WGSL backend may have bugs or missing features
**Mitigation**: Test thoroughly, file issues upstream, fallback to Tint if needed

### Risk 3: Async API Friction
**Issue**: WebGPU's async model doesn't match synchronous RHI
**Mitigation**: Blocking wait on native, event loop integration on web

### Risk 4: Web Platform Limitations
**Issue**: Browser WebGPU may have stricter limits than native
**Mitigation**: Query and respect device limits, provide fallbacks

### Risk 5: Platform-Specific Surface Creation
**Issue**: Each platform needs different surface creation code
**Mitigation**: Well-tested bridge pattern (similar to Metal's SDL bridge)

## Success Criteria

- ✅ WebGPU backend compiles on all platforms (Windows, macOS, Linux, Web)
- ✅ Triangle renders correctly
- ✅ All PBR features work (materials, textures, lighting)
- ✅ Shadow mapping functional with PCF filtering
- ✅ IBL and skybox render correctly
- ✅ ImGui integration complete and functional
- ✅ Performance within 10% of Vulkan/Metal backends
- ✅ Web builds run in browser with acceptable performance (30+ FPS)
- ✅ Zero visual differences compared to Vulkan/Metal (pixel-perfect)
- ✅ All features from Milestone 3.3 (Shadow Mapping) working on WebGPU

## Documentation

After implementation, create:

1. **docs/webgpu.md** - WebGPU backend implementation guide
   - Dawn integration
   - SPIR-V to WGSL shader compilation
   - Bind group patterns
   - Push constant emulation
   - Async/Promise handling
   - Platform surface bridges
   - Comparison with Vulkan/Metal

2. **Update docs/rhi.md** - Add WebGPU to backend comparison table

3. **Update docs/README.md** - Mark Milestone 4.2 as complete

4. **Update CLAUDE.md** - Update current status and next milestones

## Timeline Estimate

- **Week 1**: Infrastructure setup (CMake, Dawn, platform bridges)
- **Week 2**: Device and resources (buffers, textures, samplers)
- **Week 3**: Shaders and pipelines (SPIR-V→WGSL, graphics pipelines)
- **Week 4**: Command recording (encoders, render passes, drawing)
- **Week 5**: Swap chain and presentation
- **Week 6**: Integration, testing, debugging, optimization

**Total**: 6 weeks for full implementation and validation

## Dependencies

- Google Dawn (WebGPU C++ implementation)
- SPIRV-Cross WGSL backend (already integrated)
- SDL3 (already integrated)
- ImGui WebGPU backend (already available)
- Emscripten (for web builds, optional)

## Next Steps After Completion

After Milestone 4.2, the next milestones are:

- **Milestone 4.3**: Rendering Optimizations (frustum culling, LOD, instancing)
- **Milestone 5.1**: PBRT Scene Parser
- **Milestone 8.1**: Direct3D 12 Implementation (postponed to Phase 8)

The WebGPU backend validates the RHI's multi-API design with a third backend (after Vulkan and Metal) and enables web deployment, significantly expanding platform reach.
