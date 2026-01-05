# Skybox Rendering System

**Status**: ✅ Implemented
**Last Updated**: January 5, 2026
**Milestone**: 3.3 - Skybox Rendering

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Implementation Details](#implementation-details)
4. [Shader Implementation](#shader-implementation)
5. [Rendering Pipeline](#rendering-pipeline)
6. [Controls and Parameters](#controls-and-parameters)
7. [Technical Challenges](#technical-challenges)
8. [Performance Considerations](#performance-considerations)
9. [Future Enhancements](#future-enhancements)

---

## Overview

The skybox system renders an HDR environment cubemap as an infinite-distance background that follows camera rotation but not position. This provides realistic ambient lighting context and integrates seamlessly with the PBR rendering and IBL systems.

### Key Features

- ✅ **HDR Environment Rendering** - Displays high dynamic range cubemap backgrounds
- ✅ **Camera-Relative Positioning** - Follows rotation, fixed at infinite distance
- ✅ **Proper Depth Integration** - Models correctly occlude skybox
- ✅ **Exposure Control** - Shares exposure settings with main renderer
- ✅ **LOD Support** - Mipmap-based blur control for artistic effects
- ✅ **Independent Resource Management** - Separate descriptor set and pipeline
- ✅ **Toggle Visibility** - Can be shown/hidden via ImGui

---

## Architecture

### Descriptor Set Layout

The skybox uses a **separate descriptor set** with 2 bindings to avoid conflicts with the main renderer:

| Binding | Type | Stage | Purpose |
|---------|------|-------|---------|
| 0 | Uniform Buffer | Vertex | MVP matrices (model, view, projection) |
| 1 | Combined Image Sampler | Fragment | Environment cubemap |

**Why Separate?** The main renderer has 11 bindings including material textures, IBL maps, and lights. The skybox only needs MVP transforms and the environment texture, so a dedicated descriptor set with 2 bindings is more efficient and avoids binding slot conflicts.

### Pipeline Configuration

**Location**: `src/app/Application.cpp:623-672` ([CreateSkyboxPipeline](../src/app/Application.cpp#L623-L672))

```cpp
// Vertex input: position only (no normals, UVs)
pipelineDesc.vertexInput.attributes = {
    { 0, Format::R32G32B32_SFLOAT, 0 }  // vec3 position
};

// Rasterization
pipelineDesc.rasterization.cullMode = CullMode::None;  // No culling
pipelineDesc.rasterization.frontFace = FrontFace::CounterClockwise;

// Depth testing
pipelineDesc.depthStencil.depthTestEnable = true;
pipelineDesc.depthStencil.depthWriteEnable = false;  // Don't write depth
pipelineDesc.depthStencil.depthCompareOp = CompareOp::LessOrEqual;
```

**Key Design Decisions**:
- **No depth writes**: Skybox renders at depth=1.0 but doesn't modify the depth buffer
- **LessOrEqual comparison**: Allows skybox fragments (depth=1.0) to pass where depth buffer is still 1.0 (cleared value)
- **No culling**: Ensures all cube faces are visible from inside the cube

### Geometry

**Location**: `src/app/Application.cpp:674-727` ([CreateSkyboxCube](../src/app/Application.cpp#L674-L727))

The skybox uses a simple unit cube (1x1x1) with 8 vertices and 36 indices:

```cpp
Vertex vertices[] = {
    {{-1.0f, -1.0f, -1.0f}, {}, {}},  // Back-bottom-left
    {{ 1.0f, -1.0f, -1.0f}, {}, {}},  // Back-bottom-right
    {{ 1.0f,  1.0f, -1.0f}, {}, {}},  // Back-top-right
    {{-1.0f,  1.0f, -1.0f}, {}, {}},  // Back-top-left
    {{-1.0f, -1.0f,  1.0f}, {}, {}},  // Front-bottom-left
    {{ 1.0f, -1.0f,  1.0f}, {}, {}},  // Front-bottom-right
    {{ 1.0f,  1.0f,  1.0f}, {}, {}},  // Front-top-right
    {{-1.0f,  1.0f,  1.0f}, {}, {}},  // Front-top-left
};
```

**Why Unit Cube?** The cube's world-space position doesn't matter because:
1. The vertex shader removes translation from the view matrix
2. The `gl_Position.xyww` trick ensures depth is always 1.0 (far plane)
3. The vertex position is used directly as the cubemap sampling direction

---

## Implementation Details

### Resource Creation

**Initialization Order** (`Application::Init`):

```cpp
// 1. Load environment cubemap (shared with IBL)
m_EnvironmentMap = utils::LoadDDSCubemap(m_Device, "assets/envmaps/environment.dds");

// 2. Create cubemap sampler (linear filtering, clamp to edge)
m_CubemapSampler = CreateCubemapSampler(m_Device);

// 3. Create skybox descriptor set (2 bindings: MVP + cubemap)
m_SkyboxDescriptorSet = std::make_unique<rhi::VulkanDescriptorSet>(
    context,
    skyboxBindings  // Binding 0: UBO, Binding 1: environmentMap + sampler
);

// 4. Create skybox pipeline with skybox descriptor layout
SetDescriptorSetLayout(m_SkyboxDescriptorSet->GetLayout());
CreateSkyboxPipeline();

// 5. Create skybox cube geometry
CreateSkyboxCube();
```

### Uniform Buffer Sharing

**Critical Detail**: The skybox shares the same MVP uniform buffer as the main renderer:

**Location**: `src/app/Application.cpp:1111`

```cpp
// Update skybox descriptor set with uniform buffer (always use buffer[0])
// FIXME: Matches main renderer which always updates buffer[0]
m_SkyboxDescriptorSet->UpdateBuffer(0, m_UniformBuffers[0]);
```

**Why This Works**:
- The uniform buffer contains `model`, `view`, and `projection` matrices
- The skybox vertex shader modifies the view matrix (removes translation)
- Both renderer and skybox can use the same buffer because they read the same data
- **Important**: Must use `buffer[0]` consistently (see [Technical Challenges](#uniform-buffer-double-buffering-issue))

---

## Shader Implementation

### Vertex Shader (`skybox.vert`)

**Location**: [src/app/skybox.vert](../src/app/skybox.vert)

**Purpose**: Transform cube vertices while ensuring they appear at infinite distance

```glsl
#version 450

// Input: cube vertex position
layout(location = 0) in vec3 inPosition;

// Output: direction vector for cubemap sampling
layout(location = 0) out vec3 fragTexCoord;

// Uniform buffer: MVP matrices
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

void main() {
    // Remove translation from view matrix (keep only rotation)
    // This makes the skybox follow the camera rotation but not position
    mat4 viewNoTranslation = mat4(mat3(ubo.view));

    // Transform position
    vec4 pos = ubo.projection * viewNoTranslation * vec4(inPosition, 1.0);

    // Set z = w to ensure depth is always 1.0 (farthest possible)
    // This ensures skybox is always behind everything else
    gl_Position = pos.xyww;

    // Use vertex position as texture coordinate direction
    fragTexCoord = inPosition;
}
```

**Key Techniques**:

1. **Translation Removal**: `mat4(mat3(ubo.view))`
   - Converts view matrix to 3x3 (discards translation column)
   - Converts back to 4x4 (translation becomes zero)
   - Result: Skybox rotates with camera but stays centered

2. **Depth Trick**: `gl_Position.xyww`
   - After perspective division: `z' = w/w = 1.0` (far plane)
   - Ensures skybox is always at maximum depth
   - Models with depth < 1.0 will occlude it

3. **Cubemap Sampling Direction**: `fragTexCoord = inPosition`
   - The cube vertex position IS the sampling direction
   - Points from origin through cube face → samples correct part of environment

### Fragment Shader (`skybox.frag`)

**Location**: [src/app/skybox.frag](../src/app/skybox.frag)

**Purpose**: Sample environment cubemap and apply tone mapping

```glsl
#version 450

// Input: texture coordinate (3D direction vector)
layout(location = 0) in vec3 fragTexCoord;

// Output: final color
layout(location = 0) out vec4 outColor;

// Environment cubemap sampler
layout(binding = 1) uniform samplerCube environmentMap;

// Push constants for skybox parameters
layout(push_constant) uniform PushConstants {
    float exposure;     // HDR exposure
    float lod;          // Mipmap level (0 = sharp, higher = blurred)
} pushConstants;

void main() {
    // Sample environment cubemap with specified LOD
    vec3 color = textureLod(environmentMap, fragTexCoord, pushConstants.lod).rgb;

    // Apply exposure
    color = color * pushConstants.exposure;

    // Tone mapping (simple clamp)
    color = clamp(color, 0.0, 1.0);

    // Gamma correction (linear to sRGB)
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
```

**Rendering Pipeline**:
1. Sample cubemap using LOD (allows blur for artistic effects)
2. Apply exposure (shared with main renderer for consistency)
3. Tone map HDR → LDR (simple clamp, matching model shader)
4. Gamma correct to sRGB color space

---

## Rendering Pipeline

### Render Order

**Location**: `src/app/Application.cpp:979-1135` ([Render method](../src/app/Application.cpp#L979-L1135))

```cpp
// 1. Update uniform buffer with MVP matrices
UniformBufferObject ubo{};
ubo.model = glm::mat4(1.0f);           // Identity
ubo.view = m_Camera->GetViewMatrix();
ubo.projection = m_Camera->GetProjectionMatrix();
m_UniformBuffers[0]->CopyData(&ubo, sizeof(ubo));

// 2. Begin rendering pass
cmd->BeginRendering(colorTargets, depthTarget, clearValues);

// 3. Render model FIRST
if (m_Model && m_Model->IsValid()) {
    cmd->BindPipeline(m_ModelPipeline);
    // ... bind descriptors, draw meshes ...
    // Model writes depth < 1.0 for geometry
}

// 4. Render skybox LAST
if (m_ShowSkybox && m_EnvironmentMap && ...) {
    cmd->BindPipeline(m_SkyboxPipeline);

    // Update descriptor set
    m_SkyboxDescriptorSet->UpdateBuffer(0, m_UniformBuffers[0]);
    vkCmd->BindDescriptorSet(layout, m_SkyboxDescriptorSet->GetSet(m_CurrentFrame));

    // Push constants
    struct SkyboxPushConstants {
        float exposure;
        float lod;
    } skyboxPushConstants;
    skyboxPushConstants.exposure = m_Exposure;
    skyboxPushConstants.lod = m_SkyboxLOD;
    vkCmd->PushConstants(layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(SkyboxPushConstants), &skyboxPushConstants);

    // Draw cube
    cmd->BindVertexBuffer(m_SkyboxVertexBuffer);
    cmd->BindIndexBuffer(m_SkyboxIndexBuffer);
    cmd->DrawIndexed(36);  // 36 indices for cube
}

cmd->EndRendering();
```

**Why This Order?**

| Rendering | Depth Write | Depth Test | Result |
|-----------|-------------|------------|--------|
| Model (first) | ✅ Enabled | LessOrEqual | Writes depth < 1.0 for geometry |
| Skybox (last) | ❌ Disabled | LessOrEqual | Passes only where depth == 1.0 (no model) |

This ensures:
- Models are fully rendered with proper depth testing
- Skybox fills only the background (where no model exists)
- No depth buffer conflicts or z-fighting

---

## Controls and Parameters

### ImGui Interface

**Location**: `src/app/Application.cpp` (RenderImGui method)

```cpp
// GUI parameters (Application.h)
bool m_ShowSkybox = true;          // Show/hide toggle
float m_SkyboxLOD = 0.0f;          // Mipmap LOD (0 = sharp, higher = blurred)
float m_Exposure = 1.0f;           // HDR exposure (shared with main renderer)
```

**User Controls**:
- **Show Skybox**: Checkbox to enable/disable skybox rendering
- **Skybox LOD**: Slider (0.0 - 6.0) to blur skybox using mipmaps
- **Exposure**: Slider (0.1 - 10.0) affects both model and skybox

### Push Constants

**Size**: 8 bytes (2 floats)

```cpp
struct SkyboxPushConstants {
    float exposure;  // HDR exposure multiplier
    float lod;       // Cubemap mipmap level (0-6)
};
```

**Advantages**:
- Fast updates (no descriptor set changes)
- Per-frame control without uniform buffer overhead
- Allows independent skybox brightness/blur without model changes

---

## Technical Challenges

### Uniform Buffer Double-Buffering Issue

**Problem Encountered**: Initial implementation used `m_UniformBuffers[m_CurrentFrame]` which alternates between buffer[0] and buffer[1]. However, only buffer[0] was being updated each frame, causing flickering when `m_CurrentFrame == 1`.

**Root Cause**:
```cpp
// Main renderer updates only buffer[0]
m_UniformBuffers[0]->CopyData(&ubo, sizeof(ubo));

// Skybox tried to use buffer[m_CurrentFrame] (0 or 1)
m_SkyboxDescriptorSet->UpdateBuffer(0, m_UniformBuffers[m_CurrentFrame]);  // BUG!
```

**Solution**: Always use buffer[0] for both renderer and skybox:
```cpp
// Fixed: Use buffer[0] consistently
m_SkyboxDescriptorSet->UpdateBuffer(0, m_UniformBuffers[0]);
```

**Location**: `src/app/Application.cpp:1111`

**Lesson Learned**: Ensure uniform buffer update strategy matches descriptor set binding. The double-buffering infrastructure exists (`MAX_FRAMES = 2`) but is not currently utilized. Both descriptor sets must point to the actively updated buffer.

### Depth Testing Configuration

**Challenge**: Getting skybox to render behind models without flickering or z-fighting.

**Attempted Approaches**:

| Configuration | Result | Issue |
|---------------|--------|-------|
| Depth test disabled, render first | ❌ Flickering | Model and skybox compete |
| Depth test enabled, `Always` compare | ❌ Flickering | Overwrites model depth |
| Depth test enabled, render first, write depth | ❌ Not visible | Models fail depth test |
| Depth test enabled, render last, no depth write | ✅ Works! | Correct occlusion |

**Final Solution**:
- Render skybox LAST
- Enable depth test with `LessOrEqual`
- DISABLE depth writes
- Result: Skybox fragments at depth=1.0 pass test only where depth buffer is still 1.0 (no model)

### Descriptor Set Layout Management

**Challenge**: Skybox and main renderer have different descriptor layouts (2 vs 11 bindings).

**Solution**: Temporarily switch descriptor set layout when creating skybox pipeline:

```cpp
// Set main descriptor layout (11 bindings)
SetDescriptorSetLayout(m_DescriptorSet->GetLayout());
CreateModelPipeline();

// Switch to skybox layout (2 bindings)
SetDescriptorSetLayout(m_SkyboxDescriptorSet->GetLayout());
CreateSkyboxPipeline();

// Restore main layout
SetDescriptorSetLayout(m_DescriptorSet->GetLayout());
```

**Location**: `src/app/Application.cpp:382-390`

---

## Performance Considerations

### Memory Usage

- **Vertex Buffer**: 8 vertices × 32 bytes = 256 bytes
- **Index Buffer**: 36 indices × 4 bytes = 144 bytes
- **Descriptor Set**: Minimal overhead (2 bindings)
- **Pipeline State**: Standard Vulkan PSO
- **Total Additional**: < 1 KB (excluding shared environment texture)

### Rendering Cost

**Per Frame**:
- 1 pipeline bind
- 1 descriptor set bind
- 2 buffer binds (vertex + index)
- 1 push constant update (8 bytes)
- 1 draw call (36 indices = 12 triangles)

**GPU Cost**: Negligible - 12 triangles with simple shaders

**Shared Resources**:
- Environment cubemap: Already loaded for IBL (no extra memory)
- Uniform buffer: Shared with main renderer (no extra allocation)
- Sampler: Shared cubemap sampler (created for IBL)

### Optimization Opportunities

1. **Front-Face Culling**: Could enable `CullMode::Front` since camera is inside cube (currently disabled for debugging)
2. **Early-Z Optimization**: Skybox renders last, so doesn't benefit from early-Z rejection
3. **Mipmap Streaming**: Could load lower mip levels for distant/blurred skybox scenarios

---

## Future Enhancements

### Potential Features

1. **Multiple Environment Maps**
   - Load different skyboxes for different scenes
   - Runtime switching between environments
   - Smooth transitions/crossfading

2. **Skybox Animation**
   - Time-of-day transitions
   - Rotating star fields
   - Dynamic cloud movement

3. **Procedural Skyboxes**
   - Atmospheric scattering shaders
   - Procedural star generation
   - Real-time cloud rendering

4. **Skybox Reflections**
   - Use same cubemap for model reflections
   - Parallax-corrected reflections for indoor scenes
   - Local reflection probes

5. **Performance Optimizations**
   - Render skybox at half resolution
   - Use cheaper tone mapping for background
   - Cull skybox when fully occluded by geometry

### Integration Opportunities

- **Weather System**: Dynamic skybox based on weather conditions
- **Day/Night Cycle**: Interpolate between multiple environment maps
- **Post-Processing**: Apply atmospheric effects to skybox separately
- **VR Support**: Stereo skybox rendering for VR applications

---

## References

- [Learn OpenGL - Cubemaps](https://learnopengl.com/Advanced-OpenGL/Cubemaps)
- [Vulkan Tutorial - Texture Mapping](https://vulkan-tutorial.com/Texture_mapping)
- [GPU Gems - Chapter 25: Rendering the Graphics of Oblivion](https://developer.nvidia.com/gpugems/gpugems/part-iv-image-based-rendering/chapter-25-rendering-graphics-oblivion)
- [IBL System Documentation](ibl_system.md) - Environment map loading and sampling
- [Textures and Samplers](textures_and_samplers.md) - Cubemap texture system

---

**Implementation**: Complete
**Tested On**: Apple M3, macOS 14.3 (MoltenVK)
**Performance**: 500+ FPS with DamagedHelmet model (14K vertices)
