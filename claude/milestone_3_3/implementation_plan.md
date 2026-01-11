# Milestone 3.3: Shadow Mapping - Implementation Plan

**Status**: Planning Phase
**Date**: January 9, 2026
**Estimated Time**: 17-25 hours

---

## Overview

**Goal**: Implement dynamic shadow casting from directional lights using shadow mapping technique as part of the **Rasterization Mode** rendering architecture.

**Scope**:
- Establish modular renderer architecture foundation (abstract `Renderer` base class)
- Implement `RasterizationRenderer` with shadow mapping support
- Single directional light shadows with PCF (Percentage Closer Filtering) soft shadows
- Configurable quality settings and ImGui debug controls
- Designed to coexist with future hybrid (ray traced shadows) and path tracing modes

**Architecture Context**: This milestone implements the **Rasterization Mode** shadow system. Future milestones will add:
- **Milestone 6.3** (Hybrid Mode): Ray traced shadows using hardware RT pipeline
- **Milestone 7.1** (Path Tracing Mode): Physically accurate soft shadows via path tracing

---

## Current State Analysis

### ✅ What We Have
- Full PBR rendering pipeline with Cook-Torrance BRDF
- Light system with directional and point lights (`docs/light_system.md`)
- Multiple render passes (skybox + model rendering)
- Descriptor sets with 12 bindings
- Push constants system (32 bytes)
- Camera system with view/projection matrices
- glTF model loading with materials
- ImGui integration for runtime controls

### ❌ What We Need
- Depth texture creation and management
- Framebuffer abstraction for render-to-texture
- Shadow map render pass
- Light space matrix calculation
- Shadow sampling in fragment shader
- PCF (Percentage Closer Filtering) for soft shadows
- Shadow bias to prevent shadow acne
- Depth comparison sampler

---

## Architecture Design

### 0. Modular Renderer Architecture (NEW)

**Rendering Mode Foundation**: Establish abstract renderer architecture to support multiple rendering modes.

**New Abstractions**:

```cpp
// include/metagfx/renderer/Renderer.h
class Renderer {
public:
    virtual ~Renderer() = default;

    // Core rendering interface
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Render(const Scene& scene, const Camera& camera) = 0;
    virtual void OnResize(uint32 width, uint32 height) = 0;

    // Renderer capabilities
    virtual const char* GetName() const = 0;
    virtual bool SupportsFeature(RenderFeature feature) const = 0;

protected:
    Ref<rhi::GraphicsDevice> m_Device;
};

// Rendering modes enum
enum class RenderMode {
    Rasterization,  // Shadow mapping (Milestone 3.3)
    Hybrid,         // Rasterization + RT effects (Milestone 6.3)
    PathTracing     // Full ray tracing (Milestone 7.1)
};

// Renderer features
enum class RenderFeature {
    Shadows,
    RayTracedShadows,
    Reflections,
    GlobalIllumination,
    // ... more features
};
```

**RasterizationRenderer** (`include/metagfx/renderer/RasterizationRenderer.h`):

```cpp
class RasterizationRenderer : public Renderer {
public:
    RasterizationRenderer(Ref<rhi::GraphicsDevice> device);
    ~RasterizationRenderer() override;

    void Initialize() override;
    void Shutdown() override;
    void Render(const Scene& scene, const Camera& camera) override;
    void OnResize(uint32 width, uint32 height) override;

    const char* GetName() const override { return "Rasterization"; }
    bool SupportsFeature(RenderFeature feature) const override;

    // Rasterization-specific settings
    void SetShadowsEnabled(bool enabled) { m_EnableShadows = enabled; }
    void SetShadowMapSize(uint32 size);
    void SetShadowBias(float bias) { m_ShadowBias = bias; }

private:
    // Shadow system
    std::unique_ptr<ShadowMap> m_ShadowMap;
    Ref<rhi::Pipeline> m_ShadowPipeline;
    bool m_EnableShadows = true;
    uint32 m_ShadowMapSize = 2048;
    float m_ShadowBias = 0.005f;

    // Render passes
    void RenderShadowPass(Ref<rhi::CommandBuffer> cmd, const Scene& scene);
    void RenderMainPass(Ref<rhi::CommandBuffer> cmd, const Scene& scene, const Camera& camera);
};
```

**Application Integration**:

