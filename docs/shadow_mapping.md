# Shadow Mapping

This document describes the shadow mapping implementation in MetaGFX, which provides realistic real-time shadows from directional lights using a depth-based technique with Percentage Closer Filtering (PCF).

## Overview

Shadow mapping is a two-pass rendering technique:

1. **Shadow Pass**: Render the scene from the light's perspective into a depth texture (shadow map)
2. **Main Pass**: Render the scene from the camera's perspective, sampling the shadow map to determine if fragments are in shadow

MetaGFX implements **directional light shadows** with:
- Vulkan-specific depth convention ([0,1] NDC range)
- Percentage Closer Filtering (PCF) for soft shadows
- Comparison samplers for hardware-accelerated shadow testing
- Configurable shadow bias to prevent shadow acne
- Interactive light direction controls

## Architecture

### Core Components

**ShadowMap Class** ([src/scene/ShadowMap.cpp](../src/scene/ShadowMap.cpp)):
- Creates shadow map framebuffer (depth-only rendering target)
- Manages shadow map texture (2048x2048 by default)
- Computes light-space transformation matrix
- Provides comparison sampler for PCF

**Shadow Pipeline** ([src/app/Application.cpp](../src/app/Application.cpp)):
- Depth-only rendering pipeline for shadow pass
- Simple vertex shader transforms vertices to light space
- No fragment shader needed (depth writes are automatic)

**Shadow Descriptor Set**:
- Bindings for shadow uniform buffer (light-space matrix + bias)
- Shared by all meshes during shadow pass

### Resource Flow

```
┌─────────────────────────────────────────────────────────────┐
│                      SHADOW PASS                            │
├─────────────────────────────────────────────────────────────┤
│  1. Bind shadow pipeline (depth-only)                       │
│  2. Bind shadow framebuffer (depth texture attachment)      │
│  3. Update shadow UBO (light-space matrix)                  │
│  4. Render models from light's perspective                  │
│  5. Depth values written to shadow map texture              │
│                                                             │
│  Output: Shadow map depth texture                           │
└─────────────────────────────────────────────────────────────┘
                             ↓
┌─────────────────────────────────────────────────────────────┐
│                      MAIN PASS                              │
├─────────────────────────────────────────────────────────────┤
│  1. Bind main pipeline (PBR lighting)                       │
│  2. Bind main framebuffer (color + depth)                   │
│  3. Shadow map texture bound as sampler (binding 12)        │
│  4. For each fragment:                                      │
│     a. Transform to light space                             │
│     b. Sample shadow map with PCF                           │
│     c. Compute shadow factor (0.0 = shadowed, 1.0 = lit)    │
│     d. Multiply lighting by shadow factor                   │
│                                                             │
│  Output: Final rendered image with shadows                  │
└─────────────────────────────────────────────────────────────┘
```

## Implementation Details

### Vulkan Depth Convention

**Critical**: Vulkan uses [0,1] NDC depth range, unlike OpenGL's [-1,1]. The shadow map implementation manually constructs the orthographic projection matrix to ensure correct depth mapping:

```cpp
// Vulkan orthographic projection for shadow map
glm::mat4 lightProjection = glm::mat4(1.0f);
lightProjection[0][0] = 1.0f / orthoSize;  // Scale X
lightProjection[1][1] = 1.0f / orthoSize;  // Scale Y
lightProjection[2][2] = -1.0f / (farPlane - nearPlane);  // Scale Z
lightProjection[3][2] = -nearPlane / (farPlane - nearPlane);  // Translate Z

// This maps view-space depth [-far, -near] to NDC [0, 1]
```

**Why not use `glm::ortho()`?**
GLM's `glm::ortho()` uses OpenGL convention and maps depth to [-1,1], which is incompatible with Vulkan. Using it would result in negative depth values and incorrect shadow calculations.

### Shadow Map Creation

