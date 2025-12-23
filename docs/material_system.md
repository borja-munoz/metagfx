# Material System Design

**Milestone**: 2.2 - Basic Material System
**Status**: ✅ Implemented
**Last Updated**: December 23, 2024

## Overview

The Material System provides per-mesh material properties (albedo color, roughness, metallic) and implements Blinn-Phong lighting with ambient, diffuse, and specular components. This system forms the foundation for physically-based rendering (PBR) in later milestones.

### Key Features

- **Per-Mesh Materials**: Each mesh owns its material with unique properties
- **GPU-Compatible Layout**: std140-compliant uniform buffer layout
- **Blinn-Phong Lighting**: Full lighting model with ambient, diffuse, and specular
- **Assimp Integration**: Automatic material extraction from model files
- **Roughness-Based Shininess**: Intuitive roughness parameter controls specular highlight tightness
- **Extensible Design**: Ready for texture mapping and PBR in future milestones

---

## Architecture

### 1. Material Class

The `Material` class encapsulates surface properties on the CPU side:

```cpp
// GPU-side structure (std140 layout, 32 bytes)
struct MaterialProperties {
    glm::vec3 albedo;      // 12 bytes (offset 0)  - Base color
    float roughness;       // 4 bytes  (offset 12) - Surface roughness [0, 1]
    float metallic;        // 4 bytes  (offset 16) - Metallic factor [0, 1]
    float padding[3];      // 12 bytes (offset 20) - Align to 32 bytes
};

// CPU-side class
class Material {
public:
    Material(const glm::vec3& albedo = glm::vec3(0.8f),
             float roughness = 0.5f,
             float metallic = 0.0f);

    void SetAlbedo(const glm::vec3& albedo);
    void SetRoughness(float roughness);
    void SetMetallic(float metallic);

    const MaterialProperties& GetProperties() const;

private:
    MaterialProperties m_Properties;
};
```

**Design Decisions**:

- **std140 Layout Compliance**: MaterialProperties uses explicit padding to ensure 32-byte alignment for efficient GPU access
- **Value-Based Properties**: Simple float/vec3 properties (no texture support yet - deferred to Milestone 2.3)
- **Parameter Validation**: Roughness and metallic are clamped to [0, 1] range
- **Immutable Defaults**: Default material provides sensible fallback values

### 2. Mesh-Material Ownership

Each `Mesh` owns its `Material` via `std::unique_ptr`:

```cpp
class Mesh {
    // ... existing members ...
    std::unique_ptr<Material> m_Material;

public:
    void SetMaterial(std::unique_ptr<Material> material);
    Material* GetMaterial() const { return m_Material.get(); }
};
```

**Ownership Model**:
- **Unique Ownership**: Each mesh exclusively owns its material (unique_ptr)
- **Default Materials**: Created automatically if none provided
- **Move Semantics**: Efficient material transfer during mesh initialization
- **No Sharing**: Materials are not shared between meshes (allows per-mesh customization)

---

## GPU Integration

### Descriptor Set Strategy

The material system extends the descriptor set layout from 1 binding to 2 bindings:

```
Descriptor Set Layout:
┌─────────────┬──────────────────┬────────────┬─────────────┐
│ Binding     │ Type             │ Size       │ Update Rate │
├─────────────┼──────────────────┼────────────┼─────────────┤
│ 0           │ Uniform Buffer   │ 192 bytes  │ Per-frame   │
│             │ (MVP matrices)   │ (3×mat4)   │ (camera)    │
├─────────────┼──────────────────┼────────────┼─────────────┤
│ 1           │ Uniform Buffer   │ 32 bytes   │ Per-mesh    │
│             │ (Material props) │            │ (material)  │
└─────────────┴──────────────────┴────────────┴─────────────┘
```

**Implementation Details**:

```cpp
// Application.cpp - CreateModelPipeline()
std::vector<rhi::DescriptorBinding> bindings = {
    { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,   m_UniformBuffers[0] },
    { 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, m_MaterialBuffers[0] }
};

m_DescriptorSet = m_Device->CreateDescriptorSet(bindings);
```