```cpp
// Application.h
class Application {
private:
    std::unique_ptr<Renderer> m_Renderer;
    RenderMode m_CurrentMode = RenderMode::Rasterization;

    void SetRenderMode(RenderMode mode);
    void CreateRenderer();
};

// Application.cpp
void Application::CreateRenderer() {
    switch (m_CurrentMode) {
        case RenderMode::Rasterization:
            m_Renderer = std::make_unique<RasterizationRenderer>(m_Device);
            break;
        case RenderMode::Hybrid:
            // Future: HybridRenderer (Milestone 6.3)
            break;
        case RenderMode::PathTracing:
            // Future: PathTracingRenderer (Milestone 7.1)
            break;
    }
    m_Renderer->Initialize();
}

void Application::Render() {
    // Simple delegation to current renderer
    m_Renderer->Render(*m_Scene, *m_Camera);
}
```

**Benefits of This Design**:
- ✅ **Clean Separation**: Each rendering mode is self-contained
- ✅ **Runtime Switching**: User can toggle modes without scene reload
- ✅ **Shared Resources**: Scene, camera, models, materials are mode-agnostic
- ✅ **Future-Proof**: Easy to add hybrid and path tracing modes later
- ✅ **Testable**: Can compare rendering modes side-by-side

### 1. RHI Extensions

**New Abstractions**:

```cpp
// include/metagfx/rhi/Framebuffer.h
class Framebuffer {
public:
    virtual ~Framebuffer() = default;
    virtual void Bind() = 0;
    virtual void Unbind() = 0;
    virtual Ref<Texture> GetDepthAttachment() const = 0;
    virtual uint32 GetWidth() const = 0;
    virtual uint32 GetHeight() const = 0;
};
```

**Texture Format Extensions** (`include/metagfx/rhi/Types.h`):

```cpp
enum class Format {
    // ... existing formats ...
    D32_SFLOAT,              // 32-bit depth (recommended for shadows)
    D24_UNORM_S8_UINT,       // 24-bit depth + 8-bit stencil
    D16_UNORM                // 16-bit depth (lower quality)
};

enum class TextureUsage {
    ColorAttachment,
    DepthAttachment,         // NEW
    DepthStencilAttachment,
    Sampled,
    Storage
};
```

**Sampler Extensions** (`include/metagfx/rhi/Sampler.h`):

```cpp
enum class CompareOp {
    Never, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always
};

struct SamplerDesc {
    // ... existing fields ...
    bool enableCompare = false;
    CompareOp compareOp = CompareOp::LessOrEqual;
};
```

### 2. Shadow Map System

**New Class** (`include/metagfx/scene/ShadowMap.h`):

```cpp
class ShadowMap {
public:
    ShadowMap(Ref<rhi::GraphicsDevice> device, uint32 width, uint32 height);

    // Access methods
    Ref<rhi::Texture> GetDepthTexture() const { return m_DepthTexture; }
    Ref<rhi::Framebuffer> GetFramebuffer() const { return m_Framebuffer; }
    glm::mat4 GetLightSpaceMatrix() const { return m_LightSpaceMatrix; }
    uint32 GetWidth() const { return m_Width; }
    uint32 GetHeight() const { return m_Height; }

    // Shadow map generation
    void UpdateLightMatrix(const glm::vec3& lightDir, const Camera& camera);
    void Resize(uint32 width, uint32 height);

private:
    Ref<rhi::GraphicsDevice> m_Device;
    Ref<rhi::Texture> m_DepthTexture;
    Ref<rhi::Framebuffer> m_Framebuffer;
    glm::mat4 m_LightSpaceMatrix;
    uint32 m_Width, m_Height;
};
```

**Light Space Matrix Calculation**:

```cpp
void ShadowMap::UpdateLightMatrix(const glm::vec3& lightDir, const Camera& camera) {
    // For directional light, create orthographic projection
    float orthoSize = 10.0f;  // Cover 20x20 area around origin
    glm::mat4 lightProjection = glm::ortho(
        -orthoSize, orthoSize,
        -orthoSize, orthoSize,
        0.1f, 50.0f  // Near and far planes
    );

    // Light looks from direction toward origin
    glm::vec3 lightPos = -glm::normalize(lightDir) * 20.0f;
    glm::mat4 lightView = glm::lookAt(
        lightPos,
        glm::vec3(0.0f, 0.0f, 0.0f),  // Look at origin
        glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
    );

    m_LightSpaceMatrix = lightProjection * lightView;
}
```

### 3. Rendering Pipeline Changes

**Current Pipeline**:
1. Update uniforms (camera, materials)
2. Begin rendering (color + depth targets)
3. Render skybox
4. Render models with PBR
5. Render ImGui
6. Present

**New Pipeline**:
1. **Shadow Pass** (NEW):
   - Set viewport to shadow map resolution
   - Bind shadow framebuffer (depth-only)
   - Update light space matrices
   - Bind shadow pipeline (simple depth-only shader)
   - Render all shadow casters
   - End shadow pass
