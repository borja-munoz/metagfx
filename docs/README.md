# MetaGFX Documentation

Welcome to the MetaGFX documentation. This directory contains comprehensive technical documentation covering the architecture, design decisions, and implementation details of the MetaGFX renderer.

## Documentation Index

### Architecture & Design

#### [Modern Graphics API Design](modern_graphics_apis.md)
**Topics**: Vulkan, Direct3D 12, Metal, WebGPU fundamentals

Comprehensive guide to modern low-level graphics APIs and their core concepts:
- Evolution from legacy APIs (OpenGL, D3D11) to explicit modern APIs
- Core concepts: devices, command buffers, pipelines, resources, shaders
- Shader languages (SPIR-V, HLSL, MSL, WGSL) and compilation
- Binding models: descriptor sets, root signatures
- Synchronization primitives: fences, semaphores, barriers
- API comparison and best practices
- Connection to MetaGFX RHI design

---

#### [RHI (Render Hardware Interface)](rhi.md)
**Topics**: MetaGFX graphics abstraction layer

Design principles and architecture of MetaGFX's graphics abstraction:
- API-agnostic design philosophy
- Core RHI abstractions (Device, Buffer, Shader, Pipeline, CommandBuffer, SwapChain)
- Type safety and modern C++ patterns
- Usage patterns and examples
- File structure and organization

---

#### [Vulkan Implementation](vulkan.md)
**Topics**: Vulkan backend specifics

Details of the Vulkan backend implementation:
- Vulkan-specific design decisions
- Memory management with VMA
- Descriptor set handling
- Synchronization strategies
- Platform-specific considerations

---

#### [Metal Implementation](metal.md)
**Topics**: Metal backend with metal-cpp

Comprehensive Metal backend implementation guide:
- metal-cpp integration and type mappings
- Pure C++ implementation (no Objective-C)
- Shader compilation (SPIR-V → MSL with SPIRV-Cross)
- Push constants emulation with `setBytes`
- Coordinate system differences (Y-flip handling)
- SDL3 integration and bridging
- ImGui Metal C++ backend integration
- Memory management and performance considerations
- Debugging with Xcode Metal Frame Debugger
- Full feature parity with Vulkan backend

---

#### [WebGPU Implementation](webgpu.md)
**Topics**: WebGPU backend with Google's Dawn

Complete WebGPU backend implementation guide:
- Google's Dawn C++ implementation (native + web)
- Shader compilation (SPIR-V → WGSL with SPIRV-Cross)
- Bind group resource binding model
- Push constants emulation via small uniform buffer
- Cubemap texture upload (per-face-per-mip with row alignment)
- Platform surface bridges (CAMetalLayer on macOS)
- Device initialization async/callback pattern
- Device-lost callback and shutdown handling
- Coordinate system (same as Vulkan, no Y-flip needed)
- ImGui WebGPU backend integration
- Full feature parity with Vulkan and Metal backends

---

### Feature Documentation

#### [Camera System](camera_transformation_system.md)
**Topics**: Camera, MVP matrices, auto-framing, frustum culling

Comprehensive camera documentation (Milestones 1.4, 4.3):
- Perspective / orthographic projection and Y-axis flip per backend
- Orbital camera controls (drag, zoom, pan)
- FPS-style movement (WASD + QE, mouse look)
- Automatic model framing (`FrameBoundingBox`) with margin control
- Uniform buffer integration (double-buffered)
- Frustum extraction (Gribb–Hartmann) via `Frustum::FromViewProjection()`
- Per-model and per-instance sphere culling

---

#### [Asset Loading System](asset_loading.md)
**Topics**: 3D model loading, PBRT v4 scenes, texture loading, LOD, instanced rendering

