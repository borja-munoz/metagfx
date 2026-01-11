# Milestone 3.3: Shadow Mapping - Implementation Summary

**Status**: ✅ Completed
**Date**: January 2026

## Overview

Milestone 3.3 adds real-time shadow mapping to the renderer, implementing depth-based shadows from directional lights with Percentage Closer Filtering (PCF) for soft shadow edges.

## Key Features Implemented

### Shadow Mapping Core
- ✅ Two-pass shadow rendering (shadow pass + main pass)
- ✅ 2048×2048 shadow map resolution (configurable)
- ✅ Depth-only framebuffer for shadow pass
- ✅ Vulkan-specific depth convention ([0,1] NDC range)
- ✅ Comparison sampler with hardware-accelerated depth testing

### Shadow Filtering
- ✅ Percentage Closer Filtering (PCF) with 3×3 kernel
- ✅ Soft shadow edges (9-sample averaging)
- ✅ Configurable shadow bias (prevents shadow acne)

### Scene Integration
- ✅ Ground plane for shadow visualization
- ✅ Separate descriptor sets for model and ground plane
- ✅ Dynamic ground plane positioning based on model bounds
- ✅ Ground plane receives shadows but doesn't cast them

### Interactive Controls
- ✅ Shadow enable/disable toggle
- ✅ Shadow bias slider (0.0 to 0.01)
- ✅ Light direction controls (X, Y, Z sliders)
- ✅ Ground plane visibility toggle
- ✅ Multiple shadow debug visualization modes

## Technical Achievements

### Vulkan Depth Convention
- Manual construction of orthographic projection matrix for Vulkan
- Correct [0,1] depth mapping (GLM's `glm::ortho()` uses OpenGL's [-1,1])
- Proper depth coordinate transformation in fragment shader

### Resource Management
- Dedicated material buffer for ground plane
- Separate descriptor sets to prevent resource conflicts
- Proper cleanup order to avoid Vulkan validation errors
- Frame-synchronized descriptor set usage

### Critical Bug Fixes
1. **Emissive texture conflict**: Ground plane material buffer was overwriting model materials
2. **Descriptor binding indexing**: Array index corresponds to binding number (not off by one)
3. **Ground plane in shadow pass**: Removed to prevent shadow interference
4. **Shutdown validation error**: Added proper cleanup of ground plane resources

## Files Added

### Headers
- `include/metagfx/rhi/Framebuffer.h` - Framebuffer abstraction
- `include/metagfx/rhi/vulkan/VulkanFramebuffer.h` - Vulkan framebuffer implementation
- `include/metagfx/scene/ShadowMap.h` - Shadow map management

### Implementation
- `src/rhi/vulkan/VulkanFramebuffer.cpp` - Vulkan framebuffer
- `src/scene/ShadowMap.cpp` - Shadow map and light-space matrix

### Shaders
- `src/app/shadowmap.vert` - Shadow pass vertex shader
- `src/app/shadowmap.frag` - Shadow pass fragment shader (empty)
- `src/app/shadowmap.vert.spv` - Compiled SPIR-V bytecode
- `src/app/shadowmap.frag.spv` - Compiled SPIR-V bytecode
- `src/app/shadowmap.vert.spv.inl` - C++ include file
- `src/app/shadowmap.frag.spv.inl` - C++ include file

### Documentation
- `docs/shadow_mapping.md` - Comprehensive shadow mapping documentation

## Files Modified

### Core Application
- `src/app/Application.h` - Added shadow map, ground plane, light direction members
- `src/app/Application.cpp` - Shadow pass, ground plane rendering, UI controls

### Shaders
- `src/app/model.frag` - Added `calculateShadow()` function, PCF implementation

### RHI Extensions
- `include/metagfx/rhi/Types.h` - Added `TextureUsage::Sampled` flag
- `include/metagfx/rhi/GraphicsDevice.h` - Added `CreateFramebuffer()` method
- `include/metagfx/rhi/vulkan/VulkanDevice.h` - Framebuffer creation
- `src/rhi/vulkan/VulkanDevice.cpp` - Implemented framebuffer creation
- `src/rhi/vulkan/VulkanCommandBuffer.cpp` - Framebuffer rendering support
- `src/rhi/vulkan/VulkanSampler.cpp` - Comparison sampler support

### Build System
- `src/renderer/CMakeLists.txt` - Added RasterizationRenderer stub
- `src/rhi/CMakeLists.txt` - Added VulkanFramebuffer sources
- `src/scene/CMakeLists.txt` - Added ShadowMap sources