2. **Main Pass**:
   - Bind shadow map texture to descriptor set binding 12
   - Update uniforms (camera, materials, light matrices)
   - Begin rendering (color + depth targets)
   - Render skybox (no shadow)
   - Render models with PBR + shadow sampling
   - Render ImGui
   - Present

---

## Implementation Steps

### Phase 0: Renderer Architecture Foundation (NEW)

**Duration**: 2-3 hours

**Purpose**: Establish modular renderer architecture that will support rasterization, hybrid, and path tracing modes.

#### Files to Create:
1. `include/metagfx/renderer/Renderer.h` - Abstract renderer base class
2. `include/metagfx/renderer/RasterizationRenderer.h` - Rasterization mode renderer header
3. `src/renderer/RasterizationRenderer.cpp` - Rasterization mode renderer implementation
4. `src/renderer/CMakeLists.txt` - Renderer module build configuration

#### Files to Modify:
1. `CMakeLists.txt` (root) - Add renderer subdirectory
2. `src/app/Application.h` - Add renderer abstraction
3. `src/app/Application.cpp` - Migrate rendering logic to RasterizationRenderer

#### Implementation Details:

**Renderer.h** - Base abstraction:
```cpp
namespace metagfx {

enum class RenderMode {
    Rasterization,
    Hybrid,
    PathTracing
};

enum class RenderFeature {
    Shadows,
    RayTracedShadows,
    Reflections,
    GlobalIllumination
};

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Render(Scene& scene, Camera& camera) = 0;
    virtual void OnResize(uint32 width, uint32 height) = 0;

    virtual const char* GetName() const = 0;
    virtual RenderMode GetMode() const = 0;
    virtual bool SupportsFeature(RenderFeature feature) const = 0;

protected:
    Ref<rhi::GraphicsDevice> m_Device;
};

} // namespace metagfx
```

**RasterizationRenderer.h** - Initial implementation:
```cpp
class RasterizationRenderer : public Renderer {
public:
    explicit RasterizationRenderer(Ref<rhi::GraphicsDevice> device);
    ~RasterizationRenderer() override;

    void Initialize() override;
    void Shutdown() override;
    void Render(Scene& scene, Camera& camera) override;
    void OnResize(uint32 width, uint32 height) override;

    const char* GetName() const override { return "Rasterization"; }
    RenderMode GetMode() const override { return RenderMode::Rasterization; }
    bool SupportsFeature(RenderFeature feature) const override;

    // Shadow settings (accessed via Application for ImGui)
    void SetShadowsEnabled(bool enabled) { m_EnableShadows = enabled; }
    bool GetShadowsEnabled() const { return m_EnableShadows; }
    void SetShadowMapSize(uint32 size);
    uint32 GetShadowMapSize() const { return m_ShadowMapSize; }
    void SetShadowBias(float bias) { m_ShadowBias = bias; }
    float GetShadowBias() const { return m_ShadowBias; }

private:
    // Rendering resources (migrated from Application)
    Ref<rhi::Pipeline> m_ModelPipeline;
    Ref<rhi::Pipeline> m_SkyboxPipeline;
    Ref<VulkanDescriptorSet> m_DescriptorSet;

    // Shadow mapping (new)
    std::unique_ptr<ShadowMap> m_ShadowMap;
    Ref<rhi::Pipeline> m_ShadowPipeline;
    bool m_EnableShadows = true;
    uint32 m_ShadowMapSize = 2048;
    float m_ShadowBias = 0.005f;

    void RenderShadowPass(Ref<rhi::CommandBuffer> cmd, Scene& scene);
    void RenderMainPass(Ref<rhi::CommandBuffer> cmd, Scene& scene, Camera& camera);
};
```

**Migration Strategy**:
- Move rendering logic from `Application::Render()` to `RasterizationRenderer::Render()`
- Keep ImGui, window management, and input in `Application`
- `Application` delegates rendering to `m_Renderer->Render()`

**Why This Phase First?**:
- Establishes clean architecture before adding shadow complexity
- Makes future hybrid/path tracing modes easier to implement
- Separates concerns: Application = window/input/UI, Renderer = graphics
- Allows side-by-side mode comparison in future

### Phase 1: RHI Extensions (Foundation)

**Duration**: 2-3 hours

#### Files to Create:
1. `include/metagfx/rhi/Framebuffer.h` - Abstract framebuffer interface
2. `src/rhi/vulkan/VulkanFramebuffer.h` - Vulkan framebuffer header
3. `src/rhi/vulkan/VulkanFramebuffer.cpp` - Vulkan framebuffer implementation

