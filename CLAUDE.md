# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MetaGFX is a backend-agnostic physically-based renderer implementing a common abstract core with multiple graphics API backends (Vulkan, Direct3D 12, Metal, WebGPU). The project is organized around a phased roadmap with milestones tracked in `claude/metagfx_roadmap.md`.

**Current Status**: Milestone 4.1 completed (Metal Backend Implementation). The renderer supports:
- **Multi-Backend Rendering**: Vulkan (Windows, Linux, macOS) and Metal (macOS)
- Physically-Based Rendering (PBR) with Cook-Torrance BRDF
- Real-time shadow mapping with PCF filtering from directional lights
- Model loading from various formats (OBJ, FBX, glTF, COLLADA)
- Full material system (albedo, roughness, metallic, emissive, normal maps)
- Image-Based Lighting (IBL) with environment maps
- Skybox rendering with LOD control
- Interactive UI controls (ImGui on both Vulkan and Metal)

## Build Commands

**Linux/macOS:**
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)  # Linux
make -j$(sysctl -n hw.ncpu)  # macOS
```

**Windows (Visual Studio):**
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Windows (MinGW):**
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build . --config Release
```

### Running the Application

```bash
cd build/bin
./metagfx  # Linux/macOS
metagfx.exe  # Windows
```

### CMake Build Options

```bash
cmake .. -DMETAGFX_USE_VULKAN=ON      # Enable Vulkan (default: ON) ✅ Fully supported
cmake .. -DMETAGFX_USE_METAL=ON       # Enable Metal (default: OFF) ✅ Fully supported
cmake .. -DMETAGFX_USE_D3D12=ON       # Enable D3D12 (default: OFF) - Planned
cmake .. -DMETAGFX_USE_WEBGPU=ON      # Enable WebGPU (default: OFF) - Planned
cmake .. -DMETAGFX_BUILD_TESTS=ON     # Build tests (default: OFF)
```

**Backend Selection**: On macOS, you can build with both Vulkan and Metal backends enabled. The backend is selected at device creation time in the application code.

### Shader Compilation

Shaders are located in `src/app/` and must be compiled to SPIR-V bytecode included as C++ headers:

```bash
# Compile vertex shader
glslangValidator -V src/app/model.vert -o src/app/model.vert.spv

# Compile fragment shader
glslangValidator -V src/app/model.frag -o src/app/model.frag.spv

# Convert SPIR-V to C++ include file
# The .spv files are then converted to .spv.inl files containing byte arrays
python3 convert_spv.py model.frag.spv model.frag.spv.inl
```

The compiled shaders (`.spv.inl` files) are included directly in the C++ source.

**Available Shaders**:
- `triangle.vert/frag` - Simple vertex color rendering
- `model.vert/frag` - Full vertex layout with normals and UVs, with PBR lighting and shadows
- `skybox.vert/frag` - Skybox rendering
- `shadowmap.vert/frag` - Shadow map depth-only rendering (fragment shader is empty)

## Architecture

### Modular Structure

The codebase follows a strict layered architecture:

```
┌─────────────────────────────────────────┐
│      Application Layer (src/app)        │  ← main.cpp, Application.cpp
├─────────────────────────────────────────┤
│   Renderer (src/renderer)               │  ← High-level rendering logic
├─────────────────────────────────────────┤
│   Scene (src/scene)                     │  ← Camera, Light, Mesh, Model 
├─────────────────────────────────────────┤
│   RHI - Render Hardware Interface       │  ← API-agnostic abstractions
│        (src/rhi)                        │
├────────┬────────────────────────────────┤
│ Vulkan │  D3D12  │  Metal  │  WebGPU    │  ← Backend implementations
└────────┴────────────────────────────────┘
```

### RHI (Render Hardware Interface)

The RHI is the core abstraction that enables multi-backend support. Key principles:

1. **API-Agnostic Design**: All interfaces use common terminology that maps to Vulkan, D3D12, Metal, and WebGPU
2. **Explicit Resource Management**: No hidden state, clear resource lifetimes using `Ref<T>` smart pointers
3. **Command Buffer Model**: Modern command recording pattern consistent across backends
4. **Type Safety**: Strong typing with enums, no void pointers in public interfaces

**Core RHI Abstractions** (`include/metagfx/rhi/`):
- `GraphicsDevice` - Device creation, resource factory, command submission
- `SwapChain` - Presentation and back buffer management
- `CommandBuffer` - Command recording (draw calls, state changes, etc.)
- `Buffer` - GPU buffers (vertex, index, uniform)
- `Texture` - Images and sampling (implemented)
- `Sampler` - Texture sampling configuration (implemented)
- `Shader` - Shader modules from SPIR-V bytecode
- `Pipeline` - Graphics pipeline state objects
- `Types.h` - Enums and structs (GraphicsAPI, BufferUsage, ShaderStage, Format, etc.)