**Buffer Update Strategy**:
- **MVP Buffer**: Updated once per frame (view/projection changes)
- **Material Buffer**: Updated per mesh (different materials)
- **Double Buffering**: Both buffers are double-buffered for frame overlap
- **CopyData Only**: Update buffer contents via `CopyData()`, not descriptor bindings

### Critical Bug Fix: Vector Pointer Invalidation

During implementation, we discovered a critical bug in `VulkanDescriptorSet::UpdateSets()`:

```cpp
// BEFORE (buggy):
void VulkanDescriptorSet::UpdateSets(const std::vector<DescriptorBinding>& bindings) {
    for (uint32 i = 0; i < MAX_FRAMES; i++) {
        std::vector<VkDescriptorBufferInfo> bufferInfos;

        for (const auto& binding : bindings) {
            VkDescriptorBufferInfo bufferInfo{...};
            bufferInfos.push_back(bufferInfo);

            VkWriteDescriptorSet descriptorWrite{...};
            descriptorWrite.pBufferInfo = &bufferInfos.back();  // ❌ DANGEROUS!
            descriptorWrites.push_back(descriptorWrite);
        }
        // Vector reallocation invalidates all pointers stored in descriptorWrites!
    }
}

// AFTER (fixed):
void VulkanDescriptorSet::UpdateSets(const std::vector<DescriptorBinding>& bindings) {
    for (uint32 i = 0; i < MAX_FRAMES; i++) {
        std::vector<VkDescriptorBufferInfo> bufferInfos;

        // ✅ Reserve space to prevent reallocation
        bufferInfos.reserve(bindings.size());
        descriptorWrites.reserve(bindings.size());

        for (const auto& binding : bindings) {
            // ... same code, but now pointers are stable
        }
    }
}
```

**Root Cause**: When adding the 2nd descriptor binding, the `bufferInfos` vector reallocated, invalidating all pointers stored in `descriptorWrites.pBufferInfo`.

**Solution**: Pre-allocate capacity with `reserve()` to prevent reallocation during population.

**Impact**: This bug only manifested with 2+ bindings. Single-binding configurations (Milestone 2.1) worked fine.

---

## Blinn-Phong Lighting Model

### Shader Implementation

The fragment shader implements a complete Blinn-Phong lighting model:

```glsl
#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Material uniform
layout(binding = 1) uniform MaterialUBO {
    vec3 albedo;
    float roughness;
    float metallic;
} material;

// Push constants for camera position
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;
} pushConstants;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional light
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    vec3 objectColor = material.albedo;
    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(pushConstants.cameraPosition.xyz - fragPosition);

    // 1. Ambient: Global illumination approximation
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // 2. Diffuse: Lambertian reflectance
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // 3. Specular: Blinn-Phong highlights
    vec3 halfDir = normalize(lightDir + viewDir);
    float shininess = mix(256.0, 16.0, material.roughness);
    float spec = pow(max(dot(norm, halfDir), 0.0), shininess);

    float specularStrength = 0.5;
    vec3 specular = specularStrength * spec * lightColor;

    // Combine: (ambient + diffuse) * albedo + specular
    vec3 result = (ambient + diffuse) * objectColor + specular;

    outColor = vec4(result, 1.0);
}
```

### Lighting Components

**1. Ambient Lighting**
```glsl
vec3 ambient = ambientStrength * lightColor;
```
- Constant base illumination (0.1 strength)
- Simulates global indirect lighting
- Prevents completely black surfaces in shadow

**2. Diffuse Lighting (Lambertian)**
```glsl
float diff = max(dot(norm, lightDir), 0.0);
vec3 diffuse = diff * lightColor;
```
- Surface brightness based on angle to light
- `dot(normal, lightDir)` measures alignment
- `max(..., 0.0)` clamps to prevent negative values
- Creates characteristic "matte" appearance