#### Files to Modify:
1. `include/metagfx/rhi/Types.h`:
   - Add `Format::D32_SFLOAT`, `Format::D24_UNORM_S8_UINT`, `Format::D16_UNORM`
   - Add `TextureUsage::DepthAttachment`
   - Add `CompareOp` enum

2. `include/metagfx/rhi/Texture.h`:
   - Add depth texture support in TextureDesc

3. `include/metagfx/rhi/Sampler.h`:
   - Add `enableCompare` and `compareOp` to `SamplerDesc`

4. `include/metagfx/rhi/GraphicsDevice.h`:
   - Add `CreateFramebuffer()` virtual method

5. `src/rhi/vulkan/VulkanTexture.cpp`:
   - Implement depth texture creation
   - Map depth formats to VkFormat

6. `src/rhi/vulkan/VulkanSampler.cpp`:
   - Implement comparison sampler creation

7. `src/rhi/vulkan/VulkanDevice.cpp`:
   - Implement `CreateFramebuffer()` method

#### Key Implementation Details:

**VulkanFramebuffer.h**:
```cpp
class VulkanFramebuffer : public Framebuffer {
public:
    VulkanFramebuffer(VkDevice device, Ref<Texture> depthAttachment);
    ~VulkanFramebuffer() override;

    void Bind() override;
    void Unbind() override;
    Ref<Texture> GetDepthAttachment() const override { return m_DepthAttachment; }
    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }

    VkFramebuffer GetVkFramebuffer() const { return m_Framebuffer; }
    VkRenderPass GetVkRenderPass() const { return m_RenderPass; }

private:
    VkDevice m_Device;
    VkFramebuffer m_Framebuffer;
    VkRenderPass m_RenderPass;
    Ref<Texture> m_DepthAttachment;
    uint32 m_Width, m_Height;
};
```

**Depth Format Mapping** (VulkanTexture.cpp):
```cpp
VkFormat ToVulkanFormat(Format format) {
    switch (format) {
        // ... existing formats ...
        case Format::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case Format::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D16_UNORM: return VK_FORMAT_D16_UNORM;
        default: return VK_FORMAT_UNDEFINED;
    }
}
```

### Phase 2: Shadow Map System

**Duration**: 2-3 hours

#### Files to Create:
1. `include/metagfx/scene/ShadowMap.h` - Shadow map class declaration
2. `src/scene/ShadowMap.cpp` - Shadow map implementation
3. `src/app/shadowmap.vert` - Shadow map vertex shader
4. `src/app/shadowmap.frag` - Shadow map fragment shader (optional, depth-only)

#### Files to Modify:
1. `src/scene/CMakeLists.txt` - Add ShadowMap.cpp to build

#### Shadow Map Shaders:

**shadowmap.vert**:
```glsl
#version 450

// Uniform buffer with light space matrix
layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix;  // Light's view-projection matrix
} ubo;

// Push constants for model matrix
layout(push_constant) uniform PushConstants {
    mat4 model;
} pushConstants;

// Input vertex attributes
layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = ubo.lightSpaceMatrix * pushConstants.model * vec4(inPosition, 1.0);
}
```

**shadowmap.frag** (optional - can be empty for depth-only):
```glsl
#version 450

void main() {
    // Depth is written automatically
    // No fragment output needed for depth-only pass
}
```

#### ShadowMap Implementation:

**Constructor**:
```cpp
ShadowMap::ShadowMap(Ref<rhi::GraphicsDevice> device, uint32 width, uint32 height)
    : m_Device(device), m_Width(width), m_Height(height) {

    // Create depth texture
    rhi::TextureDesc depthDesc{};
    depthDesc.width = width;
    depthDesc.height = height;
    depthDesc.format = rhi::Format::D32_SFLOAT;
    depthDesc.usage = rhi::TextureUsage::DepthAttachment | rhi::TextureUsage::Sampled;
    m_DepthTexture = device->CreateTexture(depthDesc);

    // Create framebuffer
    m_Framebuffer = device->CreateFramebuffer(m_DepthTexture);
}
```

**Light Matrix Update**:
```cpp
void ShadowMap::UpdateLightMatrix(const glm::vec3& lightDir, const Camera& camera) {
    // Orthographic projection for directional light
    float orthoSize = 10.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize,
                                            -orthoSize, orthoSize,
                                            0.1f, 50.0f);

    // Light position along direction
    glm::vec3 lightPos = -glm::normalize(lightDir) * 20.0f;
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    m_LightSpaceMatrix = lightProjection * lightView;
}
```

### Phase 3: Shader Modifications

**Duration**: 2-3 hours

#### Files to Modify:
1. `src/app/model.vert` - Pass light space position to fragment shader
2. `src/app/model.frag` - Add shadow sampling with PCF

#### Vertex Shader Changes:

