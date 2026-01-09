# Textures and Samplers

**Status**: ✅ Implemented (Milestone 2.3, extended with IBL and Emissive)
**Last Updated**: January 9, 2026

This document describes the texture and sampler system in MetaGFX, enabling models to be rendered with albedo/diffuse texture maps.

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Core Components](#core-components)
4. [Texture Loading](#texture-loading)
5. [Material Integration](#material-integration)
6. [Shader Integration](#shader-integration)
7. [Usage Examples](#usage-examples)
8. [Troubleshooting](#troubleshooting)
9. [Future Extensions](#future-extensions)

## Overview

The texture system provides:
- **Albedo/diffuse texture support** for materials
- **Emissive texture support** for self-illuminating materials (January 2026)
- **Shared sampler strategy** for efficiency (one sampler instance used by all materials)
- **Synchronous texture loading** using stb_image (PNG, JPEG, TGA, BMP)
- **Fallback to scalar albedo** for materials without textures (backward compatible)
- **UV checker texture** for debugging texture mapping

### Current Scope

- ✅ Albedo textures
- ✅ Emissive textures (self-illuminating materials)
- ✅ Normal maps (PBR)
- ✅ Metallic/roughness maps (PBR)
- ✅ Ambient occlusion maps
- ✅ IBL cubemaps with mipmaps (irradiance, prefiltered environment, BRDF LUT)
- ✅ Synchronous loading
- ✅ Multi-mip cubemaps (for IBL prefiltered environment)
- ✅ Device-local GPU memory with staging buffers
- ✅ DDS file format support (including DX10 extended headers)
- ✅ Float16 textures (R16G16B16A16_SFLOAT, R16G16_SFLOAT)
- ⏳ Async texture streaming (future)
- ⏳ Procedural mipmap generation (future)

## Architecture

### Sampler Strategy: Shared Global Samplers

MetaGFX uses a **shared sampler approach**:
- Application creates samplers at startup (e.g., linear-repeat, nearest-repeat, linear-clamp)
- Materials reference textures only, not samplers
- Descriptor sets bind texture + sampler pairs
- This matches D3D12/Metal best practices and respects Vulkan's limited sampler count

**Benefits**:
- Reduced descriptor overhead (reuse sampler across materials)
- Lower memory usage
- Easier management (change filtering globally)

### Descriptor Binding Layout

The graphics pipeline uses multiple descriptor bindings:

| Binding | Type | Stage | Purpose |
|---------|------|-------|---------|
| 0 | Uniform Buffer | Vertex | MVP matrices |
| 1 | Uniform Buffer | Fragment | Material properties (albedo, roughness, metallic, emissive) |
| 2 | Combined Image Sampler | Fragment | Albedo texture + sampler |
| 3 | Storage Buffer | Fragment | Light buffer (dynamic lights) |
| 4 | Combined Image Sampler | Fragment | Normal map |
| 5 | Combined Image Sampler | Fragment | Metallic map |
| 6 | Combined Image Sampler | Fragment | Roughness map |
| 7 | Combined Image Sampler | Fragment | Ambient occlusion map |
| 8 | Combined Image Sampler | Fragment | Irradiance cubemap (IBL diffuse) |
| 9 | Combined Image Sampler | Fragment | Prefiltered environment cubemap (IBL specular) |
| 10 | Combined Image Sampler | Fragment | BRDF integration LUT (2D texture) |
| 11 | Combined Image Sampler | Fragment | Emissive texture + sampler |

When a material lacks a specific texture, bindings use appropriate default textures (checker for albedo, flat normal, white for roughness/metallic/AO, black for emissive).

**Note**: Bindings 3-5 are used for Image-Based Lighting (IBL). See [ibl_system.md](ibl_system.md) for details.

### Push Constants

The fragment shader receives material flags and rendering parameters via push constants:

```cpp
struct PushConstants {
    glm::vec4 cameraPosition;  // xyz = camera position, w unused (16 bytes)
    uint32 materialFlags;      // Bit 0: has albedo texture (4 bytes)
    float exposure;            // HDR exposure value (4 bytes)
    uint32 enableIBL;          // 0 = disabled, 1 = enabled (4 bytes)
    float iblIntensity;        // IBL contribution multiplier (4 bytes)
};  // Total: 32 bytes
```

**Material Flags**:
- Bit 0: `HasAlbedoMap` - Set if material has albedo texture
- Bit 1: `HasNormalMap` - Set if material has normal map
- Bit 2: `HasMetallicMap` - Set if material has metallic texture
- Bit 3: `HasRoughnessMap` - Set if material has roughness texture
- Bit 4: `HasMetallicRoughnessMap` - Set if material has combined metallic-roughness (glTF)
- Bit 5: `HasAOMap` - Set if material has ambient occlusion map
- Bit 6: `HasEmissiveMap` - Set if material has emissive texture

**IBL Parameters**:
- `enableIBL` - Toggle Image-Based Lighting on/off
- `iblIntensity` - Scale IBL contribution (default: 0.05 for subtle effect)

The shader conditionally samples textures based on these flags.

## Core Components

### 1. Sampler (`Sampler.h`, `VulkanSampler.h/cpp`)

Abstract sampler interface with Vulkan implementation.

**Creation**:
```cpp
rhi::SamplerDesc samplerDesc{};
samplerDesc.minFilter = rhi::Filter::Linear;
samplerDesc.magFilter = rhi::Filter::Linear;
samplerDesc.mipmapMode = rhi::Filter::Linear;
samplerDesc.addressModeU = rhi::SamplerAddressMode::Repeat;
samplerDesc.addressModeV = rhi::SamplerAddressMode::Repeat;
samplerDesc.addressModeW = rhi::SamplerAddressMode::Repeat;
samplerDesc.anisotropyEnable = true;
samplerDesc.maxAnisotropy = 16.0f;

Ref<rhi::Sampler> sampler = device->CreateSampler(samplerDesc);
```

**Vulkan Implementation**:
- Wraps `VkSampler` handle
- Converts RHI enums to Vulkan types (`Filter` → `VkFilter`, `SamplerAddressMode` → `VkSamplerAddressMode`)
- Supports anisotropic filtering

### 2. VulkanTexture (`VulkanTexture.h/cpp`)

Complete Vulkan texture implementation supporting:
- VkImage creation with device-local memory
- Staging buffer uploads
- Image layout transitions
- VkImageView for shader access

**Key Methods**:

```cpp
class VulkanTexture : public Texture {
public:
    VulkanTexture(VulkanContext& context, const TextureDesc& desc);
    void UploadData(const void* data, uint64 size) override;

    VkImageView GetImageView() const { return m_ImageView; }
    VkImage GetImage() const { return m_Image; }

private:
    void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
    uint32 FindMemoryType(uint32 typeFilter, VkMemoryPropertyFlags properties);
};
```

**Texture Creation Flow**:
1. Create `VkImage` with format, dimensions, usage flags
2. Allocate device-local memory (`VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`)
3. Bind memory to image
4. Create `VkImageView` for shader sampling
5. Transition to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`

**Data Upload Flow** (`UploadData`):
1. Create staging buffer (host-visible, `VK_BUFFER_USAGE_TRANSFER_SRC`)
2. Map memory and copy pixel data
3. Transition image: `SHADER_READ_ONLY` → `TRANSFER_DST`
4. `vkCmdCopyBufferToImage` from staging buffer
5. Transition image: `TRANSFER_DST` → `SHADER_READ_ONLY`
6. Submit command buffer and wait
7. Cleanup staging resources

**Image Layout Transitions**:
- Uses `VkImageMemoryBarrier` with pipeline barriers
- Supports: `UNDEFINED → TRANSFER_DST`, `TRANSFER_DST → SHADER_READ`, `SHADER_READ → TRANSFER_DST`

### 3. TextureUtils (`TextureUtils.h/cpp`)

Utility functions for image loading and texture creation.

**API**:

```cpp
namespace metagfx::utils {

// Image data structure
struct ImageData {
    uint8* pixels = nullptr;
    uint32 width = 0;
    uint32 height = 0;
    uint32 channels = 0;
};

// Load image from file using stb_image
ImageData LoadImage(const std::string& filepath, int desiredChannels = 4);

// Free stb_image data
void FreeImage(ImageData& data);

// Create texture from raw image data
Ref<rhi::Texture> CreateTextureFromImage(
    rhi::GraphicsDevice* device,
    const ImageData& imageData,
    rhi::Format format = rhi::Format::R8G8B8A8_SRGB
);

// Convenience: load file and create GPU texture
Ref<rhi::Texture> LoadTexture(
    rhi::GraphicsDevice* device,
    const std::string& filepath
);

} // namespace metagfx::utils
```

**Supported Formats**:
- PNG (8-bit, 16-bit)
- JPEG (baseline, progressive)
- TGA (uncompressed, RLE)
- BMP (uncompressed)
- DDS (DirectDraw Surface with DX10 extended headers, for HDR cubemaps)

**Error Handling**:
- Throws `std::runtime_error` on missing files or load failures
- Logs warnings with `METAGFX_WARN`

### 4. Material Extension (`Material.h/cpp`)

Materials now support optional albedo textures.

**Extended API**:

```cpp
class Material {
public:
    // Texture methods
    void SetAlbedoMap(Ref<rhi::Texture> texture);
    Ref<rhi::Texture> GetAlbedoMap() const;
    bool HasAlbedoMap() const;
    uint32 GetTextureFlags() const;

private:
    MaterialProperties m_Properties;  // Scalar albedo, roughness, metallic
    Ref<rhi::Texture> m_AlbedoMap;
    uint32 m_TextureFlags = 0;
};
```

**Material Flags**:

```cpp
enum class MaterialTextureFlags : uint32 {
    None = 0,
    HasAlbedoMap = 1 << 0
};
```

**Backward Compatibility**:
- Materials without textures use scalar `albedo` from `MaterialProperties`
- Shader checks flags to decide: sample texture or use scalar color

### 5. VulkanDescriptorSet Extension

Descriptor sets now support both uniform buffers and combined image samplers.

**Extended Binding Structure**:

```cpp
struct DescriptorBinding {
    uint32 binding;
    VkDescriptorType type;
    VkShaderStageFlags stageFlags;

    Ref<Buffer> buffer;    // For uniform/storage buffers
    Ref<Texture> texture;  // For combined image samplers
    Ref<Sampler> sampler;  // For combined image samplers
};
```

**New Method**:

```cpp
void VulkanDescriptorSet::UpdateTexture(
    uint32 binding,
    Ref<Texture> texture,
    Ref<Sampler> sampler
);
```

**Descriptor Pool Sizing**:
- Counts both `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` and `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`
- Allocates pool sizes: `descriptorCount = count * MAX_FRAMES`

## Texture Loading

### Loading Textures from Files

**Example**:

```cpp
#include "metagfx/utils/TextureUtils.h"

// Load texture from file
Ref<rhi::Texture> texture = utils::LoadTexture(device, "assets/textures/wood.png");

// Use texture in material
material->SetAlbedoMap(texture);
```

### Creating Procedural Textures

**Example - UV Checker Texture**:

```cpp
constexpr int texSize = 128;
constexpr int checkerSize = 8;
uint8_t pixels[texSize * texSize * 4];

for (int y = 0; y < texSize; y++) {
    for (int x = 0; x < texSize; x++) {
        int idx = (y * texSize + x) * 4;
        bool isMagenta = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;

        pixels[idx + 0] = isMagenta ? 255 : 255;  // R
        pixels[idx + 1] = isMagenta ? 0 : 255;    // G
        pixels[idx + 2] = isMagenta ? 255 : 255;  // B
        pixels[idx + 3] = 255;                    // A
    }
}

utils::ImageData imageData{pixels, texSize, texSize, 4};
Ref<rhi::Texture> checkerTexture = utils::CreateTextureFromImage(
    device, imageData, rhi::Format::R8G8B8A8_UNORM
);
```

### Assimp Texture Extraction

The `Model` class automatically extracts textures from Assimp materials:

```cpp
// In Model::ProcessMaterial (Model.cpp)
if (aiMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
    aiString texPath;
    if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
        // Resolve path relative to model directory
        std::filesystem::path fullPath = std::filesystem::path(modelDir) / texPath.C_Str();

        try {
            auto texture = utils::LoadTexture(device, fullPath.string());
            if (texture) {
                material->SetAlbedoMap(texture);
                METAGFX_INFO << "Loaded albedo texture: " << fullPath.string();
            }
        } catch (const std::exception& e) {
            METAGFX_WARN << "Failed to load texture: " << fullPath.string();
        }
    }
}
```

**Texture Path Resolution**:
- Paths are resolved relative to the model file's directory
- Example: `models/bunny.obj` with texture `textures/bunny_diffuse.png` → `models/textures/bunny_diffuse.png`

### IBL Texture Loading

The renderer loads Image-Based Lighting textures from DDS files at startup. See [ibl_system.md](ibl_system.md) for complete details.

**Loading DDS Cubemaps**:

```cpp
#include "metagfx/utils/TextureUtils.h"

// Load IBL cubemaps (multi-mip, float16 format)
Ref<rhi::Texture> irradianceMap = utils::LoadDDSCubemap(
    device, "assets/envmaps/irradiance.dds"
);

Ref<rhi::Texture> prefilteredMap = utils::LoadDDSCubemap(
    device, "assets/envmaps/prefiltered.dds"
);

// Load BRDF LUT (2D texture, float16 format)
Ref<rhi::Texture> brdfLUT = utils::LoadDDS2DTexture(
    device, "assets/envmaps/brdf_lut.dds"
);
```

**DDS Texture Specifications**:
- `irradiance.dds` - 64×64 cubemap, 1 mip level, R16G16B16A16_SFLOAT
- `prefiltered.dds` - 512×512 cubemap, 6 mip levels, R16G16B16A16_SFLOAT
- `brdf_lut.dds` - 512×512 2D texture, 1 mip level, R16G16_SFLOAT

**Critical Implementation Details**:

For multi-mip cubemaps to work correctly on MoltenVK, the image layout transition sequence must be:

1. Image created in `VK_IMAGE_LAYOUT_UNDEFINED`
2. **Do NOT** transition layout in the VulkanTexture constructor
3. In `UploadData()`, transition: `UNDEFINED → TRANSFER_DST_OPTIMAL`
4. Upload all mip levels and faces
5. Transition: `TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`

Premature layout transitions cause MoltenVK to mishandle multi-mip cubemap uploads, resulting in black textures. See [ibl_system.md](ibl_system.md) for the detailed explanation.

**DDS Data Layout**:

The prefiltered cubemap DDS file stores data sequentially by mip level, then by face:
```
Mip 0: [+X, -X, +Y, -Y, +Z, -Z]
Mip 1: [+X, -X, +Y, -Y, +Z, -Z]
...
Mip 5: [+X, -X, +Y, -Y, +Z, -Z]
```

The upload code matches this layout by looping mips first, then faces within each mip.

## Material Integration

### Using Textures in Materials

```cpp
// Create material
auto material = std::make_unique<Material>();
material->SetAlbedo(glm::vec3(1.0f, 1.0f, 1.0f));  // White tint
material->SetRoughness(0.5f);
material->SetMetallic(0.0f);

// Load and set albedo texture
Ref<rhi::Texture> albedoMap = utils::LoadTexture(device, "assets/textures/brick.png");
material->SetAlbedoMap(albedoMap);

// Check texture presence
if (material->HasAlbedoMap()) {
    uint32 flags = material->GetTextureFlags();
    // flags & MaterialTextureFlags::HasAlbedoMap is true
}
```

### Rendering with Textures

**Per-Frame Setup**:

```cpp
// Create shared sampler (once at startup)
rhi::SamplerDesc samplerDesc{};
samplerDesc.minFilter = rhi::Filter::Linear;
samplerDesc.magFilter = rhi::Filter::Linear;
samplerDesc.addressModeU = rhi::SamplerAddressMode::Repeat;
// ... configure sampler
Ref<rhi::Sampler> sampler = device->CreateSampler(samplerDesc);

// Create default fallback texture (once at startup)
Ref<rhi::Texture> defaultTexture = /* UV checker or white texture */;
```

**Per-Mesh Rendering**:

```cpp
for (const auto& mesh : model->GetMeshes()) {
    Material* material = mesh->GetMaterial();

    // Update material uniform buffer
    MaterialProperties matProps = material->GetProperties();
    materialBuffer->CopyData(&matProps, sizeof(matProps));

    // Bind texture (or default fallback)
    Ref<rhi::Texture> albedoMap = material->GetAlbedoMap();
    uint32 flags;

    if (albedoMap) {
        descriptorSet->UpdateTexture(2, albedoMap, sampler);
        flags = material->GetTextureFlags();
    } else {
        // Use default texture and force texture sampling
        descriptorSet->UpdateTexture(2, defaultTexture, sampler);
        flags = static_cast<uint32>(MaterialTextureFlags::HasAlbedoMap);
    }

    // Push material flags
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        16,  // Offset after cameraPosition vec4
        sizeof(uint32),
        &flags
    );

    // Draw mesh
    cmd->BindVertexBuffer(mesh->GetVertexBuffer());
    cmd->BindIndexBuffer(mesh->GetIndexBuffer());
    cmd->DrawIndexed(mesh->GetIndexCount());
}
```

## Shader Integration

### Fragment Shader

**Descriptor Bindings**:

```glsl
#version 450

// Binding 0: MVP matrices (vertex shader)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

// Binding 1: Material properties
layout(binding = 1) uniform MaterialUBO {
    vec3 albedo;
    float roughness;
    float metallic;
} material;

// Binding 2: Albedo texture sampler
layout(binding = 2) uniform sampler2D albedoSampler;

// Binding 3-5: IBL textures (Image-Based Lighting)
layout(binding = 3) uniform samplerCube irradianceMap;
layout(binding = 4) uniform samplerCube prefilteredMap;
layout(binding = 5) uniform sampler2D brdfLUT;
```

**Push Constants**:

```glsl
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;  // xyz = position, w unused
    uint materialFlags;   // Bit 0: has albedo texture
    float exposure;       // HDR exposure value
    uint enableIBL;       // 0 = disabled, 1 = enabled
    float iblIntensity;   // IBL contribution multiplier
} pushConstants;
```

**Conditional Texture Sampling**:

```glsl
void main() {
    // Sample texture or use scalar albedo
    vec3 objectColor;
    if ((pushConstants.materialFlags & 1u) != 0u) {
        // Material has albedo texture - sample it
        objectColor = texture(albedoSampler, fragTexCoord).rgb;
    } else {
        // No texture - use scalar albedo from material UBO
        objectColor = material.albedo;
    }

    // ... rest of lighting calculations ...
}
```

**Input Variables**:

```glsl
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;  // UV coordinates

layout(location = 0) out vec4 outColor;
```

### Shader Compilation

After modifying shaders:

```bash
cd src/app

# Compile to SPIR-V
glslangValidator -V model.frag -o model.frag.spv

# Convert to C++ include file
xxd -i model.frag.spv | grep -v "unsigned\|^}" | sed 's/^  //' > model.frag.spv.inl
```

The `.spv.inl` file is included in `Application.cpp`:

```cpp
std::vector<uint8> fragShaderCode = {
    #include "model.frag.spv.inl"
};
```

## Emissive Textures

**Added**: January 9, 2026

Emissive textures enable materials to emit light independently of scene lighting, creating self-illuminating surfaces for screens, lights, neon signs, or glowing elements.

### Material API

```cpp
class Material {
public:
    // Set emissive texture
    void SetEmissiveMap(Ref<rhi::Texture> texture);

    // Set emissive color multiplier (can exceed 1.0 for HDR)
    void SetEmissiveFactor(const glm::vec3& emissive);

    // Query emissive properties
    Ref<rhi::Texture> GetEmissiveMap() const;
    const glm::vec3& GetEmissiveFactor() const;
    bool HasEmissiveMap() const;
};
```

### Shader Integration

Emissive contribution is added AFTER lighting calculations but BEFORE tone mapping:

```glsl
// Binding 11: Emissive texture
layout(binding = 11) uniform sampler2D emissiveSampler;

// Material uniform (48 bytes with emissiveFactor)
layout(binding = 1) uniform MaterialUBO {
    vec3 albedo;
    float roughness;
    float metallic;
    vec2 padding1;
    vec3 emissiveFactor;  // RGB multiplier for emissive
    float padding2;
} material;

void main() {
    // ... PBR lighting calculations ...
    vec3 color = ambient + Lo;  // Lighting result

    // Add emissive contribution
    vec3 emissive = vec3(0.0);
    if ((pushConstants.materialFlags & (1u << 6)) != 0u) {  // HasEmissiveMap
        emissive = texture(emissiveSampler, fragTexCoord).rgb * material.emissiveFactor;
    } else {
        emissive = material.emissiveFactor;
    }
    color += emissive;

    // Tone mapping and gamma correction
    color = color * pushConstants.exposure;
    color = clamp(color, 0.0, 1.0);
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
```

**Key Points**:
- Emissive is unaffected by lighting, shadows, or ambient occlusion
- Added before tone mapping to allow HDR bloom effects
- emissiveFactor can exceed 1.0 for bright glowing effects
- Uses default black texture (no emission) when not present

### Example: Creating Glowing Material

```cpp
// Load emissive texture
Ref<rhi::Texture> emissiveMap = utils::LoadTexture(device, "assets/glow.png");

// Create material with HDR emissive
auto material = std::make_unique<Material>();
material->SetAlbedoMap(albedoTexture);
material->SetEmissiveMap(emissiveMap);
material->SetEmissiveFactor(glm::vec3(2.0f, 2.0f, 2.0f));  // 2x brightness (HDR)
material->SetRoughness(0.8f);
material->SetMetallic(0.0f);

// Assign to mesh
mesh->SetMaterial(std::move(material));
```

### glTF Integration

Emissive properties are automatically extracted from glTF models:

```cpp
// Model.cpp - ProcessMaterial() extracts:
// - material.emissiveFactor (vec3, default [0,0,0])
// - material.emissiveTexture (if present)
// Final emissive = emissiveTexture.rgb * emissiveFactor

auto model = Model::LoadFromFile(device, "assets/DamagedHelmet.gltf");
// Emissive textures automatically loaded and applied
```

**glTF 2.0 Specification**:
- `pbrMetallicRoughness.emissiveTexture`: RGB texture
- `pbrMetallicRoughness.emissiveFactor`: RGB multiplier [0-1] or higher for HDR
- Used for self-illuminating elements (screens, lights, LEDs)

### Default Black Texture

When a material has no emissive map, binding 11 uses a 1x1 black texture (RGB 0,0,0):

```cpp
// Application.cpp - CreateDefaultTextures()
uint8_t blackPixel[4] = {0, 0, 0, 255};  // RGBA black
utils::ImageData blackImage{blackPixel, 1, 1, 4};
m_DefaultBlackTexture = utils::CreateTextureFromImage(
    m_Device.get(), blackImage, rhi::Format::R8G8B8A8_UNORM
);
```

This ensures all materials have valid emissive bindings, even when not using emissive.

## Usage Examples

### Example 1: Simple Textured Material

```cpp
// Load texture
Ref<rhi::Texture> brickTexture = utils::LoadTexture(device, "assets/brick.png");

// Create material
auto material = std::make_unique<Material>();
material->SetAlbedoMap(brickTexture);
material->SetRoughness(0.8f);
material->SetMetallic(0.0f);

// Assign to mesh
mesh->SetMaterial(std::move(material));
```

### Example 2: Loading Textured Model

```cpp
// Load model (textures extracted automatically)
auto model = std::make_unique<Model>();
model->LoadFromFile(device, "assets/models/house.obj");

// Textures are already loaded and assigned to meshes
for (const auto& mesh : model->GetMeshes()) {
    Material* mat = mesh->GetMaterial();
    if (mat->HasAlbedoMap()) {
        METAGFX_INFO << "Mesh has albedo texture";
    }
}
```

### Example 3: Creating Sampler Variations

```cpp
// Linear filtering, repeat wrapping (default)
rhi::SamplerDesc linearRepeat{};
linearRepeat.minFilter = rhi::Filter::Linear;
linearRepeat.magFilter = rhi::Filter::Linear;
linearRepeat.addressModeU = rhi::SamplerAddressMode::Repeat;
linearRepeat.addressModeV = rhi::SamplerAddressMode::Repeat;
Ref<rhi::Sampler> samplerLinearRepeat = device->CreateSampler(linearRepeat);

// Nearest filtering, clamp to edge (for UI textures)
rhi::SamplerDesc nearestClamp{};
nearestClamp.minFilter = rhi::Filter::Nearest;
nearestClamp.magFilter = rhi::Filter::Nearest;
nearestClamp.addressModeU = rhi::SamplerAddressMode::ClampToEdge;
nearestClamp.addressModeV = rhi::SamplerAddressMode::ClampToEdge;
Ref<rhi::Sampler> samplerNearestClamp = device->CreateSampler(nearestClamp);
```

## Troubleshooting

### Issue: Textures appear solid color or incorrect

**Possible Causes**:
1. **Model lacks UV coordinates**: Use a UV checker texture to verify. Models without UVs default to (0,0), sampling only the top-left pixel.
2. **Incorrect texture path**: Check that texture paths are resolved relative to model directory.
3. **Texture load failure**: Check logs for `METAGFX_WARN` messages about failed texture loads.

**Solutions**:
- Verify model has UV coordinates (use Blender or other tool to inspect)
- Use UV checker texture to visualize mapping: `m_DefaultTexture` in `Application.cpp`
- Check texture file exists at resolved path
- Try loading texture manually with `utils::LoadTexture` and check for exceptions

### Issue: Validation error on exit about `vkDestroyImageView`

**Cause**: Textures or samplers not cleaned up before device destruction.

**Solution**: Reset texture and sampler resources before device in `Application::Shutdown()`:

```cpp
void Application::Shutdown() {
    // Cleanup textures and samplers BEFORE device
    m_DefaultTexture.reset();
    m_LinearRepeatSampler.reset();

    // Then cleanup device
    m_Device.reset();
}
```

### Issue: Checkerboard pattern too small or blended

**Cause**: Texture resolution too low, linear filtering blends colors.

**Solution**: Increase texture size and checker size:

```cpp
constexpr int texSize = 128;      // 128x128 resolution
constexpr int checkerSize = 8;    // 8x8 pixel checkers → 16x16 grid
```

### Issue: Texture appears upside-down

**Cause**: Different UV coordinate conventions (OpenGL Y-up vs Vulkan Y-down).

**Solution**:
- Flip texture during load: `stbi_set_flip_vertically_on_load(1);` in `TextureUtils.cpp`
- Or flip V coordinate in shader: `vec2 uv = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y);`

### Issue: Descriptor set binding errors

**Cause**: Descriptor pool not sized for combined image samplers.

**Solution**: Ensure `VulkanDescriptorSet::AllocateSets()` counts image sampler bindings:

```cpp
uint32 imageSamplerCount = 0;
for (const auto& binding : bindings) {
    if (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        imageSamplerCount++;
    }
}

if (imageSamplerCount > 0) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = imageSamplerCount * MAX_FRAMES;
    poolSizes.push_back(poolSize);
}
```

### Issue: Push constant size mismatch

**Cause**: C++ push constant struct doesn't match shader layout.

**Solution**: Verify push constant range size in `VulkanPipeline.cpp`:

```cpp
VkPushConstantRange pushConstantRange{};
pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
pushConstantRange.offset = 0;
pushConstantRange.size = 32;  // vec4(16) + uint(4) + padding(12) = 32 bytes
```

And shader layout:

```glsl
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;  // 16 bytes
    uint materialFlags;   // 4 bytes
    uint padding[3];      // 12 bytes (align to 16)
};  // Total: 32 bytes
```

## Future Extensions

### Phase 3: Advanced Material System

- **Normal Mapping**: Tangent-space normal maps for surface detail
- **PBR Texture Maps**: Roughness, metallic, ambient occlusion maps
- **Emissive Maps**: Self-illuminating surfaces
- **Additional Descriptor Bindings**: Extend to bindings 3-6 for multiple texture types

### Performance Optimizations

- **Mipmap Generation**: Automatic mipmap creation for better performance and quality
- **Texture Compression**: BC1-BC7, ASTC for reduced memory usage
- **Descriptor Indexing**: Bindless textures for rendering many materials efficiently
- **Async Texture Loading**: Stream textures on background threads

### Additional Features

- ✅ **Cube Maps**: Environment maps for skyboxes and reflections (implemented for IBL)
- **Texture Atlases**: Pack multiple textures into single atlas
- **Texture Arrays**: Array textures for terrain splatting, decals
- **Render Targets**: Render-to-texture for post-processing, shadows

### Advanced Sampling

- **Trilinear Filtering**: Between mip levels
- **Anisotropic Filtering**: Already supported (16x max)
- **Custom Mipmap Bias**: Control mip level selection
- **Shadow Samplers**: PCF filtering for shadow maps (compare mode)

---

**References**:
- [Vulkan Texture Tutorial](https://vulkan-tutorial.com/Texture_mapping)
- [RHI Architecture](rhi.md)
- [Material System](../claude/milestone_2_2/implementation_notes.md)
- [Model Loading](model_loading.md)
- [IBL System](ibl_system.md) - Image-Based Lighting with cubemaps and mipmaps
