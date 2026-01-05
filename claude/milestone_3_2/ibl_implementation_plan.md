# Milestone 3.2: IBL and Environment Maps Implementation Plan

**Date**: 2026-01-04
**Status**: Planning
**Goal**: Complete Milestone 3.2 by adding Image-Based Lighting (IBL) and environment maps to MetaGFX

---

## Executive Summary

This plan details the implementation of IBL (Image-Based Lighting) and environment maps to complete Milestone 3.2 of MetaGFX. The existing PBR implementation using Cook-Torrance BRDF is already complete and high-quality. We need to replace the placeholder ambient lighting (`vec3(0.15) * albedo * ao`) with proper environment-based lighting.

**Current Status**:
- ✅ Cook-Torrance BRDF (GGX/Schlick-GGX/Fresnel)
- ✅ Roughness, metallic, normal, AO maps
- ✅ Direct lighting (directional, point, spot)
- ✅ Tone mapping and exposure
- ❌ Image-Based Lighting (IBL)
- ❌ Environment maps / Skybox

**Infrastructure Already Available**:
- Texture system supports cubemaps (`arrayLayers = 6`) and mipmaps
- Descriptor sets easily extensible (currently using bindings 0-7)
- Complete PBR shader with proper vectors and Fresnel
- HDR tone mapping pipeline ready for HDR inputs

---

## Technical Background: What is IBL?

Image-Based Lighting uses pre-computed environment maps to provide realistic ambient lighting and reflections. Instead of calculating lighting from individual light sources, we sample pre-filtered environment textures.

**Two Components**:

1. **Diffuse IBL (Irradiance)**: Replaces ambient lighting
   - Convolves environment map over hemisphere for each texel
   - Low-frequency signal → small cubemap (64x64)
   - Provides color tint based on environment (e.g., blue from sky, green from forest)

2. **Specular IBL (Reflections)**: Provides glossy reflections
   - Pre-filters environment map for different roughness levels
   - Stored as mipchain (512x512 base, 5-6 mip levels)
   - Roughness 0.0 = mirror reflection (mip 0), roughness 1.0 = diffuse (mip 5)
   - Uses split-sum approximation with BRDF LUT

**BRDF Lookup Table (LUT)**:
- 2D texture (512x512, RG16F format)
- Pre-integrates Fresnel and geometry terms
- Axes: NdotV (x), roughness (y)
- Output: scale and bias for Fresnel (R=scale, G=bias)

---

## Implementation Phases

### Phase 1: RHI Cubemap Support
**Goal**: Extend texture system to support cubemap textures

**Files to Modify**:
- `include/metagfx/rhi/Texture.h`
- `include/metagfx/rhi/Types.h`
- `src/rhi/vulkan/VulkanTexture.h`
- `src/rhi/vulkan/VulkanTexture.cpp`

**Tasks**:

1. **Add TextureType enum** to `rhi/Types.h`:
   ```cpp
   enum class TextureType {
       Texture2D,
       TextureCube,
       Texture3D
   };
   ```

2. **Update TextureDesc** to include `TextureType type`:
   ```cpp
   struct TextureDesc {
       TextureType type = TextureType::Texture2D;
       uint32 width = 1;
       uint32 height = 1;
       uint32 depth = 1;
       uint32 mipLevels = 1;
       uint32 arrayLayers = 1;  // For cubemaps, this is 6
       Format format;
       TextureUsage usage;
       const char* debugName = nullptr;
   };
   ```

3. **VulkanTexture cubemap creation**:
   - Detect `type == TextureCube` and set `VK_IMAGE_VIEW_TYPE_CUBE`
   - Ensure `arrayLayers == 6` for cubemaps
   - Create image view with cube flag: `VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT`
   - Handle 6 faces in correct order: +X, -X, +Y, -Y, +Z, -Z

4. **Texture upload for cubemaps**:
   - Extend `UploadData()` to accept per-face data or interleaved face data
   - Handle staging buffer for all 6 faces
   - Copy each face to correct array layer

**Expected Outcome**: Ability to create cubemap textures in Vulkan backend.