**model.vert additions**:
```glsl
// Add to UBO (binding 0)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 lightSpaceMatrix;  // NEW: Add after projection
} ubo;

// Add output variable
layout(location = 3) out vec4 fragPosLightSpace;  // NEW

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);

    // Existing outputs
    fragPosition = worldPos.xyz;
    fragNormal = mat3(ubo.model) * inNormal;
    fragTexCoord = inTexCoord;

    // NEW: Transform position to light space
    fragPosLightSpace = ubo.lightSpaceMatrix * worldPos;

    gl_Position = ubo.projection * ubo.view * worldPos;
}
```

#### Fragment Shader Changes:

**model.frag additions**:
```glsl
// Add shadow map binding
layout(binding = 12) uniform sampler2DShadow shadowMap;  // NEW: Binding 12

// Add input from vertex shader
layout(location = 3) in vec4 fragPosLightSpace;  // NEW

// Shadow calculation function with PCF
float CalculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0,1] range for texture sampling
    projCoords = projCoords * 0.5 + 0.5;

    // Check if fragment is outside shadow map bounds
    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;  // No shadow outside bounds
    }

    // Calculate bias to prevent shadow acne
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float currentDepth = projCoords.z - bias;

    // PCF (Percentage Closer Filtering) - 3x3 kernel
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(shadowMap, vec3(projCoords.xy + offset, currentDepth));
        }
    }
    shadow /= 9.0;  // Average of 9 samples

    return shadow;  // 0.0 = fully shadowed, 1.0 = no shadow
}

void main() {
    // ... existing PBR calculations (ambient, Lo, etc.) ...

    // Calculate shadow factor (only for directional lights)
    float shadow = 0.0;
    if (lightBuffer.lightCount > 0) {
        // Get first directional light
        LightData light = lightBuffer.lights[0];
        if (light.type == 0) {  // Directional light
            vec3 lightDir = normalize(-light.direction.xyz);
            shadow = CalculateShadow(fragPosLightSpace, N, lightDir);
        }
    }

    // Apply shadow only to direct lighting (not ambient or emissive)
    vec3 lighting = ambient + (1.0 - shadow) * Lo;

    // Add emissive (unaffected by shadows)
    lighting += emissive;

    // ... rest of shader (exposure, tone mapping, gamma) ...
}
```

### Phase 4: Application Integration

**Duration**: 3-4 hours

#### Files to Modify:
1. `src/app/Application.h` - Add shadow system members
2. `src/app/Application.cpp` - Implement shadow pass rendering

#### Application.h Changes:

```cpp
class Application {
private:
    // Shadow mapping system
    std::unique_ptr<ShadowMap> m_ShadowMap;
    Ref<rhi::Pipeline> m_ShadowPipeline;
    Ref<rhi::Buffer> m_ShadowUniformBuffer;       // For light space matrix
    Ref<rhi::Sampler> m_ShadowSampler;            // Comparison sampler

    // Shadow settings (configurable via ImGui)
    bool m_EnableShadows = true;
    uint32 m_ShadowMapSize = 2048;
    float m_ShadowBias = 0.005f;
    bool m_ShowShadowMap = false;  // Debug visualization

    // Shadow methods
    void CreateShadowResources();
    void CreateShadowPipeline();
    void RenderShadowPass(Ref<rhi::CommandBuffer> cmd);
    void RecreateShadowMap();

    // ... existing members ...
};
```

#### Application.cpp Implementation:

**CreateShadowResources()**:
```cpp
void Application::CreateShadowResources() {
    // Create shadow map
    m_ShadowMap = std::make_unique<ShadowMap>(m_Device, m_ShadowMapSize, m_ShadowMapSize);

    // Create shadow uniform buffer (light space matrix)
    m_ShadowUniformBuffer = m_Device->CreateBuffer({
        .size = sizeof(glm::mat4),
        .usage = rhi::BufferUsage::UniformBuffer,
        .memoryType = rhi::MemoryType::CPUToGPU
    });

    // Create comparison sampler for shadow mapping
    rhi::SamplerDesc samplerDesc{};
    samplerDesc.minFilter = rhi::Filter::Linear;
    samplerDesc.magFilter = rhi::Filter::Linear;
    samplerDesc.addressModeU = rhi::AddressMode::ClampToBorder;
    samplerDesc.addressModeV = rhi::AddressMode::ClampToBorder;
    samplerDesc.addressModeW = rhi::AddressMode::ClampToBorder;
    samplerDesc.borderColor = rhi::BorderColor::OpaqueWhite;  // Outside shadow = no shadow
    samplerDesc.enableCompare = true;
    samplerDesc.compareOp = rhi::CompareOp::LessOrEqual;
    m_ShadowSampler = m_Device->CreateSampler(samplerDesc);
}
```

