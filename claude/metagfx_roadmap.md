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
┌─────────────────────────────────────────────────────────────────┐
│                     Application Layer                           │
├─────────────────────────────────────────────────────────────────┤
│                   Rendering Modes (Switchable)                  │
│   ┌─────────────┬──────────────────┬────────────────────────┐   │
│   │Rasterization│  Hybrid Renderer │  Path Tracing          │   │
│   │- Shadow Maps│  - Raster + RT   │  - Full Ray Tracing    │   │
│   │- Forward    │  - RT Shadows    │  - Global Illumination │   │
│   │- PBR + IBL  │  - RT Reflections│  - Unbiased            │   │
│   └─────────────┴──────────────────┴────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                Renderer Core (Shared)                           │
│        (Scene, Camera, Materials, Lights, BVH)                  │
├─────────────────────────────────────────────────────────────────┤
│            Graphics Abstraction Layer (RHI)                     │
│   - Rasterization Pipeline    - Ray Tracing Pipeline            │
│   - Compute Shaders           - Acceleration Structures         │
├─────────────┬───────────┬────────────┬──────────────────────────┤
│   Vulkan    │   D3D12   │    Metal   │   WebGPU                 │
└─────────────┴───────────┴────────────┴──────────────────────────┘
```

### Rendering Modes Philosophy

MetaGFX implements a **modular rendering architecture** with three distinct modes:

1. **Rasterization Mode** (Phase 3)
   - Traditional forward rendering with PBR shading
   - Shadow mapping for directional/point/spot lights
   - Image-based lighting (IBL)
   - Fast, real-time performance
   - Works on all hardware

2. **Hybrid Mode** (Phase 6)
   - Rasterization for primary visibility
   - Ray tracing for selective effects (shadows, reflections, AO)
   - Best quality-to-performance ratio
   - Requires RT-capable hardware (RTX, RDNA2+, Apple Silicon M3+)

3. **Path Tracing Mode** (Phase 7)
   - Full ray tracing for all light transport
   - Physically accurate global illumination
   - Progressive refinement for offline/high-quality renders
   - Supports interactive and production rendering

**Key Design Principles**:
- **Shared Scene Representation**: All modes use the same scene graph, materials, and lights
- **Runtime Switching**: User can toggle between modes without reloading assets
- **Graceful Degradation**: Hybrid mode falls back to rasterization if RT unavailable
- **Incremental Implementation**: Each mode builds on the previous foundation

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

### Milestone 3.3: Shadow Mapping (Rasterization Mode)
**Goal**: Cast shadows using traditional shadow mapping

- **Rendering Mode Foundation**: Establish modular renderer architecture
  - Abstract `Renderer` base class with mode-specific implementations
  - `RasterizationRenderer`: Forward rendering with shadow maps
  - Runtime mode switching infrastructure

- **Shadow Mapping Implementation**:
  - Shadow map framebuffer and depth texture
  - Shadow pass: render scene from light's perspective
  - PCF (Percentage Closer Filtering) for soft shadows
  - Shadow bias to prevent shadow acne
  - Support for directional lights (primary focus)
  - Point light shadows with cubemap depth textures (stretch goal)

- **Shader Integration**:
  - Light-space transformation matrices
  - Shadow coordinate calculation in vertex shader
  - PCF sampling in fragment shader
  - Configurable shadow map resolution

- **Quality Settings**:
  - Shadow map resolution presets (512, 1024, 2048, 4096)
  - PCF kernel size options (3x3, 5x5, 7x7)
  - Shadow distance/fade for performance

**Deliverable**: Scene with dynamic rasterized shadows from directional lights

**Note**: This implements the **Rasterization Mode** shadow system. Future milestones will add:
- **Milestone 6.3** (Hybrid Mode): Ray traced shadows for point/spot lights
- **Milestone 7.1** (Path Tracing Mode): Physically accurate soft shadows via path tracing

---

## PHASE 4: Multi-API and Optimizations

### Milestone 4.1: Metal Implementation
**Goal**: macOS/iOS support

- Implement RHI for Metal
- Platform-specific adaptations
- Verify that entire renderer core works unchanged
- API selection system at compile/runtime

**Deliverable**: Renderer working with Metal on macOS

---

### Milestone 4.2: WebGPU Implementation
**Goal**: Web and portability support

- Implement RHI for WebGPU (dawn or wgpu)
- API limitation considerations

**Deliverable**: Renderer working with WebGPU

---

### Milestone 4.3: Rendering Optimizations
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

## PHASE 6: Hardware Ray Tracing and Hybrid Rendering

### Milestone 6.1: BVH (Bounding Volume Hierarchy)
**Goal**: Ray query acceleration structures

- **CPU BVH Implementation**:
  - AABB (Axis-Aligned Bounding Box) primitive
  - BVH construction (SAH - Surface Area Heuristic)
  - BVH traversal on CPU
  - Basic ray-AABB and ray-triangle intersection tests

- **RHI Abstraction for Acceleration Structures**:
  - Abstract interfaces for BLAS (Bottom-Level AS) and TLAS (Top-Level AS)
  - Geometry descriptor structures
  - Instance descriptor structures
  - Build and update operations

**Deliverable**: Functional CPU BVH + Abstract AS interfaces in RHI

---

### Milestone 6.2: Hardware Ray Tracing Pipeline
**Goal**: Enable GPU-accelerated ray tracing

- **GPU Acceleration Structure Implementation**:
  - **Vulkan**: VK_KHR_acceleration_structure + VK_KHR_ray_query
  - **Metal**: Metal Ray Tracing acceleration structures
  - Automatic conversion from CPU BVH to hardware AS

- **Ray Tracing Pipeline**:
  - **Vulkan**: VK_KHR_ray_tracing_pipeline
  - Ray generation shaders
  - Closest hit shaders
  - Any-hit shaders (for transparency)
  - Miss shaders

- **RHI Ray Tracing Extensions**:
  - `CommandBuffer::BuildAccelerationStructure()`
  - `CommandBuffer::TraceRays()`
  - Shader binding table (SBT) management
  - RT pipeline state objects

**Deliverable**: Hardware-accelerated ray tracing pipeline working across Vulkan/Metal

**Note**: D3D12/DXR ray tracing support is implemented in Phase 8 (Milestone 8.2)

---

### Milestone 6.3: Hybrid Renderer - Ray Traced Shadows
**Goal**: Implement Hybrid Mode with selective ray tracing

- **Hybrid Renderer Implementation**:
  - `HybridRenderer` class extending base `Renderer`
  - G-buffer generation (rasterization pass)
  - Deferred lighting with RT shadows
  - Fallback to rasterization shadows if RT unavailable

- **Ray Traced Shadows**:
  - Shadow rays from fragment position to light
  - Soft shadows via area light sampling
  - Transparent shadow support (any-hit shaders)
  - Performance optimizations (ray culling, LOD)

- **Quality/Performance Modes**:
  - Shadow ray count: 1 (hard), 4 (soft), 16 (high quality)
  - Max shadow distance
  - Ray traced vs shadow mapped toggle per light type

- **Comparison System**:
  - Side-by-side rasterization vs hybrid rendering
  - Performance metrics (frame time, ray/s)
  - Quality comparison tools

**Deliverable**: Hybrid mode with RT shadows, runtime toggle between raster/hybrid modes

**Note**: Hybrid mode provides the best of both worlds:
- Rasterization for primary visibility (fast)
- Ray tracing for accurate shadows (quality)
- Graceful degradation on non-RT hardware

---

## PHASE 7: Path Tracing Mode

### Milestone 7.1: CPU Path Tracer
**Goal**: Physically accurate global illumination

- **Path Tracing Renderer Implementation**:
  - `PathTracingRenderer` class extending base `Renderer`
  - Unbiased Monte Carlo path tracing integrator
  - Support for both CPU and GPU execution

- **Core Path Tracing Algorithm**:
  - Ray generation from camera (pixel sampling)
  - Geometry intersection using BVH
  - BRDF sampling (diffuse, specular, transmission)
  - Russian roulette for path termination
  - Temporal sample accumulation
  - Progressive refinement (increasing sample count)

- **Light Transport**:
  - Direct lighting (shadow rays to lights)
  - Indirect lighting (recursive bounces)
  - Emissive materials as light sources
  - Maximum bounce depth control

- **Output and Display**:
  - Accumulation buffer (HDR)
  - Real-time preview with tone mapping
  - Save to HDR formats (EXR)

**Deliverable**: CPU path tracer with basic global illumination

**Note**: This implements the **Path Tracing Mode** - fully physically accurate rendering with:
- Soft shadows from area lights
- Color bleeding from indirect lighting
- Caustics (if using specular materials)
- Accurate ambient occlusion

---

### Milestone 7.2: Importance Sampling
**Goal**: Reduce noise and improve convergence

- **Advanced Sampling Techniques**:
  - Cosine-weighted hemisphere sampling (diffuse)
  - GGX importance sampling (specular BRDF)
  - Multiple Importance Sampling (MIS)
  - Next Event Estimation (explicit light sampling)

- **Variance Reduction**:
  - Stratified sampling
  - Low-discrepancy sequences (Halton, Sobol)
  - Adaptive sampling (more samples where needed)

- **Quality Improvements**:
  - Firefly reduction (clamp extreme values)
  - Denoising integration (Intel OIDN)
  - Configurable sample counts

**Deliverable**: Path tracing with significantly reduced noise at same sample count

---

### Milestone 7.3: GPU Path Tracing
**Goal**: Hardware-accelerated interactive path tracing

- **GPU Implementation**:
  - Port path tracer to compute shaders
  - Leverage hardware RT pipeline (ray gen, closest hit, miss shaders)
  - Coherent memory access patterns
  - Wavefront path tracing (separate ray types) - optional

- **Performance Optimizations**:
  - Tile-based rendering
  - Asynchronous accumulation
  - Dynamic sample count based on frame time
  - Hardware RT optimizations (ray culling, instance transforms)

- **Interactive Features**:
  - Real-time camera movement with accumulation reset
  - Progressive refinement while camera is static
  - Render region selection
  - Live material/light editing

- **Rendering Modes Integration**:
  - Seamless switching: Rasterization → Hybrid → Path Tracing
  - Shared scene representation across all modes
  - Mode-specific quality presets

**Deliverable**: Interactive GPU path tracer with real-time preview

**Performance Target**:
- 1920x1080 at 30+ FPS (1 sample/pixel, denoised)
- Progressive refinement to 1000+ samples for final renders

---

## PHASE 8: Direct3D 12 Backend

### Milestone 8.1: Direct3D 12 Implementation
**Goal**: Windows graphics backend

- Implement RHI for D3D12
- Verify that entire renderer core works unchanged
- D3D12 memory management (D3D12MA)
- Resource state tracking and barriers

**Deliverable**: Renderer working with D3D12 on Windows

---

### Milestone 8.2: DXR Ray Tracing Support
**Goal**: DirectX Raytracing integration

- DXR acceleration structures (BLAS/TLAS)
- Ray tracing pipeline state objects
- Shader binding tables
- Integration with hybrid and path tracing modes

**Deliverable**: Ray tracing support on D3D12/DXR

---

## PHASE 9: Advanced Techniques

### Milestone 9.1: Photon Mapping
**Goal**: Two-pass algorithm for caustics

- Photon emission from lights
- Photon tracing and storage
- Kd-tree structure for photon map
- Radiance estimation
- Caustics and color bleeding

**Deliverable**: Visible caustic effects

---

### Milestone 9.2: Bidirectional Path Tracing
**Goal**: Improve convergence in difficult scenes

- Light paths and eye paths
- Path connection
- MIS for combining strategies

**Deliverable**: Better quality in complex indirect lighting scenes

---

### Milestone 9.3: Volumetric Rendering
**Goal**: Participating media

- Scattering and absorption
- Transmittance
- Volume ray marching
- Fog, smoke, basic subsurface scattering

**Deliverable**: Volumetric effects

---

## PHASE 10: Production Features

### Milestone 10.1: Complete Asset System
**Goal**: Robust content pipeline

- Centralized asset manager
- Texture streaming
- Compression (BC7, ASTC, ETC2)
- Automatic mipmapping
- Asset cache

**Deliverable**: Efficient resource management

---

### Milestone 10.2: Denoising
**Goal**: Reduce noise in offline renders

- Integrate Intel Open Image Denoise or OptiX Denoiser
- Auxiliary G-buffer (normals, albedo)
- Improved temporal accumulation

**Deliverable**: Clean images with fewer samples

---

### Milestone 10.3: Offline Rendering System
**Goal**: High-quality rendering

- Progressive rendering
- Checkpoint saving
- Output to HDR formats (EXR)
- Tone mapping
- Basic post-processing

**Deliverable**: Offline rendering tool

---

### Milestone 10.4: Interactive Editor/Viewer
**Goal**: Interface for working with scenes

- UI with ImGui
- Scene manipulation
- Material editor
- Real-time preview
- Scene load/save

**Deliverable**: Complete application with GUI

---

## PHASE 11: Final Optimization and Polish

### Milestone 11.1: Profiling and Optimization
- GPU profiling (NSight, RenderDoc, PIX)
- CPU profiling
- Memory profiling
- Hot path optimization
- Parallelization (threading)

---

### Milestone 11.2: Testing and Validation
- Unit tests for critical components
- Visual regression tests
- Validation against reference renders (PBRT)
- Benchmark suite

---

### Milestone 11.3: Documentation
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
- Assimp: OBJ loading
- imgui: user interface components

### Per API
- **Vulkan**: Vulkan SDK, volk (loader)
- **D3D12**: Windows SDK, D3D12MA (memory allocator)
- **Metal**: Xcode, Metal SDK
- **WebGPU**: dawn or wgpu-native

### Optional
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
7. **Modular Rendering**: Design renderer abstraction to support mode switching without scene reload

---

## Rendering Mode Comparison

| Feature | Rasterization | Hybrid | Path Tracing |
|---------|---------------|--------|--------------|
| **Primary Visibility** | Rasterization | Rasterization | Ray Tracing |
| **Shadows** | Shadow Maps | RT Shadows | Path Traced |
| **Reflections** | Screen-space/IBL | RT Reflections (optional) | Path Traced |
| **Global Illumination** | IBL Approximation | IBL + RT AO (optional) | Full GI |
| **Transparency** | Alpha blending | Alpha + RT refraction | Path Traced |
| **Performance** | 60-144 FPS | 30-60 FPS | 1-30 FPS (progressive) |
| **Hardware Requirements** | Any GPU | RT-capable GPU | RT-capable GPU |
| **Use Cases** | Real-time games | High-quality real-time | Offline/production renders |
| **Implementation Phase** | Phase 3 | Phase 6 | Phase 7 |

### When to Use Each Mode

**Rasterization Mode**:
- Interactive scene exploration and editing
- Target platforms without RT hardware
- Games and real-time applications
- Maximum performance required

**Hybrid Mode**:
- High-quality real-time rendering on RT hardware
- Architectural visualization
- Product visualization with accurate shadows/reflections
- Best quality-to-performance balance

**Path Tracing Mode**:
- Offline rendering for film/animation
- Product marketing renders
- Photorealistic visualization
- Reference images for comparison
- When physical accuracy is required

### Shared Implementation Benefits

All three modes share:
- Scene graph and asset management
- Material system (PBR properties)
- Light definitions
- Camera system
- BVH acceleration structure (CPU for culling, GPU for RT)

This means:
- **No asset duplication**: Same models, textures, materials work in all modes
- **Easy comparison**: Switch modes with a single button press
- **Incremental development**: Each mode builds on previous foundations
- **Fallback support**: Graceful degradation when RT hardware unavailable

---

## Phase Summary

### Phase 1-2: Foundation (Milestones 1.1-2.3) ✅ COMPLETED
- Project setup, RHI abstraction, Vulkan backend
- Camera system, mesh loading, materials, textures
- **Current Status**: All milestones complete

### Phase 3: Rasterization Mode (Milestones 3.1-3.3) ✅ COMPLETED
- Light system (point, directional, spot)
- PBR rendering with Cook-Torrance BRDF
- **Shadow mapping** with PCF filtering
- **Deliverable**: Full rasterization renderer (60-144 FPS)

### Phase 4: Multi-API Support (Milestones 4.1-4.3)
- Metal, WebGPU backends
- Rendering optimizations (culling, LOD, instancing)
- Cross-platform validation

### Phase 5: PBRT Parser (Milestones 5.1-5.2)
- Scene file format support
- Advanced material and geometry types
- Production-ready asset pipeline

### Phase 6: Hybrid Mode (Milestones 6.1-6.3)
- CPU BVH and GPU acceleration structures
- Hardware ray tracing pipeline (Vulkan/Metal RT)
- **Ray traced shadows** in hybrid renderer
- **Deliverable**: Hybrid rasterization + RT (30-60 FPS)

### Phase 7: Path Tracing Mode (Milestones 7.1-7.3)
- CPU path tracer with global illumination
- Advanced sampling techniques (MIS, NEE)
- **GPU-accelerated path tracing**
- **Deliverable**: Full path tracer (1-30 FPS progressive)

### Phase 8: Direct3D 12 Backend (Milestones 8.1-8.2)
- D3D12 rasterization backend
- DXR ray tracing support
- Windows platform support

### Phase 9-11: Advanced Features
- Photon mapping, bidirectional path tracing, volumetric rendering
- Production pipeline (asset management, denoising, editor)
- Final optimization and documentation

**Current Focus**: Milestone 4.1 - Metal Implementation