---

### Phase 2: HDR Image Loading
**Goal**: Load HDR equirectangular environment maps

**Files to Modify**:
- `src/scene/Model.cpp` (texture loading utilities)
- New file: `src/scene/TextureLoader.h/.cpp` (optional refactor)

**Tasks**:

1. **Integrate stb_image_hdr**:
   - Add `#define STB_IMAGE_IMPLEMENTATION` section for HDR
   - Use `stbi_loadf()` for floating-point HDR data
   - Support `.hdr` (Radiance HDR) and `.exr` (OpenEXR via extension) formats

2. **HDR texture loading function**:
   ```cpp
   Ref<Texture> LoadHDRTexture(GraphicsDevice* device, const std::string& path);
   ```
   - Load equirectangular HDR image (typically 2:1 aspect ratio)
   - Use `Format::R16G16B16A16_FLOAT` or `R32G32B32A32_FLOAT`
   - Keep linear color space (no sRGB)

3. **Error handling**:
   - Fallback to default white HDR texture if load fails
   - Validate aspect ratio for equirectangular (should be ~2:1)

**Expected Outcome**: Ability to load `.hdr` files as floating-point textures.

---

### Phase 3: IBL Precomputation Pipeline
**Goal**: Generate irradiance map, prefiltered environment map, and BRDF LUT

This is the most complex phase. We have **two implementation options**:

#### Option A: Offline Precomputation Tool (Recommended)
**Pros**: Faster load times, no runtime overhead, easier to debug
**Cons**: Requires separate tool, adds build step

**Approach**:
- Create standalone command-line tool: `tools/ibl_precompute/`
- Input: HDR equirectangular image
- Output: 3 files (irradiance cubemap, prefiltered cubemap, BRDF LUT)
- Artists run tool once per environment map
- Runtime loads pre-baked textures directly

#### Option B: Runtime Precomputation with Compute Shaders
**Pros**: No offline step, dynamic environment maps possible
**Cons**: Slower load times, more complex RHI code (compute pipeline)

**Approach**:
- Use Vulkan compute shaders during initialization
- Precompute on first load, cache results to disk
- Requires compute pipeline support in RHI (not yet implemented)

**Recommendation**: Start with **Option A** (offline tool). We can add Option B later if dynamic environments are needed.

---

#### Phase 3A: Equirectangular to Cubemap Conversion

**Shader**: `tools/ibl_precompute/shaders/equirect_to_cube.comp`

**Algorithm**:
```glsl
// For each cubemap face and texel
vec3 direction = getCubemapDirection(faceIndex, uv);
vec2 equirectUV = directionToEquirectangular(direction);
vec3 color = texture(equirectMap, equirectUV).rgb;
imageStore(outputCube, ivec3(x, y, face), vec4(color, 1.0));
```

**Output**: Base environment cubemap (e.g., 1024x1024, 6 faces, HDR format)

**C++ Tool Structure**:
```cpp
// tools/ibl_precompute/main.cpp
int main(int argc, char** argv) {
    // Load HDR equirectangular
    auto hdrImage = LoadHDRTexture(inputPath);

    // Convert to cubemap
    auto envCubemap = ConvertEquirectToCubemap(hdrImage, 1024);

    // Generate irradiance map
    auto irradianceMap = GenerateIrradianceMap(envCubemap, 64);

    // Generate prefiltered map
    auto prefilteredMap = GeneratePrefilteredMap(envCubemap, 512, 6);

    // Generate BRDF LUT
    auto brdfLUT = GenerateBRDFLUT(512);

    // Save to disk
    SaveCubemap("output_env.dds", envCubemap);
    SaveCubemap("output_irradiance.dds", irradianceMap);
    SaveCubemap("output_prefiltered.dds", prefilteredMap);
    SaveTexture2D("output_brdf_lut.dds", brdfLUT);

    return 0;
}
```

---

#### Phase 3B: Irradiance Map Generation

**Shader**: `tools/ibl_precompute/shaders/irradiance_convolution.comp`