**RenderShadowPass()**:
```cpp
void Application::RenderShadowPass(Ref<rhi::CommandBuffer> cmd) {
    if (!m_EnableShadows || !m_ShadowMap || !m_Model) {
        return;
    }

    // Update light space matrix
    if (m_LightCount > 0) {
        // Get first directional light
        Light* light = nullptr;
        for (uint32 i = 0; i < m_LightCount; ++i) {
            if (m_Lights[i]->GetType() == LightType::Directional) {
                light = m_Lights[i].get();
                break;
            }
        }

        if (light) {
            m_ShadowMap->UpdateLightMatrix(
                light->GetDirection(),
                *m_Camera
            );

            // Update shadow uniform buffer
            glm::mat4 lightSpace = m_ShadowMap->GetLightSpaceMatrix();
            m_ShadowUniformBuffer->CopyData(&lightSpace, sizeof(glm::mat4));
        }
    }

    // Begin shadow rendering (depth-only pass)
    cmd->BeginRendering(
        {},  // No color attachments
        m_ShadowMap->GetDepthTexture(),
        {},  // No clear color
        1.0f // Clear depth to 1.0
    );

    // Set viewport and scissor to shadow map size
    rhi::Viewport viewport{0, 0, m_ShadowMapSize, m_ShadowMapSize, 0.0f, 1.0f};
    cmd->SetViewport(viewport);

    rhi::Rect2D scissor{0, 0, m_ShadowMapSize, m_ShadowMapSize};
    cmd->SetScissor(scissor);

    // Bind shadow pipeline
    cmd->BindPipeline(m_ShadowPipeline);

    // Bind shadow descriptor set (light space matrix)
    // TODO: Bind descriptor set with shadow uniform buffer

    // Render all meshes from light's perspective
    for (const auto& mesh : m_Model->GetMeshes()) {
        if (!mesh || !mesh->IsValid()) continue;

        // Push model matrix as push constant
        // TODO: Push model matrix

        cmd->BindVertexBuffer(mesh->GetVertexBuffer());
        cmd->BindIndexBuffer(mesh->GetIndexBuffer());
        cmd->DrawIndexed(mesh->GetIndexCount());
    }

    cmd->EndRendering();
}
```

**Render() Integration**:
```cpp
void Application::Render() {
    // ... acquire frame, begin command buffer ...

    // 1. SHADOW PASS (NEW)
    RenderShadowPass(cmd);

    // 2. MAIN PASS (with shadow sampling)
    // Update uniform buffer with light space matrix
    // ... rest of existing rendering ...

    // ... present, end command buffer ...
}
```

### Phase 5: Descriptor Set Extension

**Duration**: 1-2 hours

#### Changes Required:

**Expand Descriptor Set from 12 to 13 bindings**:

| Binding | Type | Description |
|---------|------|-------------|
| 0 | Uniform Buffer | MVP matrices + **light space matrix** |
| 1 | Uniform Buffer | Material properties |
| 2-11 | Texture Samplers | PBR textures + IBL |
| **12** | **Sampler2DShadow** | **Shadow map (NEW)** |

#### Update UniformBufferObject:

```cpp
// Update UBO structure (binding 0)
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 lightSpaceMatrix;  // NEW: Add 64 bytes
};  // Total: 256 bytes
```

#### Add Shadow Map to Descriptor Set:

```cpp
// In CreateModelPipeline() or similar
std::vector<DescriptorBinding> bindings = {
    // ... existing 12 bindings (0-11) ...

    // NEW: Binding 12 - Shadow map
    {
        12,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr,  // No buffer
        m_ShadowMap->GetDepthTexture(),
        m_ShadowSampler
    }
};
```

### Phase 6: ImGui Debug Controls

**Duration**: 1 hour

#### UI Additions:

```cpp
void Application::RenderUI() {
    // ... existing ImGui code ...

    ImGui::Separator();
    ImGui::Text("Shadow Mapping");

    ImGui::Checkbox("Enable Shadows", &m_EnableShadows);

    if (m_EnableShadows) {
        // Shadow map resolution
        const char* sizes[] = {"512", "1024", "2048", "4096"};
        static int sizeIdx = 2;  // Default: 2048
        if (ImGui::Combo("Shadow Map Size", &sizeIdx, sizes, 4)) {
            m_ShadowMapSize = 512 << sizeIdx;
            RecreateShadowMap();
        }

        // Shadow bias control
        ImGui::SliderFloat("Shadow Bias", &m_ShadowBias, 0.0f, 0.01f, "%.4f");

        // Debug visualization
        ImGui::Checkbox("Show Shadow Map", &m_ShowShadowMap);
        if (m_ShowShadowMap && m_ShadowMap) {
            ImGui::Text("Shadow Map Debug:");
            ImTextureID texID = (ImTextureID)(intptr_t)m_ShadowMap->GetDepthTexture()->GetNativeHandle();
            ImGui::Image(texID, ImVec2(256, 256));
        }
    }
}
```