```cpp
// Create depth texture for shadow map
rhi::TextureDesc depthDesc{};
depthDesc.type = rhi::TextureType::Texture2D;
depthDesc.width = 2048;   // High resolution for sharp shadows
depthDesc.height = 2048;
depthDesc.format = rhi::Format::D32_SFLOAT;  // 32-bit float depth
depthDesc.usage = rhi::TextureUsage::DepthStencilAttachment |
                  rhi::TextureUsage::Sampled;  // Both render target and sampler
depthDesc.mipLevels = 1;
depthDesc.arrayLayers = 1;

m_DepthTexture = device->CreateTexture(depthDesc);
```

### Comparison Sampler

Hardware-accelerated shadow comparison using a comparison sampler:

```cpp
rhi::SamplerDesc samplerDesc{};
samplerDesc.minFilter = rhi::Filter::Linear;
samplerDesc.magFilter = rhi::Filter::Linear;
samplerDesc.mipmapMode = rhi::Filter::Linear;
samplerDesc.addressModeU = rhi::SamplerAddressMode::ClampToEdge;
samplerDesc.addressModeV = rhi::SamplerAddressMode::ClampToEdge;
samplerDesc.addressModeW = rhi::SamplerAddressMode::ClampToEdge;
samplerDesc.enableCompare = true;  // Enable depth comparison
samplerDesc.compareOp = rhi::CompareOp::LessOrEqual;  // Standard depth test

m_Sampler = device->CreateSampler(samplerDesc);
```

The comparison sampler automatically performs depth comparison during sampling, returning 0.0 (shadowed) or 1.0 (lit) for each tap.

### Shadow Pass Rendering

**Shadow Vertex Shader** ([src/app/shadowmap.vert](../src/app/shadowmap.vert)):

```glsl
#version 450

layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix;  // Light's view-projection matrix
    mat4 model;             // Model matrix (identity for now)
    float shadowBias;
    float padding[3];
} ubo;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = ubo.lightSpaceMatrix * ubo.model * vec4(inPosition, 1.0);
}
```

**Key Points**:
- Only vertex positions needed (no normals, UVs, or colors)
- No fragment shader required - depth writes are automatic
- Transforms vertices from model space to light NDC space
- Model matrix is identity (models are already positioned at origin)

### Shadow Sampling (PCF)

**Fragment Shader Shadow Calculation** ([src/app/model.frag](../src/app/model.frag)):

```glsl
float calculateShadow(vec3 fragPos) {
    // Transform fragment position to light space
    vec4 fragPosLightSpace = shadow.lightSpaceMatrix * vec4(fragPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform X/Y to [0,1] range for texture sampling
    // Z is already in [0,1] for Vulkan (no transformation needed)
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Check if fragment is outside shadow map bounds
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;  // Outside shadow map = fully lit
    }

    // Apply depth bias to prevent shadow acne
    float currentDepth = projCoords.z - shadow.shadowBias;
    currentDepth = clamp(currentDepth, 0.0, 1.0);

    // PCF with 3x3 kernel for soft shadows
    float shadowFactor = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMapSampler, 0);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            vec3 sampleCoord = vec3(projCoords.xy + offset, currentDepth);

            // Comparison sampler returns 0.0 or 1.0
            shadowFactor += texture(shadowMapSampler, sampleCoord).r;
        }
    }

    shadowFactor /= 9.0;  // Average of 9 samples
    return shadowFactor;  // 0.0 = fully shadowed, 1.0 = fully lit
}
```

**PCF (Percentage Closer Filtering)**:
- Samples 9 texels in a 3x3 grid around the fragment
- Each sample performs hardware depth comparison
- Averages the results for soft shadow edges
- Reduces aliasing and creates penumbra-like effects

### Shadow Bias

Shadow bias prevents **shadow acne** (self-shadowing artifacts):

```cpp
float currentDepth = projCoords.z - shadow.shadowBias;
```

**Why needed?**
- Depth precision and surface angle cause surfaces to incorrectly shadow themselves
- Bias offsets the depth comparison slightly
- Too little bias = shadow acne (surface appears striped)
- Too much bias = **peter panning** (shadows detach from objects)

**Default value**: 0.005 (configurable via UI slider: 0.0 to 0.01)