**Algorithm** (Diffuse Convolution):
```glsl
// For each texel in irradiance cubemap (64x64 per face)
vec3 normal = getCubemapDirection(face, uv);
vec3 irradiance = vec3(0.0);

// Convolve over hemisphere
for (float phi = 0.0; phi < 2.0 * PI; phi += deltaPhi) {
    for (float theta = 0.0; theta < 0.5 * PI; theta += deltaTheta) {
        vec3 sampleDir = sphericalToCartesian(phi, theta, normal);
        vec3 envColor = texture(envCubemap, sampleDir).rgb;
        irradiance += envColor * cos(theta) * sin(theta);
    }
}
irradiance *= PI / float(sampleCount);

imageStore(irradianceCube, ivec3(x, y, face), vec4(irradiance, 1.0));
```

**Parameters**:
- Output resolution: 64x64 per face (diffuse is low-frequency)
- Sample count: ~1024 samples per texel (can be lower for faster precompute)
- Format: `R16G16B16A16_FLOAT`

**Expected Outcome**: Small cubemap with diffuse ambient lighting per direction.

---

#### Phase 3C: Prefiltered Environment Map

**Shader**: `tools/ibl_precompute/shaders/prefilter_env.comp`

**Algorithm** (Importance Sampling GGX):
```glsl
// For each mip level (0 to 5)
float roughness = float(mipLevel) / float(maxMipLevel);

// For each texel
vec3 normal = getCubemapDirection(face, uv);
vec3 reflection = normal;  // View = reflection for prefiltering

vec3 prefilteredColor = vec3(0.0);
float totalWeight = 0.0;

// Importance sample GGX distribution
for (uint i = 0; i < SAMPLE_COUNT; ++i) {
    vec2 Xi = Hammersley(i, SAMPLE_COUNT);
    vec3 H = ImportanceSampleGGX(Xi, normal, roughness);
    vec3 L = reflect(-reflection, H);

    float NdotL = max(dot(normal, L), 0.0);
    if (NdotL > 0.0) {
        vec3 envColor = textureLod(envCubemap, L, 0).rgb;
        prefilteredColor += envColor * NdotL;
        totalWeight += NdotL;
    }
}

prefilteredColor /= totalWeight;
imageStore(prefilteredCube, ivec3(x, y, face), vec4(prefilteredColor, 1.0));
```

**Parameters**:
- Base resolution: 512x512 per face
- Mip levels: 6 (roughness 0.0, 0.2, 0.4, 0.6, 0.8, 1.0)
- Sample count: 1024 samples per texel (adjust for quality vs speed)
- Format: `R16G16B16A16_FLOAT`

**Key Functions Needed**:
- `Hammersley(i, N)`: Low-discrepancy sequence
- `ImportanceSampleGGX(Xi, N, roughness)`: Sample half-vector based on GGX lobe
- Uses existing GGX NDF from main PBR shader

**Expected Outcome**: Cubemap with mipchain, each mip = prefiltered for specific roughness.

---

#### Phase 3D: BRDF Integration Map

**Shader**: `tools/ibl_precompute/shaders/brdf_lut.comp`

**Algorithm** (Split-Sum Approximation):
```glsl
// For each texel (NdotV on X, roughness on Y)
float NdotV = uv.x;
float roughness = uv.y;

vec3 V = vec3(sqrt(1.0 - NdotV*NdotV), 0.0, NdotV);  // View in tangent space
vec3 N = vec3(0.0, 0.0, 1.0);

float scale = 0.0;
float bias = 0.0;

for (uint i = 0; i < SAMPLE_COUNT; ++i) {
    vec2 Xi = Hammersley(i, SAMPLE_COUNT);
    vec3 H = ImportanceSampleGGX(Xi, N, roughness);
    vec3 L = reflect(-V, H);

    float NdotL = max(L.z, 0.0);
    float NdotH = max(H.z, 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotL > 0.0) {
        float G = GeometrySmith(N, V, L, roughness);
        float G_Vis = (G * VdotH) / (NdotH * NdotV);
        float Fc = pow(1.0 - VdotH, 5.0);

        scale += (1.0 - Fc) * G_Vis;
        bias += Fc * G_Vis;
    }
}

scale /= float(SAMPLE_COUNT);
bias /= float(SAMPLE_COUNT);

imageStore(brdfLUT, ivec2(x, y), vec4(scale, bias, 0.0, 1.0));
```