Comprehensive asset pipeline documentation (Milestones 2.1, 2.3, 4.3, 5.1):
- Texture loading pipeline (stb_image → CPU → GPU) with fallback textures
- Assimp model loading: supported formats (OBJ, FBX, glTF, COLLADA), post-processing, material extraction
- **PBRT v4 scene loading** — lexer, parser, loader; trianglemesh + plymesh shapes; all material types; conductor spectral data → RGB F0; lights and camera extraction
- Mesh and Model class architecture; vertex layout; per-mesh material buffers and texture flags
- **Level of Detail (LOD)** — meshoptimizer, 3 levels, distance-based selection
- **Instanced Rendering** — per-instance mat4, vertex buffer slot 1, N×N grid, per-frame CPU culling
- Bounding volume cache (AABB + sphere) for culling and framing
- Error handling, fallback patterns, and debugging tips

---

#### [Material System](material_system.md)
**Topics**: Material properties and Blinn-Phong lighting

Complete material system design and implementation (Milestone 2.2):
- Material class with albedo, roughness, metallic properties
- GPU-compatible std140 layout and descriptor set integration
- Blinn-Phong lighting model (ambient, diffuse, specular)
- Roughness-based shininess mapping for intuitive control
- Assimp material extraction and conversion
- Per-mesh material ownership and rendering
- Push constants for camera position
- Critical bug fixes (vector pointer invalidation, backface culling)
- Performance analysis and future PBR roadmap

---

#### [Textures and Samplers](textures_and_samplers.md)
**Topics**: Texture loading, sampling, and material integration

Complete texture system design and implementation (Milestone 2.3):
- Sampler abstraction with shared global sampler strategy
- VulkanTexture implementation with device-local memory and staging buffers
- TextureUtils for image loading with stb_image (PNG, JPEG, TGA, BMP)
- Material extension with optional albedo texture support
- Assimp texture extraction and path resolution
- VulkanDescriptorSet extension for combined image samplers
- Shader integration with conditional texture sampling
- UV checker textures for debugging texture mapping
- Push constants for material texture flags
- Backward compatibility with non-textured materials

---

#### [Light System](light_system.md)
**Topics**: Dynamic lighting with multiple light types

Complete light system design and implementation (Milestone 3.1):
- Light class hierarchy (Light base, DirectionalLight, PointLight, SpotLight)
- Scene integration with up to 16 lights
- GPU-compatible std140 layout (1040-byte light buffer)
- Forward rendering with Blinn-Phong shading
- Distance attenuation for point and spot lights
- Spot light cone angles with smooth falloff
- Descriptor binding 3 for light buffer
- Per-frame light buffer updates
- Test scene with 4 lights (2 directional, 1 point, 1 spot)

---

#### [Resource Management](resource_management.md)
**Topics**: GPU resource lifetimes and deferred deletion

Comprehensive guide to safe resource management:
- The problem: in-flight frames and resource destruction
- Deferred deletion queue implementation
- Double-buffering and `MAX_FRAMES_IN_FLIGHT`
- Vulkan synchronization model (fences, semaphores)
- Alternative approaches and why they fail
- Best practices for resource lifetime management
- Future enhancements (generalized deletion, ring buffers)

---

#### [Performance Metrics](performance_metrics.md)
**Topics**: Frame time, draw calls, triangle count, frustum culling counter

Built-in real-time performance counters displayed in the ImGui panel (Milestone 4.3):
- Frame time (ms) and FPS — 500 ms smoothed average to avoid flickering
- Draw call counter — all passes: shadow, model, ground plane, skybox
- Triangle counter — accounts for active LOD level and visible instance count
- Culled meshes counter — objects/instances rejected by CPU frustum culling
- Implementation notes on safe per-frame counter reset and buffer update ordering

---

#### [ImGui Integration](imgui_integration.md)
**Topics**: Immediate-mode GUI with Dear ImGui

Complete ImGui integration guide:
- Architecture and build integration
- Initialization (descriptor pool, render pass, backends)
- Framebuffer management (lazy creation, resize handling)
- Per-frame rendering pipeline
- Event handling and input capture
- Common UI patterns and widget usage
- Integration with shader parameters (push constants)
- Performance considerations and troubleshooting
- Future enhancements (docking, custom themes, profiler)

---

## Project Documentation

### Root Level Documents