### Light-Space Matrix Calculation

The light-space matrix transforms world coordinates to light NDC coordinates:

```cpp
void ShadowMap::UpdateLightMatrix(const glm::vec3& lightDir, const Camera& camera) {
    // Orthographic projection parameters
    float orthoSize = 30.0f;  // Covers ±30 units in X and Z
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    // Manually construct Vulkan-style orthographic projection
    glm::mat4 lightProjection = glm::mat4(1.0f);
    lightProjection[0][0] = 1.0f / orthoSize;
    lightProjection[1][1] = 1.0f / orthoSize;
    lightProjection[2][2] = -1.0f / (farPlane - nearPlane);
    lightProjection[3][2] = -nearPlane / (farPlane - nearPlane);

    // Position light along negative light direction
    glm::vec3 lightPos = -glm::normalize(lightDir) * 60.0f;

    // Light looks at origin (scene center)
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Avoid degenerate case (light parallel to up vector)
    if (std::abs(glm::dot(glm::normalize(lightDir), up)) > 0.999f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::mat4 lightView = glm::lookAt(lightPos, target, up);

    // Combined transformation
    m_LightSpaceMatrix = lightProjection * lightView;
}
```

**Key Parameters**:
- `orthoSize = 30.0f`: Shadow frustum covers 60x60 units (±30 in X and Z)
- `lightPos`: Positioned 60 units along negative light direction
- `target`: Always looks at origin (scene center assumed at 0,0,0)
- `farPlane = 100.0f`: Captures shadows up to 100 units depth

**Future Improvement**: Compute tight bounding box around visible scene geometry to maximize shadow map resolution.

## Descriptor Bindings

Shadow mapping adds two new descriptor bindings to the main pipeline:

```cpp
// Binding 12: Shadow map sampler (comparison sampler)
{
    12,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    nullptr,  // buffer
    m_ShadowMap->GetDepthTexture(),
    m_ShadowMap->GetSampler()  // Comparison sampler
}

// Binding 13: Shadow UBO (light-space matrix + bias)
{
    13,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    m_ShadowUniformBuffer,
    nullptr,  // texture
    nullptr   // sampler
}
```

**Shadow Uniform Buffer**:

```cpp
struct ShadowUBO {
    glm::mat4 lightSpaceMatrix;  // Transform world → light NDC
    glm::mat4 model;             // Model matrix (identity)
    float shadowBias;            // Depth bias (default 0.005)
    float padding[3];            // Alignment
};
```

## Shadow Pass vs Main Pass

### What Gets Rendered