**Parameters**:
- Resolution: 512x512
- Format: `R16G16_FLOAT` (only need 2 channels)
- Sample count: 1024

**Expected Outcome**: 2D texture mapping (NdotV, roughness) → (Fresnel scale, bias).

---

#### Phase 3E: File Format for Baked Textures

**Options**:
1. **Custom binary format** (simple, fast to load)
2. **DDS format** (industry standard, supports cubemaps and mipmaps)
3. **KTX format** (Khronos standard, Vulkan-friendly)

**Recommendation**: Use **DDS format**:
- Well-documented, widely supported
- Natively supports cubemaps and mipchains
- Libraries available (stb_dds_write, DirectXTex)
- Can store HDR formats (R16G16B16A16_FLOAT)

**File Naming Convention**:
```
assets/envmaps/studio/
├── studio_env.dds          // Base environment (optional, for skybox)
├── studio_irradiance.dds   // Irradiance map
├── studio_prefiltered.dds  // Prefiltered environment map
└── studio_brdf_lut.dds     // BRDF LUT (shared across all environments)
```

**Note**: BRDF LUT is environment-independent, only needs to be generated once.

---

### Phase 4: Shader Integration
**Goal**: Use IBL textures in fragment shader for ambient lighting and reflections

**Files to Modify**:
- `src/app/model.frag`
- `src/app/model.vert` (minimal, possibly pass tangent vectors if needed)

**Tasks**:

1. **Add new descriptor bindings** (after binding 7):
   ```glsl
   layout(set = 0, binding = 8) uniform samplerCube u_IrradianceMap;
   layout(set = 0, binding = 9) uniform samplerCube u_PrefilteredMap;
   layout(set = 0, binding = 10) uniform sampler2D u_BRDF_LUT;
   ```

2. **IBL Diffuse Calculation** (replace ambient):
   ```glsl
   // Current (line 311):
   // vec3 ambient = vec3(0.15) * albedo * ao;

   // New IBL diffuse:
   vec3 N = normalize(fragNormal);  // Already computed
   vec3 irradiance = texture(u_IrradianceMap, N).rgb;
   vec3 diffuseIBL = irradiance * albedo * ao;
   ```

3. **IBL Specular Calculation** (add after direct lighting):
   ```glsl
   vec3 V = normalize(u_CameraPos.xyz - fragPosition);
   vec3 R = reflect(-V, N);

   // Sample prefiltered map based on roughness
   const float MAX_REFLECTION_LOD = 5.0;  // Number of mip levels - 1
   float lod = roughness * MAX_REFLECTION_LOD;
   vec3 prefilteredColor = textureLod(u_PrefilteredMap, R, lod).rgb;

   // Sample BRDF LUT
   float NdotV = max(dot(N, V), 0.0);
   vec2 brdf = texture(u_BRDF_LUT, vec2(NdotV, roughness)).rg;

   // Combine using split-sum approximation
   vec3 F0 = mix(vec3(0.04), albedo, metallic);  // Already computed
   vec3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);
   ```

4. **Combine IBL with direct lighting**:
   ```glsl
   // Direct lighting (existing)
   vec3 Lo = vec3(0.0);
   for (int i = 0; i < lightCount; ++i) {
       Lo += CalculateLight(lights[i], ...);
   }

   // IBL (new)
   vec3 ambient = diffuseIBL + specularIBL;

   // Final color
   vec3 color = ambient + Lo;

   // Tone mapping (existing)
   color = ACESFilm(color * u_Exposure);
   color = pow(color, vec3(1.0/2.2));

   fragColor = vec4(color, 1.0);
   ```

5. **Optional: IBL intensity control**:
   Add to push constants:
   ```glsl
   layout(push_constant) uniform PushConstants {
       // ... existing fields ...
       float iblIntensity;  // Multiplier for IBL contribution
   } u_PushConstants;
   ```