**3. Specular Lighting (Blinn-Phong)**
```glsl
vec3 halfDir = normalize(lightDir + viewDir);
float shininess = mix(256.0, 16.0, material.roughness);
float spec = pow(max(dot(norm, halfDir), 0.0), shininess);
vec3 specular = specularStrength * spec * lightColor;
```

**Key Differences from Phong**:
- **Phong**: Uses reflection vector `reflect(-lightDir, norm)`
- **Blinn-Phong**: Uses half-vector `normalize(lightDir + viewDir)`
- **Advantage**: Half-vector is cheaper to compute and more physically plausible
- **Visual**: Nearly identical results for most viewing angles

**Roughness to Shininess Mapping**:
```glsl
float shininess = mix(256.0, 16.0, material.roughness);
```
- **Roughness 0.0** (smooth) → **Shininess 256** → Tight, bright highlights
- **Roughness 0.5** (medium) → **Shininess 136** → Medium highlights
- **Roughness 1.0** (rough)  → **Shininess 16**  → Wide, dim highlights
- **Inverse Relationship**: Higher roughness = lower shininess

**Final Combination**:
```glsl
vec3 result = (ambient + diffuse) * objectColor + specular;
```
- Ambient and diffuse are **modulated** by albedo (colored)
- Specular is **additive** (typically white highlights)
- Matches physical behavior of light reflection

### Push Constants for Camera Position

Camera position is passed via push constants for efficient per-frame updates:

```cpp
// VulkanPipeline.cpp - Pipeline Layout
VkPushConstantRange pushConstantRange{};
pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
pushConstantRange.offset = 0;
pushConstantRange.size = 16;  // vec4 (16 bytes, using only xyz)

// Application.cpp - Render Loop
glm::vec4 cameraPos(m_Camera->GetPosition(), 1.0f);
vkCmd->PushConstants(vkPipeline->GetLayout(),
                    VK_SHADER_STAGE_FRAGMENT_BIT,
                    0, sizeof(glm::vec4), &cameraPos);
```

**Why Push Constants?**:
- **Performance**: Updated once per frame, not per mesh
- **Alignment**: vec4 ensures proper 16-byte alignment
- **Simplicity**: No separate uniform buffer needed
- **Efficiency**: Push constants are stored in command buffer, very fast access

---

## Assimp Material Extraction

Materials are automatically extracted from model files during loading:

```cpp
static std::unique_ptr<Material> ProcessMaterial(const aiMaterial* aiMat) {
    // Extract diffuse color
    aiColor3D diffuse(0.8f, 0.8f, 0.8f);
    aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);

    // Extract shininess and convert to roughness
    float shininess = 32.0f;
    aiMat->Get(AI_MATKEY_SHININESS, shininess);

    // Convert shininess to roughness (inverse relationship)
    // Shininess typically ranges from 0-256 in Blinn-Phong
    float roughness = 1.0f - glm::clamp(shininess / 256.0f, 0.0f, 1.0f);

    // Default metallic to 0.0 (not available in basic formats like OBJ)
    float metallic = 0.0f;

    return std::make_unique<Material>(
        glm::vec3(diffuse.r, diffuse.g, diffuse.b),
        roughness,
        metallic
    );
}
```

**Extraction Strategy**:

| Assimp Property | Material Property | Conversion |
|-----------------|-------------------|------------|
| `AI_MATKEY_COLOR_DIFFUSE` | `albedo` | Direct mapping (RGB) |
| `AI_MATKEY_SHININESS` | `roughness` | `1.0 - (shininess / 256.0)` |
| N/A (not in OBJ/FBX) | `metallic` | Default to 0.0 |

**Fallback Strategy**:
```cpp
// In ProcessMesh():
if (scene && aiMesh->mMaterialIndex >= 0) {
    aiMaterial* aiMat = scene->mMaterials[aiMesh->mMaterialIndex];
    auto material = ProcessMaterial(aiMat);
    mesh->SetMaterial(std::move(material));
} else {
    // Fallback to default material
    mesh->SetMaterial(std::make_unique<Material>());
}
```

