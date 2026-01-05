# Image-Based Lighting (IBL) System

## Overview

MetaGFX implements physically-based Image-Based Lighting (IBL) using pre-computed environment cubemaps. The IBL system provides realistic ambient lighting and environment reflections that vary based on material roughness and viewing angle.

## Architecture

The IBL implementation uses the **split-sum approximation** developed by Epic Games for Unreal Engine 4. This technique separates the lighting integral into two parts that can be pre-computed:

1. **Diffuse Irradiance** - Pre-convolved environment map for diffuse lighting
2. **Specular Prefiltered Environment** - Importance-sampled environment maps at different roughness levels (stored as mipmaps)
3. **BRDF Integration LUT** - 2D lookup table encoding the Fresnel-BRDF integration

### Assets

IBL textures are generated offline using the `ibl_precompute` tool:

```bash
./bin/tools/ibl_precompute <input.hdr> <output_directory>
```

**Generated textures:**
- `irradiance.dds` - 64×64 cubemap, 1 mip level, R16G16B16A16_FLOAT
- `prefiltered.dds` - 512×512 cubemap, 6 mip levels, R16G16B16A16_FLOAT
- `brdf_lut.dds` - 512×512 2D texture, 1 mip level, R16G16_FLOAT
- `environment.dds` - Original HDR environment (for debugging/reference)

### Texture Loading

IBL textures are loaded at application startup:

```cpp
// Load IBL cubemaps (src/app/Application.cpp)
m_IrradianceMap = utils::LoadDDSCubemap(device, "assets/envmaps/irradiance.dds");
m_PrefilteredMap = utils::LoadDDSCubemap(device, "assets/envmaps/prefiltered.dds");
m_BRDF_LUT = utils::LoadDDS2DTexture(device, "assets/envmaps/brdf_lut.dds");
```

**Key implementation details:**
- Uses `LoadDDSCubemap()` for cubemap textures (6 array layers)
- Uses `LoadDDS2DTexture()` for the BRDF LUT
- Supports float16 formats (R16G16B16A16_SFLOAT, R16G16_SFLOAT)
- All mip levels are uploaded sequentially per face

## Shader Implementation

The IBL calculation is performed in the fragment shader (`src/app/model.frag`):

### Diffuse IBL

```glsl
// Sample irradiance map with surface normal
vec3 irradiance = texture(irradianceMap, N).rgb;

// Calculate diffuse component with energy conservation
vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
vec3 kD = (1.0 - F) * (1.0 - metallic); // Metals have no diffuse

vec3 diffuseIBL = kD * irradiance * albedo;
```

### Specular IBL

```glsl
// Calculate reflection vector
vec3 R = reflect(-V, N);

// Sample prefiltered environment map based on roughness
const float MAX_REFLECTION_LOD = 5.0; // Number of mip levels - 1
float lod = roughness * MAX_REFLECTION_LOD;
vec3 prefilteredColor = textureLod(prefilteredMap, R, lod).rgb;

// Sample BRDF integration map
vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;

// Combine using split-sum approximation
vec3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
```

### Final Lighting

```glsl
// Combine diffuse and specular IBL
vec3 ambient = (diffuseIBL + specularIBL) * pushConstants.iblIntensity;

// Add to direct lighting
vec3 color = ambient + directLighting;
```

## Critical Implementation Details

### 1. Texture Upload Issue (MoltenVK)

**Problem:** Multi-mip cubemaps were showing black when the image layout was prematurely transitioned.

**Solution:** The VulkanTexture constructor must NOT transition the layout before data upload:

```cpp
// VulkanTexture.cpp constructor (CORRECT)
// Don't transition layout here - let UploadData handle it
```

The layout transition sequence must be:
1. Image created in `UNDEFINED` layout
2. `UploadData()` transitions: `UNDEFINED → TRANSFER_DST_OPTIMAL`
3. Upload all mip levels and faces
4. Transition: `TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL`

### 2. Ambient Occlusion Issue

**Problem:** The helmet's AO texture (from metallic-roughness map) was multiplying IBL by ~0, causing black reflections.

**Solution:** Do NOT multiply IBL by ambient occlusion:

```glsl
// WRONG: ambient = (diffuseIBL + specularIBL) * ao;
// RIGHT: ambient = (diffuseIBL + specularIBL) * iblIntensity;
```

AO already affects diffuse direct lighting and shouldn't double-darken IBL.

### 3. Tone Mapping Issue

**Problem:** ACES tone mapper was causing black artifacts with IBL values.

**Solution:** Use simple clamping instead of ACES for now:

```glsl
// Simplified tone mapping (works better with IBL)
color = clamp(color, 0.0, 1.0);

// Instead of:
// color = toneMapACES(color); // Was causing black with IBL
```

### 4. Data Layout

The prefiltered cubemap DDS file stores data as:
```
Mip 0: [Face0, Face1, Face2, Face3, Face4, Face5]
Mip 1: [Face0, Face1, Face2, Face3, Face4, Face5]
...
Mip 5: [Face0, Face1, Face2, Face3, Face4, Face5]
```

The upload code matches this layout by looping mips first, then faces:
```cpp
for (uint32 mip = 0; mip < mipLevels; ++mip) {
    for (uint32 layer = 0; layer < 6; ++layer) {
        // Create one VkBufferImageCopy region per face
        // Upload each face separately for MoltenVK compatibility
    }
}
```

## User Controls

The IBL system provides runtime controls via ImGui:

```cpp
// GUI controls (src/app/Application.cpp)
ImGui::Checkbox("Enable IBL", &m_EnableIBL);
ImGui::SliderFloat("IBL Intensity", &m_IBLIntensity, 0.0f, 2.0f);
```

**Default settings:**
- `m_EnableIBL = true`
- `m_IBLIntensity = 0.05f` (very subtle)

These values are passed to the shader via push constants:

```cpp
// Push constants structure
struct PushConstants {
    vec4 cameraPosition;
    uint materialFlags;
    float exposure;
    uint enableIBL;      // 0 = disabled, 1 = enabled
    float iblIntensity;  // 0.0 to 2.0 range
};
```

## Debugging Tips

### Visualizing IBL Components

Uncomment debug lines in `model.frag` to visualize individual components:

```glsl
// Show raw prefiltered cubemap
outColor = vec4(prefilteredColor * 5.0, 1.0); return;

// Show BRDF LUT values
outColor = vec4(brdf.x, brdf.y, 0.0, 1.0); return;

// Show irradiance map
outColor = vec4(irradiance * 2.0, 1.0); return;

// Show final ambient contribution
outColor = vec4(ambient * 10.0, 1.0); return;
```

### Common Issues

**Black reflections in some directions:**
- Check that all 6 cubemap faces are uploaded correctly
- Verify the cubemap face order matches Vulkan standard: +X, -X, +Y, -Y, +Z, -Z

**Black reflections after tone mapping:**
- Try bypassing ACES and using simple clamp
- Check IBL intensity isn't too high (causing overflow)

**Overly bright IBL:**
- Reduce `m_IBLIntensity` default value
- Check that IBL isn't being multiplied by additional factors

## Performance Considerations

**Texture Memory:**
- Irradiance: 64×64×6 faces × 8 bytes = 196 KB
- Prefiltered: 512×512×6 faces × 6 mips × 8 bytes ≈ 16 MB
- BRDF LUT: 512×512 × 4 bytes = 1 MB
- **Total: ~17 MB**

**Runtime Cost:**
- 3 texture samples per fragment (irradiance, prefiltered, BRDF LUT)
- Minimal ALU cost (split-sum approximation is very efficient)
- No per-light loops for ambient contribution

## References

- [Epic Games: Real Shading in Unreal Engine 4](https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
- [LearnOpenGL: IBL - Diffuse Irradiance](https://learnopengl.com/PBR/IBL/Diffuse-irradiance)
- [LearnOpenGL: IBL - Specular IBL](https://learnopengl.com/PBR/IBL/Specular-IBL)

## Related Documentation

- [PBR Rendering](pbr_rendering.md) - Cook-Torrance BRDF and material system
- [Textures and Samplers](textures_and_samplers.md) - Texture loading and sampling
- [MoltenVK Compatibility](moltenvk_compatibility.md) - Multi-mip cubemap upload issues