**Expected Outcome**: Realistic ambient lighting and reflections from environment.

---

### Phase 5: Application Integration
**Goal**: Load and bind IBL textures in main application

**Files to Modify**:
- `src/app/Application.h`
- `src/app/Application.cpp`

**Tasks**:

1. **Add member variables**:
   ```cpp
   class Application {
       // ... existing members ...

       // IBL textures
       Ref<Texture> m_IrradianceMap;
       Ref<Texture> m_PrefilteredMap;
       Ref<Texture> m_BRDF_LUT;
       Ref<Sampler> m_CubemapSampler;
       Ref<Sampler> m_BRDF_Sampler;
   };
   ```

2. **Load IBL textures in Initialize()**:
   ```cpp
   // Load IBL textures (after generating with offline tool)
   m_IrradianceMap = LoadCubemapDDS(m_Device, "assets/envmaps/studio/studio_irradiance.dds");
   m_PrefilteredMap = LoadCubemapDDS(m_Device, "assets/envmaps/studio/studio_prefiltered.dds");
   m_BRDF_LUT = LoadTexture2DDDS(m_Device, "assets/envmaps/studio_brdf_lut.dds");
   ```

3. **Create samplers**:
   ```cpp
   // Cubemap sampler (linear filtering for smooth reflections)
   SamplerDesc cubemapSamplerDesc{};
   cubemapSamplerDesc.minFilter = Filter::Linear;
   cubemapSamplerDesc.magFilter = Filter::Linear;
   cubemapSamplerDesc.mipmapMode = SamplerMipmapMode::Linear;  // Important for prefiltered map
   cubemapSamplerDesc.addressModeU = SamplerAddressMode::ClampToEdge;
   cubemapSamplerDesc.addressModeV = SamplerAddressMode::ClampToEdge;
   cubemapSamplerDesc.addressModeW = SamplerAddressMode::ClampToEdge;
   m_CubemapSampler = m_Device->CreateSampler(cubemapSamplerDesc);

   // BRDF LUT sampler (clamp to edge, linear)
   SamplerDesc brdfSamplerDesc{};
   brdfSamplerDesc.minFilter = Filter::Linear;
   brdfSamplerDesc.magFilter = Filter::Linear;
   brdfSamplerDesc.addressModeU = SamplerAddressMode::ClampToEdge;
   brdfSamplerDesc.addressModeV = SamplerAddressMode::ClampToEdge;
   m_BRDF_Sampler = m_Device->CreateSampler(brdfSamplerDesc);
   ```

4. **Update descriptor set layout** (in CreateDescriptorSet()):
   ```cpp
   // Add 3 new bindings
   descriptorBindings.push_back({8, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment});
   descriptorBindings.push_back({9, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment});
   descriptorBindings.push_back({10, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment});
   ```

5. **Bind IBL textures to descriptor set**:
   ```cpp
   m_DescriptorSet->UpdateTexture(8, m_IrradianceMap, m_CubemapSampler);
   m_DescriptorSet->UpdateTexture(9, m_PrefilteredMap, m_CubemapSampler);
   m_DescriptorSet->UpdateTexture(10, m_BRDF_LUT, m_BRDF_Sampler);
   ```

6. **Handle missing textures gracefully**:
   - Create default white cubemap if IBL textures not found
   - Log warning but don't crash

**Expected Outcome**: Application loads IBL textures and uses them for lighting.

---

### Phase 6: Skybox Rendering (Optional)
**Goal**: Render environment map as background skybox

**Why Optional**: IBL works without visible skybox. Skybox is primarily for visual reference and aesthetic.

**Files to Create**:
- `src/app/skybox.vert`
- `src/app/skybox.frag`
- Add skybox rendering pass to `Application.cpp`

**Tasks**:

