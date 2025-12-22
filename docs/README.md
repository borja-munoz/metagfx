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

### Feature Documentation

#### [Camera and Transformation System](camera_transformation_system.md)
**Topics**: Camera implementation, MVP matrices

Overview of the camera system (Milestone 1.4):
- Camera class design
- Perspective and orthographic projection
- FPS-style controls (WASD + QE movement, mouse look)
- Uniform buffer integration
- MVP matrix computation
- Descriptor set binding

---

#### [Model Loading System](model_loading.md)
**Topics**: 3D model loading with Assimp

Comprehensive design documentation for model loading (Milestone 2.1):
- Mesh and Model class architecture
- Vertex structure (position, normal, texCoord)
- Assimp integration and supported formats (OBJ, FBX, glTF, COLLADA)
- GPU buffer management and memory strategies
- Procedural geometry generation (cube, sphere)
- Shader integration and rendering pipeline
- Error handling and fallback patterns
- Future enhancements (materials, textures, lighting)

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
2. [Vulkan Implementation](vulkan.md) - Backend specifics
3. [Modern Graphics APIs](modern_graphics_apis.md) - Underlying concepts

**Features**:
1. [Camera System](camera_transformation_system.md) - Viewport and controls
2. [Model Loading](model_loading.md) - Asset pipeline
3. [Roadmap](../claude/metagfx_roadmap.md) - Future features

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
**Current Milestone**: 2.1 (Model Loading with Assimp)
**Implemented Features**:
- ✅ Vulkan backend
- ✅ Camera system with FPS controls
- ✅ Model loading (OBJ, FBX, glTF, COLLADA)
- ✅ Procedural geometry (cube, sphere)
- ✅ Basic Phong lighting

**Next Milestones**:
- 2.2: Basic Material System
- 2.3: Textures and Samplers
- 3.1: Light System (point, directional, spot)

### Key Files
| File | Purpose |
|------|---------|
| `include/metagfx/rhi/*.h` | RHI abstract interfaces |
| `src/rhi/vulkan/*.cpp` | Vulkan backend implementation |
| `include/metagfx/scene/*.h` | Scene management (Camera, Mesh, Model) |
| `src/app/Application.cpp` | Main application and rendering loop |
| `src/app/*.vert`, `*.frag` | GLSL shaders |

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
- [ ] D3D12 implementation guide (when implemented)
- [ ] Metal implementation guide (when implemented)
- [ ] WebGPU implementation guide (when implemented)
- [ ] Material system design (Milestone 2.2)
- [ ] Texture system design (Milestone 2.3)
- [ ] Lighting system design (Phase 3)
- [ ] Performance optimization guide
- [ ] Debugging and profiling guide
- [ ] Testing strategy and framework

---