**Backend Implementations**:
- **Vulkan** (`src/rhi/vulkan/`, `include/metagfx/rhi/vulkan/`) ✅ Complete
  - Full implementation: `VulkanDevice`, `VulkanSwapChain`, `VulkanBuffer`, etc.
  - Platforms: Windows, Linux, macOS
  - See [docs/vulkan.md](docs/vulkan.md) for details
- **Metal** (`src/rhi/metal/`, `include/metagfx/rhi/metal/`) ✅ Complete
  - Pure C++ using metal-cpp: `MetalDevice`, `MetalSwapChain`, `MetalBuffer`, etc.
  - Platforms: macOS (iOS-ready)
  - SPIR-V to MSL shader transpilation via SPIRV-Cross
  - See [docs/metal.md](docs/metal.md) for details
- Backend selection happens via factory function: `CreateGraphicsDevice(GraphicsAPI api, ...)`

### External Dependencies
- **Assimp**: 3D model loading (OBJ, FBX, glTF, COLLADA importers enabled)
- **GLM**: Mathematics library for vectors, matrices
- **imgui**: User interface components
- **SDL3**: Window management and input
- **stb**: Image loading (stb_image for PNG, JPEG, TGA, BMP)

### Smart Pointers Convention

The codebase uses type aliases for smart pointers:
- `Ref<T>` = `std::shared_ptr<T>` (used for RHI resources with shared ownership)
- `Scope<T>` = `std::unique_ptr<T>` (used for exclusive ownership)
- `CreateRef<T>(...)` = `std::make_shared<T>(...)`
- `CreateScope<T>(...)` = `std::make_unique<T>(...)`

### Scene System

**Camera** (`include/metagfx/scene/Camera.h`):
- Perspective and orthographic projection
- FPS-style movement (WASD + QE for up/down)
- Mouse look controls and scroll wheel zoom
- MVP (Model-View-Projection) matrix computation
- Camera uniform data passed to shaders via uniform buffers

**Mesh** (`include/metagfx/scene/Mesh.h`):
- Holds geometry data (vertices, indices)
- Manages GPU buffers (vertex buffer, index buffer)
- Vertex structure: position (vec3), normal (vec3), texCoord (vec2)
- Supports move semantics, non-copyable

**Model** (`include/metagfx/scene/Model.h`):
- Container for multiple meshes
- Loads models from files using Assimp (OBJ, FBX, glTF, COLLADA)
- Procedural geometry generation (CreateCube, CreateSphere)
- Processes Assimp scene graph recursively
- Extracts materials and textures from model files

**Material** (`include/metagfx/scene/Material.h`):
- Material properties: albedo (vec3), roughness (float), metallic (float)
- Optional albedo texture support with texture flags
- GPU-compatible std140 layout for uniform buffers
- Blinn-Phong lighting model (ambient, diffuse, specular)
- Backward compatible (textured and non-textured materials)

## Development Workflow

### Adding New Features

1. Check the roadmap (`claude/metagfx_roadmap.md`) to understand the planned architecture
2. Create an implementation plan for the next feature in the roadmap in the `claude`folder
3. Confirm the implementation approach before proceeding with implementation
4. Implement the feature by adding/editing source files
5. Update the corresponding CMakeLists.txt to include new source files
6. When complete, document in `docs`, either updating existing docs or adding new ones if needed

### Working with Shaders

- GLSL shaders (`.vert`, `.frag`) live in `src/app/`
- Version: `#version 450` (GLSL 4.5 for Vulkan)
- Compile shaders to `.spv` using `glslangValidator`
- Generate `.inl` headers using `python convert_spv.py` script
- Include compiled bytecode as `.spv.inl` C++ headers
- Shaders use uniform buffer objects (UBOs) for MVP matrices and material data

### Platform-Specific Code

Platform detection is done via CMake:
- `METAGFX_PLATFORM_WINDOWS`
- `METAGFX_PLATFORM_MACOS`
- `METAGFX_PLATFORM_LINUX`

Vulkan is currently the only implemented backend and requires the Vulkan SDK installed.

## Key Design Patterns

### Resource Creation Pattern

```cpp
// Factory function creates API-specific device
auto device = CreateGraphicsDevice(GraphicsAPI::Vulkan, windowHandle);

// Device creates all resources
auto buffer = device->CreateBuffer(bufferDesc);
auto shader = device->CreateShader(shaderDesc);
auto pipeline = device->CreateGraphicsPipeline(pipelineDesc);
```

### Command Recording Pattern

```cpp
auto cmd = device->CreateCommandBuffer();
cmd->Begin();
cmd->BeginRendering(colorTargets, depthTarget, clearValues);
cmd->BindPipeline(pipeline);
cmd->BindVertexBuffer(buffer);
cmd->SetViewport(viewport);
cmd->Draw(vertexCount);
cmd->EndRendering();
cmd->End();

device->SubmitCommandBuffer(cmd);
device->GetSwapChain()->Present();
```

