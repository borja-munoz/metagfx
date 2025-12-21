# Implementation Plan: MetaGFX - A backend-agnostic physically-based renderer

## Core Technology Stack

- **Language**: C++20
- **Build System**: CMake 3.20+
- **Window Management**: SDL3 (supports Vulkan, D3D12, Metal)
- **Mathematics**: GLM or custom library
- **Model Loading**: Assimp, tinyobjloader
- **PBRT Parsing**: Custom implementation

## General Architecture

```
┌─────────────────────────────────────────┐
│         Application Layer               │
├─────────────────────────────────────────┤
│         Renderer Core                   │
│  (Scene, Camera, Materials, Lights)     │
├─────────────────────────────────────────┤
│      Graphics Abstraction Layer         │
│   (RHI - Render Hardware Interface)     │
├──────┬──────┬──────┬────────────────────┤
│Vulkan│D3D12 │Metal │   WebGPU           │
└──────┴──────┴──────┴────────────────────┘
```

---

## PHASE 1: Fundamentals and Base Architecture

### Milestone 1.1: Project Setup
**Goal**: Working basic project structure

- Configure CMake with modular organization
- Integrate SDL3 for window management
- Set up platform-specific build options (Windows/Linux/macOS)
- Directory structure:
  ```
  /src
    /core          - Base classes, math
    /rhi           - Graphics API abstraction
    /scene         - Scene management
    /renderer      - Rendering logic
  /include
  /external        - Third-party dependencies
  /assets          - Models, textures, scenes
  ```
- Basic logging system

**Deliverable**: Project that compiles and opens an empty window

---

### Milestone 1.2: Graphics API Abstraction (RHI) - Basic Concepts
**Goal**: Abstraction layer for basic commands

Define abstract interfaces for:
- `GraphicsDevice`: graphics device initialization
- `SwapChain`: presentation chain
- `CommandBuffer`: command recording
- `Buffer`: GPU buffers (vertex, index, uniform)
- `Pipeline`: graphics pipeline states
- `Shader`: shader compilation and management

**Deliverable**: Documented C++ abstract interfaces

---

### Milestone 1.3: Vulkan Implementation - "Hello Triangle"
**Goal**: First functional renderer with Vulkan

- Implement RHI for Vulkan
- Create instance, device, swap chain
- Basic graphics pipeline (vertex + fragment shader)
- Render a triangle with vertex colors
- Basic render loop

**Deliverable**: Color triangle rendered with Vulkan

---

### Milestone 1.4: Camera and Transformation System
**Goal**: Viewport and projection control

- Camera class (perspective and orthographic)
- View and projection matrices
- Camera controls (orbit, first person)
- Uniform buffers for MVP matrices

**Deliverable**: Triangle that can be viewed from different angles

---

## PHASE 2: Geometry and Basic Assets

### Milestone 2.1: Simple Mesh Loading
**Goal**: Render geometry from files

- Integrate tinyobjloader or Assimp
- Load basic OBJ/glTF files
- Vertex buffer and index buffer management
- Render cubes, spheres, simple models

**Deliverable**: Basic 3D models loaded from files

---

### Milestone 2.2: Basic Material System
**Goal**: Surface visual properties

- Material class with basic properties (color, roughness, metallic)
- Shader with simple lighting model (Phong/Blinn-Phong)
- Uniform buffers for material properties

**Deliverable**: Objects with different materials

---

### Milestone 2.3: Textures and Samplers
**Goal**: Texture mapping

- Image loading (stb_image)
- Texture abstraction in RHI
- UV coordinates
- Texture samplers (filtering, wrapping)
- Albedo maps in materials

**Deliverable**: Models with applied textures

---

## PHASE 3: Lighting and PBR Shading

### Milestone 3.1: Light System
**Goal**: Different types of light sources

- Light base class
- Point lights
- Directional lights
- Spot lights
- Light buffer on GPU
- Forward rendering with multiple lights

**Deliverable**: Scene with various light types

---

### Milestone 3.2: PBR - Physically Based Rendering
**Goal**: Realistic shading model

- Implement Cook-Torrance BRDF
- Roughness and metallic maps
- Normal mapping
- Basic Image-Based Lighting (IBL)
- Environment with skybox/environment map

**Deliverable**: Realistic PBR materials

---

### Milestone 3.3: Shadow Mapping
**Goal**: Cast shadows

- Shadow maps for directional lights
- PCF (Percentage Closer Filtering) technique
- Cascade shadow maps (optional for this milestone)

**Deliverable**: Scene with dynamic shadows

---

## PHASE 4: Multi-API and Optimizations

### Milestone 4.1: Direct3D 12 Implementation
**Goal**: Second graphics backend

- Implement RHI for D3D12
- Verify that entire renderer core works unchanged
- API selection system at compile/runtime

**Deliverable**: Renderer working with D3D12 on Windows

---

### Milestone 4.2: Metal Implementation
**Goal**: macOS/iOS support

- Implement RHI for Metal
- Platform-specific adaptations

**Deliverable**: Renderer working with Metal on macOS

---

### Milestone 4.3: WebGPU Implementation
**Goal**: Web and portability support

- Implement RHI for WebGPU (dawn or wgpu)
- API limitation considerations

**Deliverable**: Renderer working with WebGPU

---

### Milestone 4.4: Rendering Optimizations
**Goal**: Improve performance

- Frustum culling
- Backface culling
- Basic Level of Detail (LOD)
- Instanced rendering
- Profiling and performance metrics

**Deliverable**: Complex scenes at stable framerates

---

## PHASE 5: PBRT Scene Parser

### Milestone 5.1: Basic PBRT Parser
**Goal**: Read PBRT v3/v4 scene files