---

## Technical Considerations

### 1. Depth Format Selection

**Recommended**: `Format::D32_SFLOAT`
- 32-bit float depth
- Best precision for shadow mapping
- Widely supported on modern GPUs
- No stencil component (not needed)

**Trade-offs**:
- 512x512: 1 MB
- 1024x1024: 4 MB
- 2048x2048: 16 MB
- 4096x4096: 64 MB

### 2. Shadow Map Resolution

**Recommended Default**: 2048x2048

**Quality Levels**:
- 512x512: Low quality, visible pixelation, fast
- 1024x1024: Acceptable quality, good performance
- 2048x2048: Good quality (recommended)
- 4096x4096: High quality, performance impact

### 3. PCF Kernel Size

**3x3 Kernel** (9 samples):
- Good balance between quality and performance
- Minimal cost (~5-10% fragment shader overhead)
- Produces reasonably soft shadow edges

**Alternative**:
- 5x5 (25 samples): Better soft shadows, higher cost
- 7x7 (49 samples): Very soft, significant performance impact

### 4. Shadow Bias

**Purpose**: Prevent shadow acne (self-shadowing artifacts)

**Formula**:
```glsl
float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
```

**Trade-off**: Too much bias causes "Peter Panning" (shadows detach from objects)

**Configurable Range**: 0.0 - 0.01 (via ImGui slider)

### 5. Sampler Border Color

**Border Color**: `OpaqueWhite`
- Regions outside shadow map = no shadow (full light)
- Prevents black artifacts at shadow map edges

### 6. Performance Impact

**Estimated Costs**:
- Shadow pass: +30-50% frame time
- PCF filtering (3x3): +5-10% fragment shader cost
- Memory: 2048x2048 * 4 bytes = 16 MB
- Total impact: ~40-60% frame time increase

**Target**: Maintain >60 FPS with 2048x2048 shadow map on mid-range GPU

---

## Limitations & Future Work

### Current Scope (Milestone 3.3)
✅ Single directional light shadow
✅ PCF soft shadows (3x3 kernel)
✅ Basic shadow bias
✅ Configurable shadow map resolution
✅ ImGui debug controls

### Future Enhancements (Beyond 3.3)
❌ Cascade Shadow Maps (CSM) for large outdoor scenes
❌ Point light shadows (6 shadow maps per light)
❌ Spot light shadows (single shadow map per light)
❌ PCSS (Percentage Closer Soft Shadows)
❌ Shadow map pooling for multiple lights
❌ Variance Shadow Maps (VSM)
❌ Exponential Shadow Maps (ESM)

---

## Testing Strategy

### Test Cases

1. **Basic Shadow Casting**:
   - Single directional light
   - Simple cube casting shadow on plane
   - Verify shadow appears and moves with light

2. **Shadow Quality**:
   - Test resolutions: 512, 1024, 2048, 4096
   - Verify PCF smoothing
   - Check for shadow acne

3. **Shadow Bias**:
   - Adjust bias slider (0.0 - 0.01)
   - Verify no shadow acne at low angles
   - Verify no Peter Panning at high bias

4. **Complex Scenes**:
   - DamagedHelmet model with self-shadowing
   - Multiple objects casting overlapping shadows
   - Performance remains >60 FPS at 2048x2048

5. **Edge Cases**:
   - Objects outside shadow map bounds (no shadow)
   - Camera looking away from light
   - Very close/far objects

### Debug Visualizations

1. **Shadow Map Viewer**: Display depth texture in ImGui (256x256 preview)
2. **Shadow-Only Mode**: Render only shadow contribution (black/white)
3. **Shadow Frustum**: Visualize light's orthographic frustum (optional)

---

## File Manifest

### New Files (15)

**Renderer Architecture** (NEW):
1. `include/metagfx/renderer/Renderer.h`
2. `include/metagfx/renderer/RasterizationRenderer.h`
3. `src/renderer/RasterizationRenderer.cpp`
4. `src/renderer/CMakeLists.txt`

**RHI Abstractions**:
5. `include/metagfx/rhi/Framebuffer.h`
6. `src/rhi/vulkan/VulkanFramebuffer.h`
7. `src/rhi/vulkan/VulkanFramebuffer.cpp`