**Supported Formats**:
- **OBJ**: Diffuse color, shininess from MTL files
- **FBX**: Full material properties including shininess
- **glTF**: Metallic-roughness workflow (conversion needed)
- **COLLADA**: Phong materials with diffuse and shininess

---

## Render Loop Integration

The render loop updates material buffers per-mesh before drawing:

```cpp
// Application.cpp - Render()
for (const auto& mesh : m_Model->GetMeshes()) {
    if (mesh && mesh->IsValid() && mesh->GetMaterial()) {
        // Update material buffer for this mesh
        MaterialProperties matProps = mesh->GetMaterial()->GetProperties();
        m_MaterialBuffers[0]->CopyData(&matProps, sizeof(matProps));

        // Bind and draw
        cmd->BindVertexBuffer(mesh->GetVertexBuffer());
        cmd->BindIndexBuffer(mesh->GetIndexBuffer());
        cmd->DrawIndexed(mesh->GetIndexCount());
    }
}
```

**Performance Considerations**:

1. **Per-Mesh Buffer Update**: Each mesh may have different material
2. **Small Copy**: Only 32 bytes per material (very fast)
3. **No Descriptor Update**: Only buffer contents change, not bindings
4. **Future Optimization**: Could use push constants for material data (128-byte limit)

---

## Backface Culling Fix

During implementation, we also fixed backface culling issues related to Vulkan's coordinate system:

**Problem**: Vulkan uses inverted Y-axis compared to OpenGL, affecting winding order.

**Solution in Camera.cpp**:
```cpp
// SetPerspective()
m_ProjectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);

// GLM was designed for OpenGL - flip Y for Vulkan
m_ProjectionMatrix[1][1] *= -1;
```

**Pipeline Configuration**:
```cpp
// Application.cpp - CreateModelPipeline()
pipelineDesc.rasterization.cullMode = CullMode::Back;
pipelineDesc.rasterization.frontFace = FrontFace::Clockwise;  // Account for Y-flip
```

**Explanation**:
- Y-flip in projection reverses triangle winding from CCW to CW
- Setting `FrontFace::Clockwise` compensates for this
- Back faces are correctly culled, improving performance

---

## Default Materials

Procedural geometry receives sensible default materials:

```cpp
// Model::CreateCube()
auto material = std::make_unique<Material>(
    glm::vec3(1.0f, 1.0f, 1.0f),  // White albedo
    0.3f,                          // Low roughness (shiny)
    0.0f                           // Non-metallic
);
mesh->SetMaterial(std::move(material));

// Model::CreateSphere()
auto material = std::make_unique<Material>(
    glm::vec3(0.8f, 0.8f, 0.8f),  // Light gray albedo
    0.5f,                          // Medium roughness
    0.0f                           // Non-metallic
);
mesh->SetMaterial(std::move(material));
```

---

## Usage Examples

### Creating a Custom Material

```cpp
// Shiny gold material
auto goldMaterial = std::make_unique<Material>(
    glm::vec3(1.0f, 0.84f, 0.0f),  // Gold color
    0.1f,                           // Very shiny
    1.0f                            // Fully metallic
);
mesh->SetMaterial(std::move(goldMaterial));

// Rough plastic material
auto plasticMaterial = std::make_unique<Material>(
    glm::vec3(0.2f, 0.5f, 0.8f),   // Blue plastic
    0.8f,                           // Very rough
    0.0f                            // Non-metallic
);
mesh->SetMaterial(std::move(plasticMaterial));
```

### Loading Materials from Model Files

```cpp
// Materials are automatically extracted
auto model = Model::LoadFromFile(device, "models/bunny.obj");

// Access materials
for (const auto& mesh : model->GetMeshes()) {
    Material* mat = mesh->GetMaterial();
    METAGFX_INFO << "Material albedo: "
                 << mat->GetProperties().albedo.x << ", "
                 << mat->GetProperties().albedo.y << ", "
                 << mat->GetProperties().albedo.z;
}
```

### Runtime Material Updates