### Descriptor Set Pattern (Vulkan-specific currently)

```cpp
// Create descriptor pool and layout
auto descriptorSet = CreateRef<VulkanDescriptorSet>(device, descriptorPool);

// Bind uniform buffers
descriptorSet->BindUniformBuffer(0, uniformBuffer, 0, sizeof(UBO));
descriptorSet->Update();

// Use in command buffer
commandBuffer->BindDescriptorSets(pipelineLayout, descriptorSet);
```

### Resource Management Pattern

MetaGFX uses a **deferred deletion queue** to safely manage GPU resource lifetimes:

```cpp
// In Application.h
struct PendingDeletion {
    std::unique_ptr<Model> model;
    uint32 frameCount;  // Frames to wait before deletion
};
std::vector<PendingDeletion> m_DeletionQueue;

// When switching models
if (m_Model) {
    // Queue old model for deletion after MAX_FRAMES_IN_FLIGHT frames
    m_DeletionQueue.push_back({std::move(m_Model), 2});
}

// Each frame, process the deletion queue
for (auto it = m_DeletionQueue.begin(); it != m_DeletionQueue.end(); ) {
    it->frameCount--;
    if (it->frameCount == 0) {
        it = m_DeletionQueue.erase(it);  // Safe to destroy now
    } else {
        ++it;
    }
}
```

This prevents crashes from destroying resources while they're still referenced by in-flight GPU command buffers. See `docs/resource_management.md` for detailed explanation.

## Common Issues

### Shader Compilation Errors

- Ensure `glslangValidator` is in PATH
- Verify shader version is `#version 450`
- Check binding points match C++ descriptor set layout

### Build Errors After Adding Files

- Update the appropriate `CMakeLists.txt` in `src/<module>/`
- Add headers to the `*_HEADERS` list
- Add source files to the `*_SOURCES` list
- Rebuild from scratch if CMake cache is stale: `rm -rf build && mkdir build`

## Documentation

Key documentation files:
- `README.md` - Project overview, setup instructions, build commands
- `docs/README.md` - Documentation index and organization
- `docs/rhi.md` - RHI architecture and design principles
- `docs/vulkan.md` - Vulkan backend implementation details
- `docs/metal.md` - Metal backend implementation with metal-cpp (NEW)
- `docs/camera_transformation_system.md` - Camera implementation details
- `docs/model_loading.md` - Model loading system design (Mesh, Model, Assimp integration)
- `docs/material_system.md` - Material system and Blinn-Phong lighting
- `docs/textures_and_samplers.md` - Texture loading, sampling, and material integration
- `docs/light_system.md` - Light system design and implementation
- `docs/pbr_rendering.md` - PBR rendering with Cook-Torrance BRDF
- `docs/shadow_mapping.md` - Shadow mapping with PCF, Vulkan depth convention, ground plane shadows
- `docs/resource_management.md` - GPU resource lifetimes and deferred deletion
- `docs/imgui_integration.md` - ImGui GUI system integration and usage
- `claude/metagfx_roadmap.md` - Full implementation roadmap (10 phases, 30+ milestones)
- `claude/milestone_x_y/` - Per-milestone implementation notes

## Code Style

- **Namespace**: All code is in `namespace metagfx { namespace rhi { ... } }`
- **Naming**:
  - Classes: `PascalCase` (e.g., `GraphicsDevice`, `VulkanBuffer`)
  - Methods: `PascalCase` (e.g., `CreateBuffer()`, `BeginRendering()`)
  - Members: `m_PascalCase` (e.g., `m_Device`, `m_SwapChain`)
  - Enums: `PascalCase` with scoped enums (e.g., `GraphicsAPI::Vulkan`)
- **Headers**: `#pragma once` for header guards
- **Includes**: Project headers use `"metagfx/..."`, external use `<...>`
- **C++ Standard**: C++20 features are allowed

## Backend Implementation Pattern

The Metal backend (Milestone 4.1) validates the RHI's multi-API design. When adding future backends (D3D12, WebGPU):

1. Create backend directory: `src/rhi/<api>/` and `include/metagfx/rhi/<api>/`
2. Implement all RHI abstract interfaces for the new backend
3. Update `src/rhi/GraphicsDevice.cpp` factory function to support new API
4. Add CMake option and conditional compilation in `src/rhi/CMakeLists.txt`
5. Handle shader compilation (SPIR-V → target shading language)
6. Test that the renderer core works without modification
7. Document backend-specific details in `docs/<api>.md`

**Key Insight**: Both Vulkan and Metal backends share 100% of application-level code. The RHI successfully abstracts fundamental API differences, proving the design works.