1. **Skybox vertex shader**:
   ```glsl
   #version 450

   layout(location = 0) in vec3 a_Position;

   layout(set = 0, binding = 0) uniform CameraUBO {
       mat4 view;
       mat4 projection;
   } u_Camera;

   layout(location = 0) out vec3 v_TexCoord;

   void main() {
       v_TexCoord = a_Position;

       // Remove translation from view matrix
       mat4 viewNoTranslation = mat4(mat3(u_Camera.view));
       vec4 pos = u_Camera.projection * viewNoTranslation * vec4(a_Position, 1.0);

       // Set depth to 1.0 (far plane)
       gl_Position = pos.xyww;  // Trick: z/w = w/w = 1.0
   }
   ```

2. **Skybox fragment shader**:
   ```glsl
   #version 450

   layout(location = 0) in vec3 v_TexCoord;

   layout(set = 0, binding = 1) uniform samplerCube u_Skybox;

   layout(push_constant) uniform PushConstants {
       float exposure;
   } u_PushConstants;

   layout(location = 0) out vec4 fragColor;

   void main() {
       vec3 envColor = texture(u_Skybox, v_TexCoord).rgb;

       // Apply exposure (match main shader)
       envColor *= u_PushConstants.exposure;

       // Tone mapping (simple, or reuse ACES from main shader)
       envColor = envColor / (envColor + vec3(1.0));  // Reinhard
       envColor = pow(envColor, vec3(1.0/2.2));       // Gamma

       fragColor = vec4(envColor, 1.0);
   }
   ```

3. **Skybox geometry**:
   - Cube mesh with positions [-1, 1]^3
   - 36 vertices (or 8 vertices + 36 indices)

4. **Rendering pass**:
   ```cpp
   void Application::RenderSkybox(Ref<CommandBuffer> cmd) {
       cmd->BindPipeline(m_SkyboxPipeline);
       cmd->BindDescriptorSets(m_SkyboxPipelineLayout, m_SkyboxDescriptorSet);
       cmd->BindVertexBuffer(m_SkyboxVertexBuffer, 0);
       cmd->Draw(36, 1, 0, 0);
   }
   ```

5. **Integration in Render()**:
   ```cpp
   cmd->BeginRendering(...);

   // Render skybox FIRST (writes depth = 1.0)
   RenderSkybox(cmd);

   // Render scene geometry (depth test LESS_OR_EQUAL)
   RenderScene(cmd);

   cmd->EndRendering();
   ```

**Expected Outcome**: Visible environment map in background, matches IBL lighting.

---

## Testing and Validation

### Test Cases

1. **Metallic Sphere Test**:
   - Render sphere with metallic = 1.0, roughness = 0.0
   - Should show clear environment reflections
   - Check that reflection direction matches view angle

2. **Roughness Gradient Test**:
   - Render 5 spheres with roughness 0.0, 0.25, 0.5, 0.75, 1.0
   - Verify smooth transition from sharp to blurry reflections
   - Roughness 1.0 should match diffuse irradiance

3. **Metallic Gradient Test**:
   - Render spheres with metallic 0.0 to 1.0
   - Non-metallic should show diffuse IBL dominance
   - Metallic should show specular IBL dominance

4. **Energy Conservation Test**:
   - IBL contribution should not exceed input environment intensity
   - Check with pure white environment map (all 1.0)

5. **BRDF LUT Validation**:
   - NdotV = 0 should give low scale, high bias
   - NdotV = 1 should give high scale, low bias
   - Roughness = 0 should have sharp Fresnel falloff
   - Roughness = 1 should be smooth

6. **Comparison Test**:
   - Load known reference scene (e.g., glTF sample models)
   - Compare with Babylon.js, Three.js, or Filament renderer
   - Verify similar appearance

### Performance Metrics

- **Offline precomputation time**: Should complete in < 30 seconds for one environment
- **Load time**: IBL textures should load in < 200ms
- **Runtime overhead**: IBL adds ~3 texture samples per fragment (negligible)
- **Memory usage**: ~20-30 MB per environment set (irradiance + prefiltered + BRDF)

---

## File Structure After Implementation