```cpp
// Modify material at runtime
Material* mat = mesh->GetMaterial();
mat->SetAlbedo(glm::vec3(1.0f, 0.0f, 0.0f));  // Change to red
mat->SetRoughness(0.2f);                       // Make shinier
```

---

## File Manifest

### New Files (2)

- `include/metagfx/scene/Material.h` - Material class interface
- `src/scene/Material.cpp` - Material class implementation

### Modified Files (9)

- `include/metagfx/scene/Mesh.h` - Added material member
- `src/scene/Mesh.cpp` - Material ownership and initialization
- `include/metagfx/scene/Model.h` - Material extraction declaration
- `src/scene/Model.cpp` - Assimp material extraction, procedural materials
- `src/app/Application.h` - Material uniform buffers
- `src/app/Application.cpp` - Descriptor sets, push constants, render loop
- `src/app/model.frag` - Material uniform, Blinn-Phong lighting
- `src/rhi/vulkan/VulkanDescriptorSet.cpp` - **Critical bug fix** (reserve())
- `src/rhi/vulkan/VulkanPipeline.cpp` - Push constant range

### Build Configuration

- `src/scene/CMakeLists.txt` - Added Material.cpp to sources

---

## Technical Challenges & Solutions

### Challenge 1: Descriptor Set Performance

**Issue**: Updating descriptor bindings per-mesh could have performance overhead.

**Solution**: Only update buffer contents (`CopyData()`), not descriptor bindings. Descriptor set points to the same buffer, only buffer data changes.

**Future Optimization**: Consider push constants for material data (limited to 128 bytes in Vulkan).

### Challenge 2: std140 Alignment

**Issue**: GPU may reject misaligned uniform buffers.

**Solution**: Explicitly pad `MaterialProperties` to 32 bytes with `float padding[3]`. Verified with Vulkan validation layers.

### Challenge 3: Shininess to Roughness Mapping

**Issue**: Different tools use different shininess ranges (0-128, 0-256, 0-1000).

**Solution**: Normalize to 0-256 range with clamping: `1.0 - clamp(shininess / 256.0, 0.0, 1.0)`.

### Challenge 4: Missing Material Data

**Issue**: Some model formats (simple OBJ) may not have material properties.

**Solution**: Always provide fallback default material if extraction fails.

### Challenge 5: Vector Pointer Invalidation

**Issue**: Rendering failed with 2+ descriptor bindings due to vector reallocation.

**Solution**: Pre-allocate vector capacity with `reserve()` before populating.

**Discovery**: Systematic debugging revealed the issue only appeared with multiple bindings.

---

## Performance Characteristics

### Memory Usage

| Component | Size per Mesh | Notes |
|-----------|---------------|-------|
| Material CPU | 32 bytes | MaterialProperties struct |
| Material GPU | 32 bytes | Uniform buffer (double-buffered: 64 bytes total) |
| Descriptor Set | ~200 bytes | Vulkan internal overhead |

**Total**: ~300 bytes per mesh for material system.

### GPU Costs

- **Descriptor Binding**: 1 descriptor set bind per frame
- **Material Updates**: ~30 bytes copy per mesh per frame
- **Push Constants**: 16 bytes per frame (camera position)
- **Shader Complexity**: +15 instructions for Blinn-Phong (minimal impact)

**Conclusion**: Material system adds negligible overhead to rendering.

---

## Future Enhancements

### Milestone 2.3: Texture Maps

Replace constant albedo with texture sampling:

```glsl
// Future shader code
layout(binding = 2) uniform sampler2D albedoMap;

void main() {
    vec3 albedo = texture(albedoMap, fragTexCoord).rgb;
    // ... rest of lighting
}
```

**Required Changes**:
- Add texture samplers to material
- Extend descriptor set to include texture bindings
- Update shader to sample textures

### Phase 3: Physically-Based Rendering (PBR)

Replace Blinn-Phong with PBR:

```glsl
// Future: Cook-Torrance BRDF
float D = DistributionGGX(N, H, roughness);
float G = GeometrySmith(N, V, L, roughness);
vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

vec3 specular = (D * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0));
```