- Lexer and parser for PBRT syntax
- Load geometry (shapes)
- Load basic materials
- Load transformations
- Load camera and film

**Deliverable**: Simple PBRT scenes rendered

---

### Milestone 5.2: Advanced PBRT Features
**Goal**: Complete format support

- All shape types
- All materials (glass, metal, plastic, etc.)
- Textures and maps
- Instancing
- Area lights

**Deliverable**: Complex PBRT scenes working

---

## PHASE 6: Acceleration Structures

### Milestone 6.1: BVH (Bounding Volume Hierarchy)
**Goal**: Ray query acceleration

- Implement AABB (Axis-Aligned Bounding Box)
- BVH construction (SAH - Surface Area Heuristic)
- BVH traversal on CPU
- Tests with basic ray casting

**Deliverable**: Functional BVH system

---

### Milestone 6.2: Hardware Acceleration - Ray Tracing Pipeline
**Goal**: Use native RT extensions

- **Vulkan**: VK_KHR_ray_tracing_pipeline
- **D3D12**: DXR (DirectX Raytracing)
- **Metal**: Metal Ray Tracing
- Abstract acceleration structures in RHI
- Bottom-level AS (geometry)
- Top-level AS (instances)

**Deliverable**: GPU BVH with hardware ray tracing

---

## PHASE 7: Path Tracing

### Milestone 7.1: Basic Path Tracer
**Goal**: Global illumination with Monte Carlo

- Unbiased path tracing integrator
- Ray generation from camera
- Geometry intersection (using BVH)
- BRDF sampling
- Russian roulette for termination
- Temporal sample accumulation

**Deliverable**: First images with basic GI

---

### Milestone 7.2: Importance Sampling
**Goal**: Reduce noise in path tracing

- Cosine-weighted hemisphere sampling
- BRDF importance sampling
- Multiple Importance Sampling (MIS)
- Next Event Estimation (direct light sampling)

**Deliverable**: Path tracing with less noise

---

### Milestone 7.3: GPU Path Tracing
**Goal**: Accelerate with compute shaders / ray tracing pipeline

- Implement path tracer in shaders
- Wavefront path tracing (optional)
- Coherent memory handling
- Accumulation and basic denoising

**Deliverable**: Interactive real-time path tracing

---

## PHASE 8: Advanced Techniques

### Milestone 8.1: Photon Mapping
**Goal**: Two-pass algorithm for caustics

- Photon emission from lights
- Photon tracing and storage
- Kd-tree structure for photon map
- Radiance estimation
- Caustics and color bleeding

**Deliverable**: Visible caustic effects

---

### Milestone 8.2: Bidirectional Path Tracing
**Goal**: Improve convergence in difficult scenes

- Light paths and eye paths
- Path connection
- MIS for combining strategies

**Deliverable**: Better quality in complex indirect lighting scenes

---

### Milestone 8.3: Volumetric Rendering
**Goal**: Participating media

- Scattering and absorption
- Transmittance
- Volume ray marching
- Fog, smoke, basic subsurface scattering

**Deliverable**: Volumetric effects

---

## PHASE 9: Production Features

### Milestone 9.1: Complete Asset System
**Goal**: Robust content pipeline

- Centralized asset manager
- Texture streaming
- Compression (BC7, ASTC, ETC2)
- Automatic mipmapping
- Asset cache

**Deliverable**: Efficient resource management

---

### Milestone 9.2: Denoising
**Goal**: Reduce noise in offline renders

- Integrate Intel Open Image Denoise or OptiX Denoiser
- Auxiliary G-buffer (normals, albedo)
- Improved temporal accumulation

**Deliverable**: Clean images with fewer samples

---

### Milestone 9.3: Offline Rendering System
**Goal**: High-quality rendering

- Progressive rendering
- Checkpoint saving
- Output to HDR formats (EXR)
- Tone mapping
- Basic post-processing

**Deliverable**: Offline rendering tool

---

### Milestone 9.4: Interactive Editor/Viewer
**Goal**: Interface for working with scenes

- UI with ImGui
- Scene manipulation
- Material editor
- Real-time preview
- Scene load/save

**Deliverable**: Complete application with GUI

---

## PHASE 10: Final Optimization and Polish

### Milestone 10.1: Profiling and Optimization
- GPU profiling (NSight, RenderDoc, PIX)
- CPU profiling
- Memory profiling
- Hot path optimization
- Parallelization (threading)

---

### Milestone 10.2: Testing and Validation
- Unit tests for critical components
- Visual regression tests
- Validation against reference renders (PBRT)
- Benchmark suite

---

### Milestone 10.3: Documentation
- Architecture documentation
- API reference
- User guide
- Examples and tutorials

---

## Dependencies

### Required
- SDL3: window and input management
- GLM: mathematics
- stb_image: image loading
- tinyobjloader: OBJ loading

### Per API
- **Vulkan**: Vulkan SDK, volk (loader)
- **D3D12**: Windows SDK, D3D12MA (memory allocator)
- **Metal**: Xcode, Metal SDK
- **WebGPU**: dawn or wgpu-native

### Optional
- Assimp: multiple 3D format loading
- ImGui: user interface
- Catch2: testing
- Intel OIDN: denoising

---

## Important Considerations

1. **Multi-API Debugging**: Implement exhaustive validation in RHI layer
2. **Shaders**: Use SPIRV-Cross for transpilation or maintain native versions
3. **Continuous Testing**: Validate each new feature across all APIs
4. **GPU Synchronization**: Different models between APIs (fences, semaphores)
5. **Memory**: API-specific memory managers (VMA, D3D12MA)
6. **Performance**: Don't optimize prematurely, functionality first