```
metagfx/
├── assets/
│   └── envmaps/                      # Environment maps
│       ├── studio/
│       │   ├── studio_irradiance.dds
│       │   ├── studio_prefiltered.dds
│       │   └── studio_env.hdr        # Source HDR (for reference)
│       ├── outdoor/
│       │   ├── outdoor_irradiance.dds
│       │   └── outdoor_prefiltered.dds
│       └── brdf_lut.dds              # Shared BRDF LUT
│
├── tools/
│   └── ibl_precompute/               # Offline precomputation tool
│       ├── CMakeLists.txt
│       ├── main.cpp
│       ├── shaders/
│       │   ├── equirect_to_cube.comp
│       │   ├── irradiance_convolution.comp
│       │   ├── prefilter_env.comp
│       │   └── brdf_lut.comp
│       └── README.md                 # How to use the tool
│
├── src/
│   ├── app/
│   │   ├── model.frag                # Updated with IBL
│   │   ├── skybox.vert               # New
│   │   ├── skybox.frag               # New
│   │   └── Application.cpp           # IBL texture loading and binding
│   │
│   ├── rhi/
│   │   └── vulkan/
│   │       └── VulkanTexture.cpp     # Cubemap support
│   │
│   └── scene/
│       ├── TextureLoader.cpp         # HDR and DDS loading (new/refactored)
│       └── Material.h                # Possibly add IBL intensity parameter
│
├── docs/
│   └── ibl_implementation.md         # Technical documentation (this gets created after)
│
└── claude/
    └── milestone_3_2/
        ├── ibl_implementation_plan.md  # This document
        └── ibl_completion_notes.md     # After implementation
```

---

## Dependencies and Libraries

### Already Available:
- ✅ stb_image (can load .hdr with `stbi_loadf`)
- ✅ Vulkan SDK (compute shaders, cubemap support)
- ✅ GLM (vector/matrix math)

### Need to Add:
- **DDS loading/writing library** (choose one):
  - Option 1: `stb_dds` (simple, header-only, limited format support)
  - Option 2: DirectXTex (full-featured, Windows-focused, large)
  - Option 3: Custom DDS reader/writer (500 lines, full control)
  - **Recommendation**: Custom DDS reader for loading, simple binary writer for tool

- **Compute shader support in RHI**:
  - Not needed if using offline tool (recommended)
  - Required only for runtime precomputation

---

## Implementation Order (Recommended)

### Week 1: Core Infrastructure
1. ✅ Phase 1: Cubemap support in VulkanTexture
2. ✅ Phase 2: HDR image loading with stb_image
3. ✅ Add DDS file loader (read cubemaps, mipchains)

### Week 2: Precomputation Tool
4. ✅ Phase 3A: Equirectangular to cubemap conversion
5. ✅ Phase 3B: Irradiance map generation
6. ✅ Phase 3C: Prefiltered environment map
7. ✅ Phase 3D: BRDF LUT generation
8. ✅ Test tool with sample HDR environment

### Week 3: Shader and Application Integration
9. ✅ Phase 4: Update model.frag with IBL calculations
10. ✅ Phase 5: Application.cpp IBL texture loading and binding
11. ✅ Test with sample models (spheres, complex geometry)
12. ✅ Debug and validate against reference

### Week 4: Polish and Optional Features
13. ✅ Phase 6: Skybox rendering (optional)
14. ✅ Add multiple environment maps to assets
15. ✅ UI for switching environments (if ImGui available)
16. ✅ Documentation and completion notes

---

## Risks and Mitigation

### Risk 1: Cubemap Texture Support Complexity
**Impact**: High
**Probability**: Medium
**Mitigation**: Start with simple 2D cubemap face loading before full DDS integration. Use Vulkan validation layers extensively.

### Risk 2: Precomputation Quality vs Performance
**Impact**: Medium
**Probability**: Medium
**Mitigation**: Make sample counts configurable. Provide "fast" and "quality" presets. Reference Epic's UE4 IBL parameters.

### Risk 3: BRDF LUT Artifacts
**Impact**: Medium
**Probability**: Low
**Mitigation**: Use well-tested importance sampling code from LearnOpenGL or Google Filament. Validate LUT visually (should be smooth gradient).