#### [README.md](../README.md)
Main project documentation covering:
- Project overview and goals
- Prerequisites and dependencies
- Setup instructions (quick and manual)
- Build commands for all platforms
- Running the application
- CMake build options
- Controls and usage
- License information

---

#### [CLAUDE.md](../CLAUDE.md)
AI coding assistant guidance covering:
- Project status and current milestone
- Build commands and workflow
- Architecture overview (RHI, scene system, modules)
- Development patterns and best practices
- Common issues and troubleshooting
- Code style conventions
- Documentation references

---

### Implementation Notes

#### [claude/metagfx_roadmap.md](../claude/metagfx_roadmap.md)
Complete implementation roadmap:
- 10 phases covering the entire project lifecycle
- 30+ milestones from basic setup to production features
- Phase 1: Fundamentals (setup, RHI, basic rendering, camera)
- Phase 2: Geometry and assets (models, materials, textures)
- Phase 3: Lighting and PBR
- Phase 4: Multi-API support and optimizations
- Phase 5-10: PBRT parsing, ray tracing, path tracing, production features

---

#### [claude/milestone_X_Y/](../claude/)
Per-milestone implementation notes and artifacts:
- Detailed implementation instructions
- Code samples and explanations
- Design decisions and rationale
- Generated during development with Claude

**Audience**: Historical context, understanding implementation evolution.

---

## Documentation Organization

### By Topic

**Getting Started**:
1. [README.md](../README.md) - Project overview and setup
2. [Modern Graphics APIs](modern_graphics_apis.md) - Learn graphics concepts
3. [RHI Design](rhi.md) - Understand the abstraction

**Architecture**:
1. [RHI Design](rhi.md) - Graphics abstraction layer
2. [Vulkan Implementation](vulkan.md) - Vulkan backend specifics
3. [Metal Implementation](metal.md) - Metal backend specifics
4. [WebGPU Implementation](webgpu.md) - WebGPU backend specifics
5. [Modern Graphics APIs](modern_graphics_apis.md) - Underlying concepts

**Features**:
1. [Camera System](camera_transformation_system.md) - Viewport, controls, framing, frustum culling
2. [Asset Loading](asset_loading.md) - Asset pipeline (models, PBRT, textures), LOD, instancing
3. [Material System](material_system.md) - Materials and lighting
4. [Textures and Samplers](textures_and_samplers.md) - Texture system
5. [Light System](light_system.md) - Dynamic lighting
6. [Shadow Mapping](shadow_mapping.md) - PCF shadow maps
7. [PBR Rendering](pbr_rendering.md) - Cook-Torrance BRDF
8. [IBL System](ibl_system.md) - Image-Based Lighting
9. [Skybox System](skybox_system.md) - Cubemap skybox
10. [Performance Metrics](performance_metrics.md) - Frame time, draw calls, culling counters
11. [Resource Management](resource_management.md) - GPU resource lifetimes
12. [ImGui Integration](imgui_integration.md) - GUI system
13. [Roadmap](../claude/metagfx_roadmap.md) - Future features

**Development**:
1. [CLAUDE.md](../CLAUDE.md) - Development guide
2. [Milestone Notes](../claude/) - Implementation history
3. [RHI Design](rhi.md) - API reference

---

## Document Maintenance

### Adding New Documentation

When adding new features or systems:

1. **Create Feature Documentation**: Write a dedicated `.md` file in `docs/`
2. **Update This Index**: Add entry with description and metadata
3. **Update CLAUDE.md**: Add reference to new documentation
4. **Cross-Reference**: Link related documents

### Documentation Standards

- **Structure**: Use clear headings and sections
- **Code Examples**: Include practical code samples
- **Visuals**: Use ASCII diagrams where helpful
- **Updates**: Keep documentation in sync with code

---

## Quick Reference

### Build Commands
```bash
# Setup (first time)
./setup.sh  # Linux/macOS
setup.bat   # Windows

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)  # Linux
make -j$(sysctl -n hw.ncpu)  # macOS

# Run
cd bin
./metagfx
```

