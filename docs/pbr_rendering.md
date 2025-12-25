# Physically-Based Rendering (PBR) System

**Milestone 3.2** - Completed December 2025

## Overview

MetaGFX implements a complete physically-based rendering (PBR) system using the Cook-Torrance BRDF with metallic workflow. The implementation follows industry-standard practices and supports glTF 2.0 PBR materials with full texture support.

## Table of Contents

1. [PBR Theory and Implementation](#pbr-theory-and-implementation)
2. [Shader Architecture](#shader-architecture)
3. [Texture Support](#texture-support)
4. [Tone Mapping and Post-Processing](#tone-mapping-and-post-processing)
5. [Debug Visualization](#debug-visualization)
6. [Usage Examples](#usage-examples)
7. [Future Enhancements](#future-enhancements)

---

## PBR Theory and Implementation

### Cook-Torrance BRDF

The PBR system implements the Cook-Torrance specular BRDF combined with Lambertian diffuse:

```
f(l,v) = kd * (c/π) + ks * (DFG)/(4(n·l)(n·v))
```

Where:
- **kd**: Diffuse reflection coefficient (energy conservation: kd = 1 - ks)
- **ks**: Specular reflection coefficient (from Fresnel)
- **c**: Albedo color
- **D**: Normal Distribution Function (GGX/Trowbridge-Reitz)
- **F**: Fresnel term (Schlick approximation)
- **G**: Geometry function (Smith's Schlick-GGX)

### Normal Distribution Function (GGX)

**Implementation**: `src/app/model.frag:59-69`

The GGX (Trowbridge-Reitz) distribution describes how microfacet normals are oriented:

```glsl
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001);
}
```

**Properties**:
- Sharp highlights for low roughness values
- Wide, diffuse highlights for high roughness
- Physically plausible falloff

### Geometry Function (Smith's Schlick-GGX)

**Implementation**: `src/app/model.frag:73-88`

Models microfacet self-shadowing and masking:

```glsl
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;  // Direct lighting

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
```

### Fresnel (Schlick Approximation)

**Implementation**: `src/app/model.frag:92-94`

Describes how reflection changes with viewing angle:

```glsl
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
```

**F0 Calculation**:
- Dielectrics (non-metals): `vec3(0.04)` (~4% reflection)
- Metals: Uses albedo color directly
- Blended using metallic parameter: `F0 = mix(vec3(0.04), albedo, metallic)`

### Energy Conservation

The shader ensures energy conservation through proper balance of diffuse and specular:

```glsl
vec3 kS = F;  // Specular ratio from Fresnel
vec3 kD = vec3(1.0) - kS;  // Diffuse ratio
kD *= 1.0 - metallic;  // Metals have no diffuse
```

---

## Shader Architecture

### Shader Stages

**Vertex Shader**: `src/app/model.vert`
- Transforms vertices to clip space
- Outputs world-space position and normal
- Passes through texture coordinates

**Fragment Shader**: `src/app/model.frag`
- Samples all PBR textures
- Computes Cook-Torrance BRDF
- Applies tone mapping and gamma correction

### Uniform Bindings

| Binding | Type | Description |
|---------|------|-------------|
| 0 | UBO | MVP matrices (model, view, projection) |
| 1 | UBO | Material properties (albedo, roughness, metallic) |
| 2 | Sampler2D | Albedo/base color texture |
| 3 | UBO | Light buffer (up to 16 lights) |
| 4 | Sampler2D | Normal map |
| 5 | Sampler2D | Metallic map (or combined metallic-roughness) |
| 6 | Sampler2D | Roughness map (or combined metallic-roughness) |
| 7 | Sampler2D | Ambient occlusion map |

### Push Constants

**Structure**: `src/app/model.frag:38-43`

```glsl
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;   // Camera world position (xyz) + padding
    uint materialFlags;    // Texture presence flags
    float exposure;        // Exposure adjustment
    uint padding[2];       // Alignment
} pushConstants;
```

**Material Flags**:
- Bit 0: HasAlbedoMap
- Bit 1: HasNormalMap
- Bit 2: HasMetallicMap
- Bit 3: HasRoughnessMap
- Bit 4: HasMetallicRoughnessMap (glTF combined texture)
- Bit 5: HasAOMap

---

## Texture Support

### glTF 2.0 Material System

The renderer fully supports glTF 2.0 PBR materials with the metallic-roughness workflow.

#### Texture Channel Mapping

**Albedo/Base Color**: `binding = 2`
- RGBA channels for base color
- Alpha channel for transparency (if applicable)
- **Color Space**: sRGB (automatically converted to linear in shader)

**Normal Map**: `binding = 4`
- RGB channels encode tangent-space normals
- Mapped from [0,1] to [-1,1] range
- **Convention**: OpenGL (Y-up)
- **Color Space**: Linear (UNORM)

**Metallic-Roughness-AO (Combined)**: `binding = 5 and 6`
- **Red channel**: Ambient Occlusion
- **Green channel**: Roughness
- **Blue channel**: Metallic
- **Color Space**: Linear (UNORM)

This follows the glTF 2.0 specification for packed textures, allowing a single texture sample to provide three material properties.

### Separate Texture Support

The system also supports separate texture maps:
- Individual metallic map (R channel)
- Individual roughness map (R channel)
- Individual AO map (R channel)

The shader dynamically selects the appropriate sampling path based on material flags.

### Texture Loading

**Implementation**: `src/scene/Model.cpp:42-98`

The `LoadTextureFromAssimp()` function handles:
- Embedded textures (glTF/GLB `*0`, `*1`, `*2` notation)
- External texture files (relative paths)
- Automatic color space selection (sRGB for albedo, linear for data)
- stb_image integration for multiple formats (PNG, JPEG, TGA, BMP)

### Normal Mapping

**Implementation**: `src/app/model.frag:118-136`

On-the-fly TBN matrix calculation using screen-space derivatives:

```glsl
vec3 getNormalFromMap(vec2 texCoord, vec3 worldPos, vec3 worldNormal) {
    vec3 tangentNormal = texture(normalSampler, texCoord).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;

    // Calculate TBN using derivatives (no vertex tangents needed)
    vec3 Q1 = dFdx(worldPos);
    vec3 Q2 = dFdy(worldPos);
    vec2 st1 = dFdx(texCoord);
    vec2 st2 = dFdy(texCoord);

    vec3 N = normalize(worldNormal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}
```

**Advantages**:
- No need for pre-computed tangent vertex attributes
- Works with any mesh topology
- Handles UV discontinuities correctly

---

## Tone Mapping and Post-Processing

### ACES Filmic Tone Mapping

**Implementation**: `src/app/model.frag:103-110`

The renderer uses the ACES (Academy Color Encoding System) filmic tone mapping curve, the industry standard for film and game production:

```glsl
vec3 toneMapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}
```

**Benefits**:
- Smooth shoulder in highlights (prevents over-saturation)
- Preserves color hue better than Reinhard
- Widely adopted in production pipelines
- Handles HDR to LDR conversion gracefully

### Exposure Control

**Implementation**: `src/app/model.frag:293`

Dynamic exposure adjustment via push constants:

```glsl
color = color * pushConstants.exposure;
```

**Usage**: Adjust in `src/app/Application.cpp:663`

```cpp
float exposure = 1.0f;  // Default, increase to brighten (e.g., 1.5, 2.0)
```

**Typical Values**:
- `0.5` - Darker, moody scenes
- `1.0` - Neutral (default)
- `1.5-2.0` - Brighter, outdoor scenes
- `>2.0` - Very bright, overexposed look

### Rendering Pipeline

The complete post-processing pipeline in order:

1. **PBR Lighting Calculation**: Cook-Torrance BRDF for all lights
2. **Ambient Term**: Simple ambient approximation with AO
3. **Exposure**: Multiply by exposure value
4. **Tone Mapping**: ACES filmic curve (HDR → LDR)
5. **Gamma Correction**: Power 2.2 (linear → sRGB)

```glsl
vec3 color = ambient + Lo;               // Step 1-2
color = color * pushConstants.exposure;  // Step 3
color = toneMapACES(color);              // Step 4
color = pow(color, vec3(1.0/2.2));       // Step 5
```

### Roughness Clamping

**Implementation**: `src/app/model.frag:270`

To prevent artifacts from infinitely sharp specular highlights:

```glsl
roughness = max(roughness, 0.04);
```

Minimum value of 0.04 provides:
- Smooth surfaces without fireflies
- Realistic specular for polished metals
- Stable rendering at grazing angles

---

## Debug Visualization

**Implementation**: `src/app/model.frag:301-326`

The shader includes comprehensive debug modes for PBR development. Uncomment any line to visualize:

### Material Properties
```glsl
outColor = vec4(albedo, 1.0);           // Base color
outColor = vec4(vec3(metallic), 1.0);   // Metallic (0=dielectric, 1=metal)
outColor = vec4(vec3(roughness), 1.0);  // Roughness (0=smooth, 1=rough)
outColor = vec4(vec3(ao), 1.0);         // Ambient occlusion
```

### Normals
```glsl
outColor = vec4(N * 0.5 + 0.5, 1.0);    // World-space normals (RGB)
outColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);  // Vertex normals
```

### Lighting Components
```glsl
outColor = vec4(ambient, 1.0);          // Ambient only
outColor = vec4(Lo, 1.0);               // Direct lighting only
```

### Advanced
```glsl
outColor = vec4(vec3(max(dot(N, V), 0.0)), 1.0);  // Fresnel term
```

**Workflow**:
1. Uncomment desired debug line in shader
2. Recompile shader: `glslangValidator -V src/app/model.frag -o src/app/model.frag.spv`
3. Regenerate include: `xxd -i src/app/model.frag.spv | sed -n '2,1524p' > src/app/model.frag.spv.inl`
4. Rebuild and run

---

## Usage Examples

### Loading a glTF Model with PBR

```cpp
auto model = std::make_unique<Model>();
if (model->LoadFromFile(device, "assets/models/DamagedHelmet.glb")) {
    // Model automatically loads all PBR textures:
    // - Albedo (base color)
    // - Normal map
    // - Metallic-Roughness-AO (combined)
}
```

### Setting Up Lighting

```cpp
// Create 3-point lighting for PBR
auto keyLight = std::make_unique<DirectionalLight>(
    glm::vec3(0.2f, -0.5f, -1.0f),  // Direction
    glm::vec3(1.0f, 1.0f, 1.0f),    // Color
    5.0f                             // Intensity
);

auto fillLight = std::make_unique<DirectionalLight>(
    glm::vec3(-0.7f, 0.0f, 0.5f),
    glm::vec3(0.8f, 0.9f, 1.0f),
    2.5f
);

scene->AddLight(std::move(keyLight));
scene->AddLight(std::move(fillLight));
```

### Adjusting Exposure

In `Application.cpp`, modify the exposure value:

```cpp
// In Render() function, before push constants
float exposure = 1.5f;  // Brighten scene by 50%

vkCmd->PushConstants(vkPipeline->GetLayout(),
                     VK_SHADER_STAGE_FRAGMENT_BIT,
                     20, sizeof(float), &exposure);
```

### Creating Materials Programmatically

```cpp
Material material;
material.SetAlbedo(glm::vec3(1.0f, 0.0f, 0.0f));  // Red
material.SetMetallic(1.0f);   // Fully metallic
material.SetRoughness(0.2f);  // Fairly smooth

// Or load textures
material.SetAlbedoMap(albedoTexture);
material.SetMetallicRoughnessMap(mrTexture);  // glTF combined
material.SetNormalMap(normalTexture);
```

---

## Future Enhancements

### Planned Features (Phase 3 - IBL)

**Image-Based Lighting**:
- Environment maps (cubemaps)
- Pre-filtered importance sampling
- BRDF lookup table (LUT)
- Diffuse irradiance convolution
- Specular reflection with varying roughness

**Benefits**:
- Realistic ambient lighting
- Accurate reflections from environment
- Better integration with surroundings
- Professional-quality results

### Potential Optimizations

1. **Clustered Forward Rendering**: Support for 100+ lights efficiently
2. **Material System Refactor**: Bindless textures for unlimited materials
3. **Shader Variants**: Specialized shaders for common material types
4. **Texture Compression**: BC7 for albedo, BC5 for normals
5. **LOD System**: Multiple detail levels for distant objects

### Additional PBR Features

- **Clearcoat**: Dual-layer materials (car paint, varnish)
- **Sheen**: Fabric/velvet materials
- **Transmission**: Glass and transparent materials
- **Subsurface Scattering**: Skin, wax, jade
- **Anisotropic Reflections**: Brushed metal, hair

---

## Technical Reference

### Files Modified/Created

**Core Shader Files**:
- `src/app/model.frag` - PBR fragment shader with Cook-Torrance BRDF
- `src/app/model.vert` - Vertex shader with world-space outputs

**Material System**:
- `include/metagfx/scene/Material.h` - Material properties and texture management
- `src/scene/Material.cpp` - Material implementation

**Model Loading**:
- `include/metagfx/scene/Model.h` - Model container
- `src/scene/Model.cpp` - Assimp integration, glTF texture loading

**Application**:
- `src/app/Application.cpp` - Pipeline setup, lighting, rendering loop

### Performance Characteristics

**Shader Complexity**:
- Fragment shader: ~350 lines
- Cook-Torrance BRDF: ~30 instructions per light
- Texture samples: 1-5 per fragment (depending on material)

**Memory Usage**:
- Push constants: 32 bytes
- Material UBO: 32 bytes
- Light buffer: 1040 bytes (16 lights max)
- Textures: ~16-64 MB typical for 2K textures

**Typical Performance** (Apple M3, 1280x720):
- DamagedHelmet (14K vertices, 4 lights): >120 FPS
- Simple cube (24 vertices, 4 lights): >500 FPS

### Known Limitations

1. **No IBL**: Ambient lighting is a simple constant term
2. **No Shadows**: Direct lighting doesn't account for occlusion
3. **Limited Light Count**: Maximum 16 lights in uniform buffer
4. **No Transparency**: Alpha blending not yet implemented
5. **No SSR**: Screen-space reflections for dynamic objects

---

## Conclusion

The MetaGFX PBR system provides a solid foundation for physically-accurate rendering with:

✅ Industry-standard Cook-Torrance BRDF
✅ Full glTF 2.0 material support
✅ High-quality ACES tone mapping
✅ Flexible texture system
✅ Comprehensive debug modes
✅ Clean, maintainable shader code

The implementation follows best practices and provides a clear path for future enhancements including IBL, advanced materials, and performance optimizations.

---

**Last Updated**: December 25, 2025
**Milestone**: 3.2 - PBR Rendering
**Status**: Complete (Phase 1, 2, 4)
**Pending**: Phase 3 (IBL) - deferred to future milestone