### Documentation
- `README.md` - Updated features list
- `CLAUDE.md` - Updated project status and documentation list
- `docs/rhi.md` - Added Framebuffer and Sampler abstractions

## Descriptor Bindings

Shadow mapping added two new descriptor bindings:

**Binding 12**: Shadow map sampler (comparison sampler)
- Type: `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`
- Texture: Shadow depth texture (D32_SFLOAT)
- Sampler: Comparison sampler with `LessOrEqual` compare op

**Binding 13**: Shadow uniform buffer
- Type: `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`
- Data: Light-space matrix, model matrix, shadow bias

## Key Implementation Details

### Shadow Pass
```cpp
// 1. Update shadow map light matrix
m_ShadowMap->UpdateLightMatrix(lightDir, camera);

// 2. Render to shadow framebuffer
cmd->BeginRendering({}, m_ShadowMap->GetDepthTexture(), clearValues);

// 3. Render ONLY shadow casters (not ground plane)
for (const auto& mesh : m_Model->GetMeshes()) {
    cmd->DrawIndexed(mesh->GetIndexCount());
}

cmd->EndRendering();
```

### Main Pass Shadow Sampling
```glsl
// Transform fragment to light space
vec4 fragPosLightSpace = shadow.lightSpaceMatrix * vec4(fragPos, 1.0);
vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

// Transform X/Y to [0,1], Z is already [0,1] for Vulkan
projCoords.xy = projCoords.xy * 0.5 + 0.5;

// PCF with 3x3 kernel
float shadowFactor = 0.0;
for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
        shadowFactor += texture(shadowMapSampler, projCoords.xy + offset).r;
    }
}
shadowFactor /= 9.0;
```

### Ground Plane Setup
```cpp
// Create dedicated material buffer
m_GroundPlaneMaterialBuffer = m_Device->CreateBuffer(materialBufferDesc);

// Create separate descriptor set (same layout as model)
std::vector<rhi::DescriptorBinding> groundPlaneBindings = bindings;
groundPlaneBindings[1].buffer = m_GroundPlaneMaterialBuffer;  // Index 1 = binding 1
groundPlaneBindings[2].texture = m_DefaultWhiteTexture;        // Index 2 = binding 2
// ... set default textures ...

m_GroundPlaneDescriptorSet = std::make_unique<rhi::VulkanDescriptorSet>(
    context, groundPlaneBindings
);
```

## Lessons Learned

### Descriptor Set Indexing
**Problem**: Array index in bindings vector corresponds to binding number, not sequential position.

**Solution**:
```cpp
groundPlaneBindings[1].buffer = ...  // Binding 1 (Material UBO)
groundPlaneBindings[2].texture = ... // Binding 2 (Albedo texture)
```

### Resource Sharing Between Descriptor Sets
**Problem**: Copying bindings creates shallow copies - shared buffer pointers cause conflicts.

**Solution**: Create dedicated buffers for resources that need different values per descriptor set.

### Shadow Receivers vs Casters
**Problem**: Rendering ground plane in shadow pass caused shadow interference.

**Solution**: Only render shadow casters in shadow pass, not receivers.

### Vulkan Depth Convention
**Problem**: GLM's `glm::ortho()` uses OpenGL convention, incompatible with Vulkan.

**Solution**: Manually construct orthographic matrix with correct [0,1] depth mapping.

## Performance Characteristics

- **Shadow Map Resolution**: 2048×2048 = 4.2M pixels
- **Memory Usage**: 16 MB (D32_SFLOAT)
- **PCF Cost**: 9 shadow map samples per fragment
- **Comparison Sampler**: Hardware-accelerated depth comparison (very efficient)

## Future Improvements

1. **Cascaded Shadow Maps**: Multiple shadow maps at different distances
2. **Tight Shadow Frustum**: Fit frustum to visible scene bounds
3. **Slope-Based Bias**: Adjust bias based on surface angle
4. **Point Light Shadows**: Cube map shadows for omnidirectional lights
5. **Variance Shadow Maps**: Pre-filtering for better soft shadows

## References

- LearnOpenGL - Shadow Mapping: https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
- Vulkan Specification - Coordinate Systems: https://registry.khronos.org/vulkan/specs/1.3/html/chap24.html
- GPU Gems 3 - Cascaded Shadow Maps: https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus

## Next Milestone

**Milestone 3.4**: Image-Based Lighting (IBL) Phase 2
- Environment map rotation
- Dynamic environment switching
- HDR environment map support
- Spherical harmonics for diffuse IBL