### Project Status
**Current Milestone**: 5.1 (Basic PBRT Parser) ✅ Complete
**Implemented Features**:
- ✅ **Vulkan backend** (Windows, Linux, macOS)
- ✅ **Metal backend** (macOS, iOS-ready)
- ✅ **WebGPU backend** (Windows, macOS, Linux, Web)
- ✅ Camera system with FPS controls, orbital mode, and automatic model framing
- ✅ **Frustum culling** — per-model and per-instance CPU sphere tests - **NEW**
- ✅ Model loading (OBJ, FBX, glTF, COLLADA)
- ✅ Runtime model switching with deferred deletion
- ✅ Procedural geometry (cube, sphere, ground plane)
- ✅ **Level of Detail (LOD)** — 3 levels via meshoptimizer, distance-based selection - **NEW**
- ✅ **Instanced rendering** — N×N grid, per-instance mat4, single draw call - **NEW**
- ✅ **Performance metrics** — frame time, draw calls, triangles, culled count - **NEW**
- ✅ Material system (albedo, roughness, metallic, AO, emissive)
- ✅ PBR rendering with Cook-Torrance BRDF
- ✅ Image-Based Lighting (IBL) with environment maps
- ✅ Normal mapping with derivative-based TBN
- ✅ ACES filmic tone mapping and exposure control
- ✅ Texture system (albedo, normal, metallic-roughness, AO, emissive)
- ✅ Light system (directional, point, spot lights - up to 16 lights)
- ✅ Shadow mapping with PCF filtering
- ✅ Skybox rendering with LOD control (cubemap, all 6 faces)
- ✅ ImGui integration (Vulkan + Metal + WebGPU backends)

**Next Milestones**:
- 5.2: Advanced PBRT Features (instancing, infinite lights, procedural textures)
- 8.1: Direct3D 12 Implementation (Windows) - Postponed to Phase 8

### Key Files
| File | Purpose |
|------|---------|
| `include/metagfx/rhi/*.h` | RHI abstract interfaces |
| `src/rhi/vulkan/*.cpp` | Vulkan backend implementation |
| `src/rhi/metal/*.cpp` | Metal backend implementation |
| `src/rhi/webgpu/*.cpp` | WebGPU backend implementation |
| `include/metagfx/scene/*.h` | Scene management (Camera, Mesh, Model) |
| `src/app/Application.cpp` | Main application and rendering loop |
| `src/app/*.vert`, `*.frag` | GLSL shaders (compiled to SPIR-V) |

### External Dependencies
- **SDL3**: Window management and input
- **Vulkan SDK**: Graphics API
- **Assimp**: 3D model loading
- **GLM**: Mathematics library
- **stb**: Image loading (future)

---

## Contributing to Documentation

### Guidelines

1. **Clarity**: Write for your target audience
2. **Examples**: Include practical code examples
3. **Completeness**: Cover design rationale, not just implementation
4. **Structure**: Use consistent heading hierarchy
5. **Links**: Cross-reference related documents
6. **Updates**: Keep docs synchronized with code changes

### Documentation TODO

Future documentation needs:
- [x] Metal implementation guide ✅ Complete (Milestone 4.1)
- [x] WebGPU implementation guide ✅ Complete (Milestone 4.2)
- [x] Frustum culling + auto-framing in camera doc ✅ Complete (Milestone 4.3)
- [x] LOD + instancing in model loading doc ✅ Complete (Milestone 4.3)
- [x] Performance metrics guide ✅ Complete (Milestone 4.3)
- [ ] D3D12 implementation guide (Milestone 8.1 - postponed to Phase 8)
- [x] Texture system design ✅ Complete (Milestone 2.3)
- [x] Lighting system design ✅ Complete (Milestone 3.1)
- [x] PBR rendering guide ✅ Complete (Milestone 3.2)
- [x] Shadow mapping guide ✅ Complete (Milestone 3.3)
- [x] PBRT scene parser guide ✅ Complete (Milestone 5.1, merged into asset_loading.md)
- [ ] Testing strategy and framework

---