**Shadow System**:
8. `include/metagfx/scene/ShadowMap.h`
9. `src/scene/ShadowMap.cpp`

**Shaders**:
10. `src/app/shadowmap.vert`
11. `src/app/shadowmap.frag`
12. `src/app/shadowmap.vert.spv` (compiled)
13. `src/app/shadowmap.frag.spv` (compiled)
14. `src/app/shadowmap.vert.spv.inl` (C++ include)
15. `src/app/shadowmap.frag.spv.inl` (C++ include)

**Documentation**:
16. `docs/shadow_mapping.md`
17. `docs/renderer_architecture.md` (NEW)

### Modified Files (15)

**RHI**:
1. `include/metagfx/rhi/Types.h` - Depth formats, compare ops
2. `include/metagfx/rhi/Texture.h` - Depth texture support
3. `include/metagfx/rhi/Sampler.h` - Comparison mode
4. `include/metagfx/rhi/GraphicsDevice.h` - CreateFramebuffer()
5. `src/rhi/vulkan/VulkanTexture.cpp` - Depth texture creation
6. `src/rhi/vulkan/VulkanSampler.cpp` - Comparison sampler
7. `src/rhi/vulkan/VulkanDevice.cpp` - Framebuffer creation

**Application**:
8. `src/app/Application.h` - Renderer abstraction, mode switching
9. `src/app/Application.cpp` - Delegate rendering to RasterizationRenderer

**Shaders**:
10. `src/app/model.vert` - Light space transformation
11. `src/app/model.frag` - Shadow sampling + PCF

**Build**:
12. `CMakeLists.txt` (root) - Add renderer subdirectory
13. `src/scene/CMakeLists.txt` - Add ShadowMap.cpp
14. `src/renderer/CMakeLists.txt` - Renderer module (NEW)
15. `src/rhi/vulkan/CMakeLists.txt` - Add VulkanFramebuffer.cpp

---

## Success Criteria

Milestone 3.3 is complete when:

**Renderer Architecture**:
✅ Abstract `Renderer` base class implemented
✅ `RasterizationRenderer` class successfully encapsulates rendering logic
✅ Application delegates rendering to renderer abstraction
✅ Foundation ready for future hybrid and path tracing modes

**Shadow Mapping**:
✅ Shadow map texture created with configurable resolution
✅ Depth-only shadow pass renders correctly
✅ Main pass samples shadow map with PCF filtering
✅ Shadows appear on surfaces and move with light direction
✅ No visible shadow acne or major artifacts
✅ Performance remains >60 FPS with 2048x2048 shadow map

**User Interface**:
✅ ImGui controls work: enable/disable, resolution, bias
✅ Shadow map debug visualization works
✅ (Future) Rendering mode selector (rasterization/hybrid/path tracing)

**Documentation**:
✅ `docs/shadow_mapping.md` complete
✅ `docs/renderer_architecture.md` complete (NEW)
✅ `CLAUDE.md` updated with renderer architecture

**Testing**:
✅ Works correctly with DamagedHelmet and other glTF models
✅ Rendering logic unchanged from user perspective (same visual output)
✅ Code is cleaner and more maintainable

---

## Estimated Timeline

| Phase | Task | Duration |
|-------|------|----------|
| **0** | **Renderer Architecture (NEW)** | **2-3 hours** |
| | - Create Renderer base class, RenderMode enum | |
| | - Create RasterizationRenderer implementation | |
| | - Migrate Application rendering logic to renderer | |
| 1 | RHI Extensions (Framebuffer, depth formats, comparison samplers) | 2-3 hours |
| 2 | Shadow Map System (class, shaders, matrix calculation) | 2-3 hours |
| 3 | Shader Modifications (vertex + fragment, PCF) | 2-3 hours |
| 4 | Application Integration (shadow pass, render loop) | 3-4 hours |
| 5 | Descriptor Set Extension (binding 12, UBO update) | 1-2 hours |
| 6 | ImGui Debug Controls (UI, visualization) | 1 hour |
| **Subtotal** | **Development** | **13-19 hours** |
| Testing | Test cases, debugging, quality assurance | 4-6 hours |
| Documentation | `shadow_mapping.md`, `renderer_architecture.md`, update CLAUDE.md | 3-4 hours |
| **Total** | **Complete Implementation** | **20-29 hours** |

---

## Next Steps

1. **Get User Approval**: Review this plan and confirm approach
2. **Begin Phase 1**: RHI extensions (foundational work)
3. **Incremental Testing**: Test after each phase
4. **Iterate on Quality**: Tune bias, PCF kernel, resolution
5. **Document Lessons**: Capture insights for future milestones

---

**Ready to proceed with implementation when approved!**