**Required Changes**:
- Implement microfacet BRDF (GGX normal distribution, Smith geometry, Fresnel-Schlick)
- Use metallic workflow (base color + metallic + roughness)
- Add image-based lighting (environment maps)

### Phase 4: Material Library System

Share materials across multiple meshes:

```cpp
class MaterialLibrary {
    std::unordered_map<std::string, std::shared_ptr<Material>> m_Materials;

public:
    std::shared_ptr<Material> GetMaterial(const std::string& name);
    void RegisterMaterial(const std::string& name, std::shared_ptr<Material> mat);
};
```

**Benefits**:
- Memory savings (shared materials)
- Centralized material management
- Hot-reload support

---

## Debugging Tips

### Visualize Material Properties

Add debug visualization to fragment shader:

```glsl
// Debug: Show roughness as grayscale
outColor = vec4(vec3(material.roughness), 1.0);

// Debug: Show normals as RGB
outColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);

// Debug: Show albedo only (no lighting)
outColor = vec4(material.albedo, 1.0);
```

### Validate Descriptor Updates

Enable Vulkan validation layers to catch descriptor update issues:

```bash
export VK_LAYER_PATH=/path/to/vulkan/sdk/etc/vulkan/explicit_layer.d
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
```

### Profile Material Updates

Add timing to measure material buffer update cost:

```cpp
auto start = std::chrono::high_resolution_clock::now();
m_MaterialBuffers[0]->CopyData(&matProps, sizeof(matProps));
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
METAGFX_INFO << "Material update: " << duration.count() << "μs";
```

---

## References

### Internal Documentation

- [Model Loading System](model_loading.md) - Mesh and Model architecture
- [RHI Design](rhi.md) - Buffer and descriptor set abstractions
- [Vulkan Implementation](vulkan.md) - Vulkan backend specifics
- [Camera System](camera_transformation_system.md) - Camera and projection matrices

### External Resources

- [Learn OpenGL - Basic Lighting](https://learnopengl.com/Lighting/Basic-Lighting)
- [Learn OpenGL - Materials](https://learnopengl.com/Lighting/Materials)
- [Blinn-Phong Reflection Model](https://en.wikipedia.org/wiki/Blinn%E2%80%93Phong_reflection_model)
- [Assimp Material System](https://assimp-docs.readthedocs.io/en/latest/usage/use_the_lib.html#materials)
- [Vulkan Push Constants](https://vkguide.dev/docs/chapter-3/push_constants/)

### Academic Papers

- Blinn, J. F. (1977). "Models of light reflection for computer synthesized pictures"
- Phong, B. T. (1975). "Illumination for computer generated pictures"

---

## Glossary

| Term | Definition |
|------|------------|
| **Albedo** | Base color of a surface, independent of lighting |
| **Roughness** | Surface irregularity (0 = smooth/shiny, 1 = rough/matte) |
| **Metallic** | Whether surface is metallic (0 = dielectric, 1 = metal) |
| **Blinn-Phong** | Lighting model using half-vector for specular highlights |
| **std140** | GLSL uniform buffer layout standard with specific alignment rules |
| **Push Constants** | Small amount of data pushed directly into command buffer |
| **Descriptor Set** | Vulkan binding mechanism for shader resources (buffers, textures) |
| **Shininess** | Exponent controlling specular highlight tightness (legacy term) |

---

## Changelog

### December 23, 2024 - Initial Implementation

- ✅ Material class with albedo, roughness, metallic
- ✅ Per-mesh material ownership
- ✅ Blinn-Phong lighting (ambient + diffuse + specular)
- ✅ Assimp material extraction (diffuse → albedo, shininess → roughness)
- ✅ Push constants for camera position
- ✅ Descriptor set extension (2 bindings)
- ✅ Backface culling fix (Clockwise front face for Y-flipped projection)
- ✅ **Critical bug fix**: Vector pointer invalidation in descriptor set updates

---

**Status**: ✅ Milestone 2.2 Complete
**Next**: Milestone 2.3 - Textures and Samplers
