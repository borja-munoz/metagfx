# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

MetaGFX is a backend-agnostic physically-based renderer implementing a common abstract core with multiple graphics API backends (Vulkan, Direct3D 12, Metal, WebGPU). The project is organized around a phased roadmap with milestones tracked in `claude/metagfx_roadmap.md`.

**Current Status**: Milestone 2.1 completed (Model Loading with Assimp). The renderer can load 3D models from various file formats (OBJ, FBX, glTF, COLLADA) and render them with basic lighting.

## Build Commands

### Quick Setup (First Time)

**Linux/macOS:**
```bash
chmod +x setup.sh
./setup.sh
```

**Windows:**
```bash
setup.bat
```

### Building

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
cmake .. -DMETAGFX_USE_VULKAN=ON      # Enable Vulkan (default: ON)
cmake .. -DMETAGFX_USE_D3D12=ON       # Enable D3D12 (default: OFF)
cmake .. -DMETAGFX_USE_METAL=ON       # Enable Metal (default: OFF)
cmake .. -DMETAGFX_USE_WEBGPU=ON      # Enable WebGPU (default: OFF)
cmake .. -DMETAGFX_BUILD_TESTS=ON     # Build tests (default: OFF)
```

### Shader Compilation

Shaders are located in `src/app/` and must be compiled to SPIR-V bytecode included as C++ headers:

```bash
# Compile vertex shader
glslangValidator -V src/app/triangle.vert -o src/app/triangle.vert.spv
glslangValidator -V src/app/model.vert -o src/app/model.vert.spv

# Compile fragment shader
glslangValidator -V src/app/triangle.frag -o src/app/triangle.frag.spv
glslangValidator -V src/app/model.frag -o src/app/model.frag.spv

# Convert SPIR-V to C++ include file (manual step or custom script)
# The .spv files are then converted to .spv.inl files containing byte arrays
```

The compiled shaders (`.spv.inl` files) are included directly in the C++ source.

**Available Shaders**:
- `triangle.vert/frag` - Simple vertex color rendering
- `model.vert/frag` - Full vertex layout with normals and UVs, basic Phong lighting

## Architecture

### Modular Structure

The codebase follows a strict layered architecture:

```
┌─────────────────────────────────────────┐
│      Application Layer (src/app)        │  ← main.cpp, Application.cpp
├─────────────────────────────────────────┤
│   Renderer (src/renderer)               │  ← High-level rendering logic
├─────────────────────────────────────────┤
│   Scene (src/scene)                     │  ← Camera, Mesh, Model (future)
├─────────────────────────────────────────┤
│   RHI - Render Hardware Interface       │  ← API-agnostic abstractions
│        (src/rhi)                        │
├──────┬──────────────────────────────────┤
│Vulkan│  D3D12  │  Metal  │  WebGPU      │  ← Backend implementations
└──────┴──────────────────────────────────┘
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
- `Texture` - Images and sampling (future)
- `Shader` - Shader modules from SPIR-V bytecode
- `Pipeline` - Graphics pipeline state objects
- `Types.h` - Enums and structs (GraphicsAPI, BufferUsage, ShaderStage, Format, etc.)

**Backend Implementations** (`src/rhi/vulkan/`, `include/metagfx/rhi/vulkan/`):
- Currently only Vulkan is implemented (`VulkanDevice`, `VulkanSwapChain`, `VulkanBuffer`, etc.)
- Each backend implements the abstract RHI interfaces
- Backend selection happens via factory function: `CreateGraphicsDevice(GraphicsAPI api, ...)`

### Module Dependencies

- **core**: Base types (`Ref<T>`, `Scope<T>`, `uint32`, etc.), logging, platform abstraction
- **rhi**: Depends on core, provides graphics abstraction
- **scene**: Depends on core + rhi, provides Camera, Mesh, Model, Scene graph
- **renderer**: Depends on core + rhi + scene, high-level rendering
- **app**: Depends on all modules, application entry point

**External Dependencies**:
- **Assimp**: 3D model loading (OBJ, FBX, glTF, COLLADA importers enabled)
- **GLM**: Mathematics library for vectors, matrices
- **SDL3**: Window management and input
- **stb**: Image loading (future use)

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

## Development Workflow

### Adding New Features

1. Check the roadmap (`claude/metagfx_roadmap.md`) to understand the planned architecture
2. If adding RHI functionality, define the abstract interface first in `include/metagfx/rhi/`
3. Implement the Vulkan backend in `src/rhi/vulkan/`
4. Update the corresponding CMakeLists.txt to include new source files
5. When complete, document in `docs` if it's a milestone feature

### Working with Shaders

- GLSL shaders (`.vert`, `.frag`) live in `src/app/`
- Version: `#version 450` (GLSL 4.5 for Vulkan)
- Compile shaders to `.spv` using `glslangValidator`
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
- `docs/rhi.md` - RHI architecture and design principles
- `docs/camera_transformation_system.md` - Camera implementation details
- `docs/model_loading.md` - Model loading system design (Mesh, Model, Assimp integration)
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

## Future Backend Implementation

When adding new graphics API backends (D3D12, Metal, WebGPU):

1. Create backend directory: `src/rhi/<api>/` and `include/metagfx/rhi/<api>/`
2. Implement all RHI abstract interfaces for the new backend
3. Update `src/rhi/GraphicsDevice.cpp` factory function to support new API
4. Add CMake option and conditional compilation in `src/rhi/CMakeLists.txt`
5. Test that the renderer core works without modification
6. The RHI abstraction should hide all API-specific details from upper layers
