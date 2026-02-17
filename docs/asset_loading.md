# Asset Loading System

**Status**: ✅ Implemented (Milestones 2.1, 2.3, 4.3, 5.1, 5.2)
**Last Updated**: February 2026

This document covers all asset loading in MetaGFX: 3D models via Assimp, PBRT v4 scene
files via the built-in parser, and image textures via stb_image.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Texture Loading](#texture-loading)
3. [Assimp Model Loading](#assimp-model-loading)
4. [PBRT v4 Scene Loading](#pbrt-v4-scene-loading)
5. [Mesh and Model Classes](#mesh-and-model-classes)
6. [Material Extraction](#material-extraction)
7. [Level of Detail (LOD)](#level-of-detail-lod)
8. [Instanced Rendering](#instanced-rendering)
9. [Bounding Volumes](#bounding-volumes)
10. [Error Handling and Fallbacks](#error-handling-and-fallbacks)
11. [HDR Environment Map Loading (TextureUtils)](#hdr-environment-map-loading-textureutils)

---

## Architecture Overview

```
                        ┌───────────────────────────────────────────┐
                        │          Application::LoadModel()         │
                        │  Detects file extension (.pbrt vs others) │
                        └────────────────┬──────────────────────────┘
                                         │
                   ┌─────────────────────┼─────────────────────┐
                   │                     │                     │
         ┌─────────▼─────────┐  ┌────────▼────────┐  ┌────────▼────────┐
         │  PbrtLoader       │  │  Model::        │  │  TextureUtils   │
         │  (PBRT v4 scenes) │  │  LoadFromFile() │  │  (image files)  │
         │  .pbrt            │  │  (Assimp-based) │  │  PNG/JPEG/etc.  │
         └─────────┬─────────┘  └────────┬────────┘  └────────┬────────┘
                   │                     │                     │
                   └─────────────────────┼─────────────────────┘
                                         │
                        ┌────────────────▼──────────────────────────┐
                        │             Model (scene graph)            │
                        │   vector<unique_ptr<Mesh>>                │
                        │   Each Mesh: vertices + indices + Material │
                        └────────────────┬──────────────────────────┘
                                         │
                        ┌────────────────▼──────────────────────────┐
                        │        RHI (Buffer / Texture creation)     │
                        │  Vertex/index buffers, texture uploads     │
                        └───────────────────────────────────────────┘
```

All asset loading produces `Model` objects composed of `Mesh` objects. The path through the
loading pipeline depends on the file extension:

| Extension | Loader | Detail |
|-----------|--------|--------|
| `.glb`, `.gltf` | Assimp | glTF 2.0 including embedded textures |
| `.obj` | Assimp | Wavefront OBJ with `.mtl` sidecar |
| `.fbx` | Assimp | Autodesk FBX |
| `.dae` | Assimp | COLLADA |
| `.pbrt` | PbrtParser | PBRT v4 scene file with `Include` support |

---

## Texture Loading

### Image loading pipeline

```
Disk (PNG / JPEG / TGA / BMP)
        │
        ▼
  TextureUtils::LoadImage()
  ├── stb_image::stbi_load()            ← CPU decode, RGBA8 output
  └── returns ImageData { pixels, w, h, channels }
        │
        ▼
  TextureUtils::CreateTextureFromImage()
  ├── device->CreateTexture(TextureDesc)  ← allocate GPU memory
  ├── texture->CopyData(pixels, ...)       ← transfer CPU→GPU
  └── returns Ref<rhi::Texture>
```

**Location**: `include/metagfx/utils/TextureUtils.h`, `src/utils/TextureUtils.cpp`

### stb_image

stb_image (`external/stb/stb_image.h`) loads PNG, JPEG, TGA, and BMP files.  Output is
always forced to 4 channels (RGBA8) for simplicity and GPU alignment:

```cpp
int w, h, channels;
uint8_t* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4 /*force RGBA*/);
```

Failure (file not found, corrupt data) returns `nullptr`. The caller logs the error and
falls back to a default texture.

### Supported formats

| Format | Notes |
|--------|-------|
| PNG | Fully supported, including transparency |
| JPEG | No alpha channel; treated as RGB |
| TGA | Legacy format used by some PBRT scenes |
| BMP | 24-bit uncompressed |

### Texture descriptor

```cpp
rhi::TextureDesc desc{};
desc.width   = imageData.width;
desc.height  = imageData.height;
desc.format  = rhi::Format::R8G8B8A8_SRGB;  // colour textures
desc.usage   = rhi::TextureUsage::Sampled | rhi::TextureUsage::TransferDst;
desc.mipLevels = 1;  // no runtime mip generation yet
```

Depth textures and HDR cubemaps use separate descriptors; see
[textures_and_samplers.md](textures_and_samplers.md) for GPU-side details.

### Default / fallback textures

Several default textures are created at startup and used when a mesh has no texture assigned
for a given slot:

| Member | Appearance | Used for |
|--------|-----------|---------|
| `m_DefaultTexture` | 128×128 magenta/white checkerboard | Missing albedo |
| `m_DefaultNormalMap` | Solid (0.5, 0.5, 1.0) flat blue | Missing normal map |
| `m_DefaultWhiteTexture` | Solid white 1×1 | Missing metallic/roughness/AO |
| `m_DefaultBlackTexture` | Solid black 1×1 | Missing emissive |

The checkerboard pattern makes missing textures immediately visible during development.

### Texture coordinate conventions

- Assimp applies `aiProcess_FlipUVs`, converting from OpenGL (bottom-left origin) to
  Vulkan/Metal/WebGPU (top-left origin).
- PBRT scenes use top-left origin natively; no flip is applied.

---

## Assimp Model Loading

### Supported formats

| Format | Extension | Notes |
|--------|-----------|-------|
| glTF 2.0 | `.glb`, `.gltf` | PBR materials, embedded textures |
| Wavefront | `.obj` | External `.mtl` sidecar |
| FBX | `.fbx` | Autodesk scene format |
| COLLADA | `.dae` | Open exchange format |

Importers are selectively enabled in `external/CMakeLists.txt` to minimise compile time:

```cmake
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_OBJ_IMPORTER     ON)
set(ASSIMP_BUILD_FBX_IMPORTER     ON)
set(ASSIMP_BUILD_GLTF_IMPORTER    ON)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON)
```

### Post-processing flags

```cpp
unsigned int flags =
    aiProcess_Triangulate           |  // all primitives → triangles
    aiProcess_FlipUVs               |  // UV origin to top-left
    aiProcess_GenNormals            |  // auto-generate if absent
    aiProcess_CalcTangentSpace      |  // tangents for normal mapping
    aiProcess_JoinIdenticalVertices;   // de-duplicate vertices
```

### Scene graph traversal

Assimp scenes are hierarchical; the loader recursively descends the node tree:

```cpp
void ProcessNode(aiNode* node, const aiScene* scene) {
    for (uint32_t i = 0; i < node->mNumMeshes; ++i)
        ProcessMesh(scene->mMeshes[node->mMeshes[i]], scene);

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
        ProcessNode(node->mChildren[i], scene);
}
```

### Vertex extraction

Each Assimp mesh produces one `Mesh` with a unified `Vertex` layout:

```cpp
struct Vertex {
    glm::vec3 position;  // aiMesh->mVertices[i]
    glm::vec3 normal;    // aiMesh->mNormals[i]  (or {0,1,0} if absent)
    glm::vec2 texCoord;  // aiMesh->mTextureCoords[0][i].xy (or {0,0})
};
```

### Material extraction (Assimp)

Assimp materials are converted to `Material` objects. The extractor queries aiMaterial
properties and texture paths:

```cpp
// Base colour
aiColor4D diffuse;
aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse);

// Roughness / metallic (PBR materials)
float roughness = 0.5f, metallic = 0.0f;
aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness);
aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallic);

// Albedo texture
aiString texPath;
if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
    material.SetAlbedoMap(LoadTexture(texPath.C_Str(), modelDir));
```

Texture paths are resolved relative to the model file's directory.

---

## PBRT v4 Scene Loading

### Overview

PBRT v4 scene files (`.pbrt`) are loaded by a self-contained parser module in
`src/scene/pbrt/`. No third-party PBRT library is used.

**Location**:
```
include/metagfx/scene/pbrt/
  PbrtLexer.h      ← tokeniser
  PbrtParser.h     ← parser + intermediate types
  PbrtLoader.h     ← public API

src/scene/pbrt/
  PbrtLexer.cpp
  PbrtParser.cpp
  PbrtLoader.cpp
```

### Public API

```cpp
#include "metagfx/scene/pbrt/PbrtLoader.h"

struct PbrtLoadResult {
    std::unique_ptr<Model> model;    // all scene meshes
    PbrtCameraParams       camera;   // camera.defined == true if LookAt/Camera found
    // Lights are added directly to the Scene* passed to Load()
};

PbrtLoadResult result = PbrtLoader::Load(device, "scene.pbrt", scene);
```

### Lexer

`PbrtLexer` tokenises PBRT text into a flat token stream.  It maintains a **file stack**
so `Include` directives are handled transparently:

| Input | Token type |
|-------|-----------|
| `# …\n` | Skipped (comment) |
| `"…"` | `String` (quotes stripped) |
| `[` `]` `{` `}` | `LBracket` `RBracket` `LBrace` `RBrace` |
| Letter-start word | `Ident` (`WorldBegin`, `Shape`, …) |
| Digit / `-` / `.` | `Number` (stored as raw string) |

`PushFile(path)` pushes a new file state onto the stack. When EOF is reached the lexer
pops back to the parent file transparently.

### Parser directives

#### Global section (before `WorldBegin`)

| Directive | Action |
|-----------|--------|
| `LookAt` | 9 floats → eye, look, up |
| `Camera "perspective"` | Extracts `fov` from params |
| `Transform [16 floats]` | Set CTM (column-major) |
| `Translate x y z` | CTM = CTM × T |
| `Rotate a ax ay az` | CTM = CTM × R |
| `Scale x y z` | CTM = CTM × S |
| `ConcatTransform [16]` | CTM = CTM × M |
| `Identity` | CTM = I |
| `Film`, `Sampler`, `Integrator`, `Accelerator` | Parsed, discarded |
| `Include "path"` | Lexer pushes nested file |
| `WorldBegin` | Enters world section |

#### World section (`WorldBegin` … `WorldEnd`)

| Directive | Action |
|-----------|--------|
| `AttributeBegin` / `{` | Push graphics state copy |
| `AttributeEnd` / `}` | Pop graphics state |
| `TransformBegin` | Push graphics state (transform scope only) |
| `TransformEnd` | Pop graphics state |
| Transform directives | Update CTM |
| `Material "type" [params]` | Build material, store in active state |
| `MakeNamedMaterial "name" [params]` | Build material, store in global map |
| `NamedMaterial "name"` | `state.material = m_NamedMaterials[name]` |
| `Texture "name" "spectrum"/"rgb" "imagemap" [params]` | Load colour image → `m_NamedTextures` |
| `Texture "name" "float" "imagemap" [params]` | Load greyscale image → `m_NamedFloatTextures` |
| `Texture "name" "..." "scale" [params]` | Pass-through: alias to base texture (float scale textures skipped) |
| `Shape "trianglemesh"` | Build inline triangle mesh |
| `Shape "plymesh"` | Load external `.ply` via Assimp |
| `Shape "sphere"` | Generate UV sphere triangle mesh |
| `Shape "disk"` | Generate circular fan triangle mesh |
| `LightSource "distant"` | Add `DirectionalLight` to result |
| `LightSource "point"` | Add `PointLight` to result |
| `LightSource "spot"` | Add `SpotLight` to result |
| `LightSource "infinite"` | Record environment map path for IBL setup |
| `AreaLightSource "diffuse"` | Extract emission colour, used for emissive meshes |
| `ReverseOrientation` | Flip normal winding in state |
| `ObjectBegin "name"` | Begin object definition (push state, buffer meshes) |
| `ObjectEnd` | End object definition (store buffered meshes by name) |
| `ObjectInstance "name"` | Clone stored meshes with current CTM into scene |
| `WorldEnd` | Finish parsing |

### Shape loading

#### TriangleMesh (inline geometry)

Positions, normals, and UVs are provided directly as parameter arrays:

```
Shape "trianglemesh"
    "point3 P" [ x0 y0 z0  x1 y1 z1 … ]
    "normal N" [ nx0 ny0 nz0 … ]     # optional
    "point2 st" [ u0 v0  u1 v1 … ]   # optional UV coords
    "integer indices" [ 0 1 2  0 2 3 … ]
```

The current CTM is baked into vertex positions and normals at parse time:

```cpp
glm::mat4 M    = state.ctm;
glm::mat3 Minv = glm::transpose(glm::inverse(glm::mat3(M)));

for (auto& v : vertices) {
    v.position = glm::vec3(M * glm::vec4(v.position, 1.0f));
    v.normal   = glm::normalize(Minv * v.normal);
}
```

When `N` is absent, flat normals are computed from triangle cross-products.
When `st` is absent, UVs default to `{0, 0}`.

#### PLYMesh (external file)

```
Shape "plymesh"
    "string filename" [ "geometry/mesh.ply" ]
```

The path is resolved relative to the `.pbrt` file's directory. Assimp loads the `.ply`
file with the same post-processing flags as regular models (`Triangulate`,
`GenSmoothNormals`, `CalcTangentSpace`). The CTM is then baked into the resulting vertices.

### Texture loading (PBRT)

The parser maintains three separate named-texture maps:

| Map | Content | Created from |
|-----|---------|-------------|
| `m_NamedTextures` | `Ref<rhi::Texture>` colour/albedo textures | `"spectrum"` or `"rgb"` imagemap |
| `m_NamedFloatTextures` | `Ref<rhi::Texture>` greyscale textures | `"float"` imagemap (roughness, metallic) |

```
# Colour texture (albedo / reflectance)
Texture "floor-kd" "spectrum" "imagemap"
    "string filename" [ "textures/floor.png" ]

# Float texture (roughness channel)
Texture "floor-rough" "float" "imagemap"
    "string filename" [ "textures/floor_roughness.png" ]

# Scale passthrough (ignores scale factor; aliases to base texture)
Texture "floor-bump-scaled" "float" "scale"
    "float scale" [ 0.05 ]
    "texture tex" [ "floor-bump-base" ]

MakeNamedMaterial "floor"
    "string type" [ "coateddiffuse" ]
    "texture reflectance" [ "floor-kd" ]
    "texture roughness"   [ "floor-rough" ]
    "float roughness"     [ 0.010408 ]
```

**Format selection**:
- Colour textures → `R8G8B8A8_SRGB`
- Normal map textures → `R8G8B8A8_UNORM` (linear)
- Float (greyscale) textures → `R8G8B8A8_UNORM` (single channel packed)

**`scale` texture operator**: PBRT uses `scale` to multiply two textures together (e.g.,
for bump map intensity). The rasteriser does not render bump/displacement, so `"float"`
scale textures are silently skipped. For `"spectrum"` or `"rgb"` scale textures the base
texture is stored as-is (the scale factor is ignored as an approximation).

### Material types

| PBRT type | Albedo | Roughness | Metallic | Notes |
|-----------|--------|-----------|----------|-------|
| `diffuse` | `rgb reflectance` or texture | 1.0 | 0.0 | Fully matte |
| `coateddiffuse` | `rgb reflectance` or texture | `float roughness` | 0.0 | Diffuse with glossy coat |
| `conductor` | Spectral F0 from `spectrum eta/k` | `float roughness` (≥ 0.02) | 1.0 | Metallic |
| `coatedconductor` | Same as conductor | Same | 1.0 | Conductor with dielectric coat |
| `dielectric` | White | `float roughness` | 0.0 | Glass / transparent |
| `mirror` | White | 0.0 | 1.0 | Perfect reflector |
| `mix` | Linear blend of `mat1`/`mat2` by `amount` | Blended | Blended | Both materials looked up in `m_NamedMaterials` |
| `measured` | White-grey (0.9) | 0.3 | 0.0 | Proprietary BSDF approximated as coateddiffuse |
| default | Grey 0.5 | 0.5 | 0.0 | Fallback for unknown types |

#### `mix` material blending

```
Material "mix"
    "material mat1" [ "wall_paint" ]
    "material mat2" [ "plaster" ]
    "float amount"  [ 0.5 ]
```

The parser looks up both named materials in `m_NamedMaterials` and linearly interpolates
albedo, roughness, metallic, and emissive using the `amount` parameter (0 = mat1, 1 = mat2).
If one material has an albedo texture and the other does not, the textured material takes
precedence. If either named material is not found, the other is used unchanged.

#### `measured` material fallback

PBRT's `measured` BSDF type references proprietary `.bsdf` data files that are not
publicly available. The parser approximates this as a light-grey coated diffuse
(albedo = 0.9, roughness = 0.3) and logs an info message.

#### Normal / roughness / metallic maps

When a named material references float or colour textures for PBR channels, the material
slots are populated:

```
MakeNamedMaterial "my-material"
    "string type"         [ "coateddiffuse" ]
    "texture reflectance" [ "my-albedo-tex" ]  # → SetAlbedoMap()
    "texture normalmap"   [ "my-normal-tex" ]  # → SetNormalMap()
    "texture roughness"   [ "my-rough-tex" ]   # → SetRoughnessMap()
    "texture metallic"    [ "my-metal-tex" ]   # → SetMetallicMap()
```

Texture flags (`HasNormalMap`, `HasRoughnessMap`, `HasMetallicMap`) are set in
`MaterialProperties::textureFlags` so the GPU shader samples the right slots.

#### Conductor spectral data

PBRT conductors define optical constants via named spectra:

```
MakeNamedMaterial "silver-faucet"
    "string type" [ "conductor" ]
    "spectrum eta" [ "metal-Ag-eta" ]
    "spectrum k"   [ "metal-Ag-k" ]
    "float roughness" [ 0.001 ]
```

The parser converts the named spectra to approximate RGB Fresnel reflectance (F0) using a
lookup table of optical constants at R/G/B wavelengths (630 / 532 / 467 nm):

```
F0(λ) = ((n(λ) - 1)² + k(λ)²) / ((n(λ) + 1)² + k(λ)²)
```

| Named spectra prefix | Metal | Approx. RGB F0 |
|---------------------|-------|----------------|
| `metal-Ag` | Silver | (0.972, 0.960, 0.915) |
| `metal-Al` | Aluminium | (0.913, 0.922, 0.924) |
| `metal-Au` | Gold | (1.000, 0.782, 0.344) |
| `metal-Cr` | Chromium | (0.549, 0.556, 0.554) |
| `metal-Cu` | Copper | (0.955, 0.638, 0.538) |
| `metal-Fe` | Iron | (0.531, 0.512, 0.496) |
| `metal-Ni` | Nickel | (0.660, 0.609, 0.526) |
| `metal-Pt` | Platinum | (0.673, 0.637, 0.585) |
| `metal-Ti` | Titanium | (0.542, 0.497, 0.449) |
| `metal-W` | Tungsten | (0.504, 0.500, 0.478) |

**Minimum roughness clamp**: Conductor roughness is clamped to 0.02 regardless of the
scene value. Near-zero roughness creates a perfect mirror that appears black without
ray-traced reflections — the clamp ensures direct lights still produce visible highlights
in the real-time rasteriser.

### Light extraction

| PBRT type | MetaGFX class | Key parameters |
|-----------|--------------|----------------|
| `distant` | `DirectionalLight` | `from`, `to` → direction; `L` → colour; `scale` → intensity |
| `point` | `PointLight` | `from` → position; `I` → colour; `scale` → intensity |
| `spot` | `SpotLight` | `from`, `to` → position/direction; `I` → colour; `coneangle` / `conedeltaangle` → outer/inner angle |
| `infinite` | Environment map | `string mapname` → HDR env map path stored in `PbrtParseResult::envMapPath` |

#### `LightSource "infinite"` — environment map

PBRT's infinite light source maps to an HDR sky environment. The parser records the path:

```
LightSource "infinite"
    "string mapname" [ "textures/Skydome.pfm" ]
```

After parsing, `Application::LoadModel()` reads `result.envMapPath`. If the file has
extension `.pfm`, it is loaded as an equirectangular HDR image and converted to a GPU
cubemap for the skybox and an irradiance cubemap for IBL ambient lighting (see
[HDR Environment Map Loading](#hdr-environment-map-loading-textureutils) below).
The skybox is automatically shown; IBL is prepared but left off by default so that the
scene's direct lights dominate — IBL can be enabled at any time via the ImGui panel.

Lights are added directly to the `Scene` object passed to `PbrtLoader::Load()`. If a
scene is provided, `scene->ClearLights()` is called first so PBRT-defined lights replace
the default test lights.

### Camera extraction

`LookAt` and `Camera "perspective"` together define the starting viewpoint:

```
LookAt  eye_x eye_y eye_z
        look_x look_y look_z
        up_x up_y up_z

Camera "perspective"
    "float fov" [ 45 ]
```

When `camera.defined == true`, the application sets the camera perspective and position
then sets the orbit target to the scene centre so the user can freely orbit after loading.

### Procedural shapes

#### Sphere

```
Shape "sphere"  "float radius" [ 0.5 ]
```

A UV sphere is generated with 32 latitude × 16 longitude segments. The current CTM is
baked into vertex positions and normals. UVs follow spherical mapping (φ/2π, θ/π).

#### Disk

```
Shape "disk"  "float radius" [ 1.0 ]  "float height" [ 0.0 ]
```

A circular triangle fan with 64 sectors. The disk is centred at `(0, height, 0)` with
the normal pointing up (+Y). The CTM is baked in.

### Object instancing

`ObjectBegin` / `ObjectEnd` define a named object template; `ObjectInstance` stamps copies
into the scene at the current CTM:

```
ObjectBegin "desk-unit"
    Shape "plymesh"  "string filename" [ "geometry/desk.ply" ]
    Shape "plymesh"  "string filename" [ "geometry/chair.ply" ]
ObjectEnd

# Place four copies
AttributeBegin
    Translate  0  0  0    ObjectInstance "desk-unit"
    Translate  3  0  0    ObjectInstance "desk-unit"
    Translate  0  0 -3    ObjectInstance "desk-unit"
    Translate  3  0 -3    ObjectInstance "desk-unit"
AttributeEnd
```

The parser accumulates meshes during an `ObjectBegin` block into `m_ObjectBuffer`, then
stores them in `m_NamedObjects[name]` on `ObjectEnd`. Each `ObjectInstance` clones the
stored meshes and applies the current CTM to their vertices before appending to the scene.

### TransformBegin / TransformEnd

These scoped directives push and pop the graphics state (equivalent to `AttributeBegin` /
`AttributeEnd`) but conventionally enclose only transform changes. They were needed to
correctly handle scenes like the classroom, where a large floor scale transform is
wrapped in `TransformBegin`/`TransformEnd` to prevent it from bleeding into subsequent
mesh definitions.

### Remaining limitations

- Procedural textures (`"checkerboard"`, `"fbm"`, `"windy"`, etc.) — not supported
- `Shape "loopsubdiv"` (Loop subdivision surfaces) — not supported
- Spectral rendering — colours approximated as RGB
- Bump mapping / displacement — parsed but not rendered in the rasteriser

---

## Mesh and Model Classes

**Location**: `include/metagfx/scene/Mesh.h`, `src/scene/Mesh.cpp`,
`include/metagfx/scene/Model.h`, `src/scene/Model.cpp`

### Vertex layout

```cpp
struct Vertex {
    glm::vec3 position;  // 12 bytes, offset  0
    glm::vec3 normal;    //  12 bytes, offset 12
    glm::vec2 texCoord;  //  8 bytes, offset 24
};                       // Total: 32 bytes
```

**Shader binding** (`model.vert`):
```glsl
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
```

### Mesh lifecycle

```
Mesh::Initialize(device, vertices, indices)
  ├── Create vertex buffer (BufferUsage::Vertex | TransferDst)
  ├── CopyData(vertices)
  ├── Create index buffer (BufferUsage::Index | TransferDst)
  ├── CopyData(indices)
  ├── Generate LOD index buffers (meshoptimizer)
  └── CacheBounds (baked into Model after AddMesh)
```

`Mesh` is **movable but not copyable** — prevents accidental duplication of GPU resources.

### Model as container

```cpp
class Model {
    bool LoadFromFile(rhi::GraphicsDevice*, const std::string& path);
    bool CreateCube  (rhi::GraphicsDevice*, float size = 1.0f);
    bool CreateSphere(rhi::GraphicsDevice*, float radius = 1.0f, uint32_t segs = 32);
    void AddMesh(std::unique_ptr<Mesh>);

    const std::vector<std::unique_ptr<Mesh>>& GetMeshes() const;
    glm::vec3 GetCenter() const;
    glm::vec3 GetSize()   const;
    float     GetBoundingSphereRadius() const;
    bool      GetBoundingBox(glm::vec3& min, glm::vec3& max) const;
};
```

---

## Material Extraction

### Per-mesh materials

Each mesh carries its own `Material`, uploaded to its own GPU uniform buffer.  This
avoids the "last material wins" problem where a shared buffer is overwritten per-mesh
on the CPU and only the last write is seen by the GPU.

The material properties struct (48 bytes, std140 layout):

```cpp
struct MaterialProperties {
    glm::vec3 albedo;         // offset  0, 12 bytes
    float     roughness;      // offset 12,  4 bytes
    float     metallic;       // offset 16,  4 bytes
    uint32    textureFlags;   // offset 20,  4 bytes  — bitmask of MaterialTextureFlags
    float     padding1[2];    // offset 24,  8 bytes
    glm::vec3 emissiveFactor; // offset 32, 12 bytes
    float     padding2;       // offset 44,  4 bytes
};                             // Total: 48 bytes
```

### Texture flags bitmask

```cpp
enum class MaterialTextureFlags : uint32 {
    HasAlbedoMap            = 1 << 0,
    HasNormalMap            = 1 << 1,
    HasMetallicMap          = 1 << 2,
    HasRoughnessMap         = 1 << 3,
    HasMetallicRoughnessMap = 1 << 4,  // glTF combined map
    HasAOMap                = 1 << 5,
    HasEmissiveMap          = 1 << 6,
};
```

The flags are stored inside `MaterialProperties::textureFlags` and read by the fragment
shader at binding 1 (MaterialUBO), so each mesh uses its own flags without contention.

The fragment shader samples textures conditionally:

```glsl
if ((material.textureFlags & (1u << 0)) != 0u)  // HasAlbedoMap
    albedo = texture(albedoSampler, fragTexCoord).rgb;
else
    albedo = material.albedo;  // scalar fallback
```

---

## Level of Detail (LOD)

MetaGFX generates simplified LOD index buffers at mesh load time using
[meshoptimizer](https://github.com/zeux/meshoptimizer).

### LOD levels

| Level | Target ratio | Typical use |
|-------|-------------|-------------|
| LOD 0 | 100% (original) | Close range, shadow pass |
| LOD 1 | 50% of indices | Mid range |
| LOD 2 | 20% of indices | Far range |

Each LOD has its own index buffer; all LODs share the same vertex buffer.

### Generation

```cpp
const float lodRatios[2] = { 0.5f, 0.2f };
for (int lod = 0; lod < 2; ++lod) {
    size_t targetCount = std::max<size_t>(3, indices.size() * lodRatios[lod]);
    std::vector<uint32_t> lodIndices(indices.size());
    size_t lodCount = meshopt_simplify(
        lodIndices.data(), indices.data(), indices.size(),
        &vertices[0].position.x, vertices.size(), sizeof(Vertex),
        targetCount, /*target_error=*/0.05f);
    meshopt_optimizeVertexCache(lodIndices.data(), lodIndices.data(), lodCount, vertices.size());
    lodIndices.resize(lodCount);
    // Upload to dedicated GPU index buffer stored in m_LODLevels[lod]
}
```

### LOD selection

Camera distance to model centre selects the LOD:

```cpp
int lod = 0;
if (m_EnableLOD) {
    float dist = glm::length(camera->GetPosition() - model->GetCenter());
    if      (dist > m_LOD2Distance) lod = 2;  // default: 20 m
    else if (dist > m_LOD1Distance) lod = 1;  // default:  5 m
}
cmd->BindIndexBuffer(mesh->GetIndexBuffer(lod));
cmd->DrawIndexed(mesh->GetIndexCount(lod), instanceCount);
```

Thresholds are adjustable at runtime in the ImGui **Optimizations** panel.

### Mesh API

```cpp
Ref<rhi::Buffer> GetIndexBuffer(int lod = 0) const;  // LOD 0-2
uint32_t         GetIndexCount (int lod = 0) const;
int              GetLODCount()               const;   // always 3
```

---

## Instanced Rendering

Instanced rendering draws multiple copies of a model in a **single draw call**.

### How it works

Per-instance transform (`mat4`) is passed as a second vertex buffer at slot 1 with
`VertexInputRate::Instance`. The vertex shader reads it from locations 3–6:

```glsl
layout(location = 3) in vec4 instanceRow0;
layout(location = 4) in vec4 instanceRow1;
layout(location = 5) in vec4 instanceRow2;
layout(location = 6) in vec4 instanceRow3;

mat4 instanceModel = mat4(instanceRow0, instanceRow1, instanceRow2, instanceRow3);
vec4 worldPos = instanceModel * vec4(inPosition, 1.0);
```

### Pipeline vertex input

```cpp
pipelineDesc.vertexInputState.bindings = {
    { 0, sizeof(Vertex),    VertexInputRate::Vertex   },  // per-vertex
    { 1, sizeof(glm::mat4), VertexInputRate::Instance },  // per-instance
};
```

### Instance buffer management

`CreateInstanceBuffer()` builds the CPU-side transform list and uploads it to
`m_InstanceBuffer`. The buffer is sized for the maximum instance count and never
reallocated mid-frame. UI changes set `m_InstanceBufferDirty = true`; the buffer is
rebuilt at the start of the next frame before command recording begins.

A separate `m_SingleInstanceBuffer` (one identity mat4) is always bound for non-instanced
draws (e.g., ground plane) so the pipeline's declared vertex binding is satisfied.

### Per-instance frustum culling

When both instancing and frustum culling are enabled, only visible instances are uploaded:

```cpp
for (const auto& t : m_InstanceTransforms) {
    glm::vec3 iCenter = glm::vec3(t * glm::vec4(modelCenter, 1.0f));
    if (frustum.IntersectsSphere(iCenter, sphereRadius))
        visibleTransforms.push_back(t);
}
m_InstanceBuffer->CopyData(visibleTransforms.data(), ...);
cmd->DrawIndexed(indexCount, visibleTransforms.size());
```

See [camera_transformation_system.md](camera_transformation_system.md) for frustum details.

### Rendering loop

```cpp
cmd->BindVertexBuffer(mesh->GetVertexBuffer(), 0);  // per-vertex
cmd->BindVertexBuffer(m_InstanceBuffer,        1);  // per-instance
cmd->BindIndexBuffer (mesh->GetIndexBuffer(lod));
cmd->DrawIndexed     (mesh->GetIndexCount(lod), instanceCount);
```

---

## Bounding Volumes

`Model` caches bounding volumes after loading for frustum culling and camera framing:

```cpp
glm::vec3 center = model->GetCenter();
glm::vec3 size   = model->GetSize();
float     radius = model->GetBoundingSphereRadius();

glm::vec3 bboxMin, bboxMax;
model->GetBoundingBox(bboxMin, bboxMax);
```

`CacheBounds()` is called automatically after `LoadFromFile`, `CreateCube`, `CreateSphere`,
and `AddMesh` by iterating all mesh vertices. This is a one-time O(N) cost at load time.

---

## Error Handling and Fallbacks

### Loading failures

| Condition | Behaviour |
|-----------|-----------|
| File not found | Log error, return `false` / empty result |
| Assimp import failure | Log Assimp error string, fallback to cube |
| PBRT parse error | Log token location, fallback to cube |
| Missing texture file | Log warning, bind default texture |
| Missing PBRT named texture | Log warning, use scalar albedo |
| Unknown PBRT material type | Log warning, use grey diffuse fallback |

### Application fallback pattern

```cpp
auto result = PbrtLoader::Load(m_Device.get(), path, m_Scene.get());
if (!result.model || !result.model->IsValid()) {
    METAGFX_WARN << "PBRT load failed: " << path << " — using fallback cube";
    m_Model = std::make_unique<Model>();
    m_Model->CreateCube(m_Device.get(), 1.0f);
} else {
    m_Model = std::move(result.model);
}
```

### Debugging tips

1. **Check paths**: The application working directory is `build/bin/`; relative asset paths
   must be consistent with that root.
2. **Inspect WGSL output**: WebGPU shaders are saved to `/tmp/shader_N.wgsl` at startup.
3. **Missing textures**: The magenta/white checkerboard default texture makes binding
   failures immediately obvious.
4. **PBRT unknown materials**: `METAGFX_WARN` messages log the material type that was not
   recognised; check the `.pbrt` file's `MakeNamedMaterial` blocks.
5. **Conductor darkness**: Near-zero roughness conductors rely on environment reflections.
   Enable IBL in the ImGui panel or increase roughness to see direct highlights.

---

---

## HDR Environment Map Loading (TextureUtils)

**Location**: `include/metagfx/utils/TextureUtils.h`, `src/utils/TextureUtils.cpp`

PBRT scenes often specify environment maps as equirectangular HDR images. MetaGFX provides
a CPU-side pipeline to convert these into GPU-ready cubemaps:

```
Equirectangular HDR image (PFM / EXR)
        │
        ▼
  TextureUtils::LoadPFMImage()        ← parse PFM header, flip rows, expand RGB→RGBA
        │
        ▼  HDRImageData { float* pixels, width, height, channels }
        │
   ┌────┴────────────────────────────────┐
   │                                     │
   ▼                                     ▼
LoadCubemapFromEquirectangular()    ComputeIrradianceCubemap()
  (for skybox display)               (for IBL ambient fill)
        │                                 │
        ▼                                 ▼
  R16G16B16A16_SFLOAT              R16G16B16A16_SFLOAT
  TextureCube, 256×256/face        TextureCube, 32×32/face
```

### LoadPFMImage

Loads a Portable Float Map (`.pfm`) file:
- Parses the three-line text header: magic `"PF"` (colour), `"width height"`, scale factor
- Reads binary RGB float32 data (PFM stores rows bottom-to-top)
- Flips vertically and expands to RGBA (alpha = 1.0)
- Returns `HDRImageData` with `malloc`-allocated pixels (free with `FreeHDRImage()`)

Only colour PFMs (`"PF"` magic) are supported. Greyscale PFMs (`"Pf"`) are rejected with
an error log.

### LoadCubemapFromEquirectangular

Converts an equirectangular HDR image to a `TextureCube` for skybox display:

```cpp
Ref<rhi::Texture> LoadCubemapFromEquirectangular(
    rhi::GraphicsDevice* device,
    const HDRImageData&  equirectangular,
    uint32               faceSize = 256
);
```

For each of the 6 cubemap faces (+X, -X, +Y, -Y, +Z, -Z), each texel's world-space
direction is computed via `CubemapFaceDir()` (OpenGL/DDS convention), then the
equirectangular image is sampled at the corresponding (φ, θ) with bilinear filtering
(horizontal wraparound). Output pixels are stored as **float16** (`R16G16B16A16_SFLOAT`).

Float16 is used instead of float32 because WebGPU requires the `float32-filterable`
optional feature to sample `R32G32B32A32_SFLOAT` textures — a feature not universally
available. Float16 is filterable on all backends without optional features.

### ComputeIrradianceCubemap

Computes a diffuse irradiance cubemap by integrating the equirectangular environment map
over the hemisphere (for PBR ambient IBL):

```cpp
Ref<rhi::Texture> ComputeIrradianceCubemap(
    rhi::GraphicsDevice* device,
    const HDRImageData&  equirectangular,
    uint32               faceSize = 32
);
```

Uses cosine-weighted Monte Carlo integration with the Fibonacci spiral:

```
E(n) ≈ (π / N) × Σᵢ L(ωᵢ)   for N = 128 cosine-weighted samples per texel
```

The Fibonacci spiral distributes N samples uniformly over the hemisphere without
clumping. The integrand is the radiance from the equirectangular environment at each
sample direction. Output is `R16G16B16A16_SFLOAT`, 32×32 per face (irradiance is
low-frequency; higher resolution adds no visible benefit).

### IBL integration with PBRT scenes

When a PBRT scene defines `LightSource "infinite"`, `Application::LoadModel()`:
1. Calls `LoadPFMImage()` to decode the HDR file
2. Calls `LoadCubemapFromEquirectangular()` → stored in `m_EnvironmentMap`; skybox is shown
3. Calls `ComputeIrradianceCubemap()` → stored in `m_IrradianceMap`; IBL is prepared
4. Recreates skybox descriptor sets via `RecreateSkyboxDescriptorSets()`
5. Leaves `m_EnableIBL = false` — the scene's direct lights provide primary illumination;
   IBL ambient fill can be enabled interactively via **Enable IBL** in the ImGui panel

---



### CMake modules

| Module | Purpose |
|--------|---------|
| `external/CMakeLists.txt` | Assimp importers, stb, meshoptimizer |
| `src/scene/CMakeLists.txt` | Scene sources including `pbrt/` sub-directory |

### Scene module sources

```cmake
# src/scene/CMakeLists.txt
list(APPEND SCENE_SOURCES
    Mesh.cpp  Model.cpp  Material.cpp  Camera.cpp  Light.cpp  ShadowMap.cpp
    pbrt/PbrtLexer.cpp
    pbrt/PbrtParser.cpp
    pbrt/PbrtLoader.cpp
)
list(APPEND SCENE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/pbrt/PbrtLexer.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/pbrt/PbrtParser.h
    ${CMAKE_SOURCE_DIR}/include/metagfx/scene/pbrt/PbrtLoader.h
)
```

No new external dependency: PLY loading reuses Assimp; image loading reuses stb_image.

### Dependencies

| Library | Used for |
|---------|---------|
| Assimp | glTF/OBJ/FBX/COLLADA loading + PLYMesh in PBRT |
| stb_image | PNG/JPEG/TGA/BMP decode |
| meshoptimizer | LOD index buffer simplification |
| GLM | Vertex maths, CTM application |