### Risk 4: File Format Compatibility
**Impact**: Low
**Probability**: Low
**Mitigation**: If DDS proves difficult, fall back to custom binary format. Save raw float arrays with metadata header.

---

## Success Criteria

✅ **Milestone 3.2 Complete When**:
1. IBL diffuse lighting replaces hardcoded ambient (`vec3(0.15)`)
2. IBL specular reflections work for varying roughness (0.0 to 1.0)
3. Metallic surfaces show clear environment reflections
4. Offline precomputation tool successfully processes HDR → IBL textures
5. At least 2 environment maps included in assets
6. Skybox rendering functional (optional, but recommended for visual validation)
7. No visual artifacts (seams on cubemap faces, incorrect reflections, energy loss)
8. Performance: 60 FPS on reference hardware (RTX 2060 or equivalent)
9. Documentation updated with IBL usage

---

## References and Resources

### Theory and Math:
- **Real Shading in Unreal Engine 4** (Brian Karis, SIGGRAPH 2013)
  - Split-sum approximation
  - Importance sampling GGX
  - BRDF integration
  - https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf

- **Physically Based Shading at Disney** (Brent Burley, SIGGRAPH 2012)
  - Roughness/metallic workflow justification
  - Artist-friendly parameters

- **LearnOpenGL - PBR Theory**
  - https://learnopengl.com/PBR/Theory
  - https://learnopengl.com/PBR/IBL/Diffuse-irradiance
  - https://learnopengl.com/PBR/IBL/Specular-IBL

### Implementation References:
- **Google Filament** (Production-quality PBR renderer)
  - https://github.com/google/filament
  - `shaders/src/light_indirect.fs` (IBL implementation)
  - `tools/cmgen/` (IBL precomputation tool, excellent reference)

- **Khronos glTF Sample Viewer**
  - https://github.com/KhronosGroup/glTF-Sample-Viewer
  - Reference WebGL implementation of IBL

### Sample Assets:
- **Poly Haven** (CC0 HDR environment maps)
  - https://polyhaven.com/hdris
  - High-quality, free HDRIs in various resolutions

- **HDRI Haven** (merged into Poly Haven)
  - Outdoor, studio, indoor environments

- **sIBL Archive** (legacy, but good test cases)
  - http://www.hdrlabs.com/sibl/archive.html

---

## Open Questions

1. **Do we want runtime environment switching in the GUI?**
   - Requires UI integration (ImGui already available per roadmap)
   - Adds complexity but great for demos/debugging

2. **Should we support dynamic environment maps (e.g., from game world)?**
   - Would require runtime precomputation (compute shaders)
   - Defer to Phase 4+ of roadmap?

3. **Skybox: Use environment map or separate texture?**
   - Option A: Use prefiltered map mip 0 (matches reflections perfectly)
   - Option B: Use original HDR (higher quality, but requires extra texture)
   - **Recommendation**: Option A (use prefiltered map)

4. **Multiple environment maps per scene?**
   - glTF 2.0 extension supports per-material environment maps
   - Adds complexity, defer to later milestone?
   - **Recommendation**: Single global environment for Milestone 3.2

---

## Conclusion

This implementation plan provides a structured path to completing Milestone 3.2 by adding IBL and environment maps to MetaGFX. The existing PBR infrastructure is solid, and the codebase is well-architected for this addition.

**Key Decisions**:
- ✅ Use offline precomputation tool (simpler, faster runtime)
- ✅ DDS file format for baked textures
- ✅ 3 texture bindings: irradiance, prefiltered, BRDF LUT
- ✅ Optional skybox rendering for visual validation

**Estimated Effort**: 3-4 weeks for full implementation and testing.

**Next Steps**:
1. Review and approve this plan
2. Begin Phase 1 (cubemap support in RHI)
3. Prototype precomputation tool with simple equirect→cube conversion
4. Iterate on quality and integration

---

*This plan will be updated as implementation progresses. See `claude/milestone_3_2/ibl_completion_notes.md` for final results.*