**Shadow Pass**:
- ✅ Model meshes (shadow casters)
- ❌ Ground plane (shadow receiver only)
- ❌ Skybox (doesn't cast shadows)

**Main Pass**:
- ✅ Model meshes (with shadows applied)
- ✅ Ground plane (receives shadows)
- ✅ Skybox (background)

**Critical Rule**: Shadow receivers (ground plane) must NOT be rendered in the shadow pass, or they will write depth values that interfere with shadow calculations.

### Pipeline Differences

| Feature | Shadow Pipeline | Main Pipeline |
|---------|----------------|---------------|
| **Color Attachments** | None (depth-only) | 1 (RGBA back buffer) |
| **Depth Attachment** | Shadow map texture | Scene depth buffer |
| **Vertex Shader** | `shadowmap.vert` | `model.vert` |
| **Fragment Shader** | None | `model.frag` |
| **Vertex Attributes** | Position only | Position + Normal + TexCoord |
| **Descriptor Sets** | Shadow descriptor set | Main descriptor set |
| **Culling** | Back-face (front face = CCW) | Back-face (front face = CCW) |

## Ground Plane Shadow Reception

The ground plane has a **separate descriptor set** to avoid resource conflicts with model materials:

```cpp
// Ground plane descriptor set (same layout as model, but separate instance)
std::vector<rhi::DescriptorBinding> groundPlaneBindings = bindings;  // Copy layout

// Use dedicated material buffer for ground plane
groundPlaneBindings[1].buffer = m_GroundPlaneMaterialBuffer;

// Set default textures (no fancy materials)
groundPlaneBindings[2].texture = m_DefaultWhiteTexture;   // Albedo
groundPlaneBindings[11].texture = m_DefaultBlackTexture;  // Emissive (no glow)
// ... other textures ...

m_GroundPlaneDescriptorSet = std::make_unique<rhi::VulkanDescriptorSet>(
    context, groundPlaneBindings
);
```

**Why separate descriptor set?**
- Ground plane and model share the same pipeline (same layout)
- But they need different material properties
- Sharing buffers would cause the ground plane material to overwrite model materials
- Separate buffer prevents conflicts, especially with emissive textures

**Rendering**:

```cpp
// Render ground plane with grey material
MaterialProperties groundMat{};
groundMat.albedo = glm::vec3(0.35f, 0.35f, 0.35f);  // Dark grey
groundMat.roughness = 0.9f;   // Very rough
groundMat.metallic = 0.0f;    // Not metallic
groundMat.emissiveFactor = glm::vec3(0.0f);  // No emission

m_GroundPlaneMaterialBuffer->CopyData(&groundMat, sizeof(groundMat));

// Bind ground plane descriptor set
vkCmd->BindDescriptorSet(vkPipeline->GetLayout(),
                         m_GroundPlaneDescriptorSet->GetSet(m_CurrentFrame));

// Draw ground plane
for (const auto& mesh : m_GroundPlane->GetMeshes()) {
    cmd->DrawIndexed(mesh->GetIndexCount());
}
```

## UI Controls

Interactive shadow controls available in the ImGui interface:

**Shadow Toggle**:
- Enable/disable shadows entirely

**Shadow Bias** (slider: 0.0 to 0.01):
- Adjusts depth bias to prevent shadow acne
- Default: 0.005
- Lower values: More accurate shadows, risk of acne
- Higher values: Less acne, risk of peter panning

**Light Direction** (X, Y, Z sliders: -2.0 to 2.0):
- Dynamically adjust shadow-casting light direction
- X: Left (-) / Right (+)
- Y: Down (-) / Up (+)
- Z: Forward (-) / Backward (+)
- Default: (0.5, -1.0, -0.3) - from above-left-front
- **Reset Button**: Restores default light direction

**Ground Plane Toggle**:
- Show/hide ground plane for shadow visualization

**Shadow Debug Modes**:
- **Off**: Normal rendering
- **Shadow Factor**: White = lit, Black = shadowed
- **Normals**: Visualize vertex normals
- **Depth & Factor**: Combined depth and shadow visualization
- **Depth Color**: Depth buffer color-coded
- **Shadow Grayscale**: Grayscale shadow intensity
- **Depth vs Sample**: Compare sampled vs current depth

## Performance Considerations

### Shadow Map Resolution

**Default**: 2048x2048 (configurable in `ShadowMap` constructor)

**Trade-offs**:
- Higher resolution: Sharper shadow edges, more memory, slower rendering
- Lower resolution: Softer/blurrier shadows, less memory, faster rendering

**Memory usage**: 2048×2048×4 bytes (D32_SFLOAT) = 16 MB

### PCF Kernel Size

**Current**: 3×3 kernel (9 samples)

**Trade-offs**:
- Larger kernel (5×5, 7×7): Softer shadows, more expensive
- Smaller kernel (1×1): Sharper shadows, cheaper, more aliasing
- Comparison sampler makes each tap very cheap (hardware-accelerated)

### Render Pass Overhead

Shadow mapping adds one extra render pass per frame:
1. Shadow pass (depth-only, simpler than main pass)
2. Main pass (full PBR lighting with shadow sampling)

**Optimization**: Shadow pass only renders shadow casters (models), not receivers (ground plane) or skybox.

## Limitations and Future Work

### Current Limitations

1. **Single Shadow-Casting Light**: Only the first directional light casts shadows
2. **Fixed Orthographic Frustum**: Shadow frustum is fixed at origin, not fitted to scene bounds
3. **No Cascaded Shadow Maps**: Single shadow map covers entire scene (limited resolution)
4. **Directional Lights Only**: Point lights and spot lights don't cast shadows yet
5. **Static Bias**: Single bias value for all geometry (some surfaces may need different bias)

### Future Improvements

**Cascaded Shadow Maps (CSM)**:
- Multiple shadow maps at different distances from camera
- Near objects get high-resolution shadows
- Far objects get lower-resolution shadows
- Better shadow quality across large scenes

**Tight Shadow Frustum**:
- Compute bounding box of visible geometry
- Fit shadow frustum to actual scene bounds
- Maximizes shadow map resolution utilization
- Reduces wasted shadow map space

**Slope-Based Bias**:
- Adjust bias based on surface angle relative to light
- `bias = baseBias * tan(acos(dot(N, L)))`
- Prevents both shadow acne and peter panning

**Point Light Shadows**:
- Use cube map for omnidirectional shadows
- Render 6 shadow maps (one per cube face)
- Sample appropriate face based on light-to-fragment vector

**Variance Shadow Maps (VSM)**:
- Store mean and variance of depth
- Enables pre-filtering and better soft shadows
- Trades precision for filtering quality

**Percentage Closer Soft Shadows (PCSS)**:
- Variable penumbra width based on occluder distance
- More realistic soft shadow falloff
- More expensive than PCF

## Debugging Shadow Issues

### Common Problems

**1. No Shadows Visible**

Possible causes:
- Shadow map not bound to correct descriptor binding (should be binding 12)
- Light-space matrix incorrect (check debug logs)
- Shadow bias too high (reduce bias)
- Models outside shadow frustum (increase `orthoSize`)

**Solution**: Enable "Shadow Factor" debug mode to visualize shadow calculations.

**2. Shadow Acne (Striped Patterns on Surfaces)**

Cause: Depth precision errors cause surface to shadow itself

**Solution**: Increase shadow bias (try 0.005 to 0.01).

**3. Peter Panning (Shadows Detached from Objects)**

Cause: Shadow bias too high

**Solution**: Reduce shadow bias (try 0.001 to 0.005).

**4. Blocky/Aliased Shadow Edges**

Cause: Shadow map resolution too low or PCF kernel too small

**Solution**: Increase shadow map resolution or PCF kernel size.

**5. Shadows in Wrong Direction**

Cause: Incorrect light direction

**Solution**: Adjust light direction using UI sliders.

### Debug Logging

Enable shadow debug logging in [ShadowMap.cpp](../src/scene/ShadowMap.cpp):

```cpp
METAGFX_INFO << "Shadow frustum - orthoSize: " << orthoSize
             << ", near: " << nearPlane << ", far: " << farPlane;
METAGFX_INFO << "Test: Origin (0,0,0) -> Light NDC: (" << ... << ")";
```

And in [Application.cpp](../src/app/Application.cpp):

```cpp
METAGFX_INFO << "Shadow pass rendered " << meshesRendered << " meshes";
METAGFX_INFO << "Model bounds: min(...), max(...)";
```

## Related Documentation

- [PBR Rendering](pbr_rendering.md) - How shadows integrate with PBR lighting
- [RHI Architecture](rhi.md) - Graphics API abstraction layer
- [Textures and Samplers](textures_and_samplers.md) - Texture creation and sampling
- [Material System](material_system.md) - Material properties and uniform buffers
- [Resource Management](resource_management.md) - GPU resource lifetimes and cleanup

## References

- **Shadow Mapping Tutorial**: LearnOpenGL - Shadow Mapping ([learnopengl.com](https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping))
- **Vulkan Depth Convention**: Vulkan Specification - Coordinate Systems ([khronos.org](https://registry.khronos.org/vulkan/specs/1.3/html/chap24.html#vertexpostproc-coord))
- **PCF Filtering**: Real-Time Rendering 4th Edition, Chapter 7.4
- **Cascaded Shadow Maps**: GPU Gems 3, Chapter 10 ([nvidia.com](https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus))
