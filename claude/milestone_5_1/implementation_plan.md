# Milestone 5.1 — Basic PBRT v4 Parser: Implementation Plan

## Goal

Add a PBRT v4 scene parser so `.pbrt` files can be selected in the model combo and rendered
using the existing PBR pipeline. PBRT v3 support is out of scope.

User decisions:
- v4 syntax only
- Supported shapes: `TriangleMesh` (inline) and `PLYMesh` (external `.ply` via Assimp)
- PBRT `LookAt` / `Camera` replace the camera's starting position; user can orbit freely after
- `.pbrt` entries appear in the existing model list combo alongside `.glb` / `.obj` files

---

## Architecture

New self-contained module `src/scene/pbrt/` (3 files). Output plugs into the unchanged
`Model` / `Scene` / `Camera` / `Material` / `Light` classes.

```
include/metagfx/scene/pbrt/
  PbrtLexer.h      ← token types + Lexer class
  PbrtParser.h     ← Parser class, intermediate structs (PbrtMesh, PbrtCameraParams, PbrtParseResult)
  PbrtLoader.h     ← public API: PbrtLoadResult + PbrtLoader::Load()

src/scene/pbrt/
  PbrtLexer.cpp
  PbrtParser.cpp
  PbrtLoader.cpp
```

No new external library. PLY loading reuses the existing Assimp linkage.
Image textures reuse existing `utils::LoadImage` / `utils::CreateTextureFromImage`.

---

## Step-by-Step Implementation

### Step 1 — Test scene (`assets/scenes/cornell-box.pbrt`)

Write a hand-crafted PBRT v4 Cornell box scene that exercises:
- `LookAt` and `Camera "perspective"`
- `WorldBegin` / `WorldEnd`
- `AttributeBegin` / `AttributeEnd` (keyword form) and `{ }` (brace form)
- `Material "diffuse"` with `rgb reflectance`
- `MakeNamedMaterial` + `NamedMaterial`
- `Shape "trianglemesh"` with `P`, `N`, `st`, `indices`
- `LightSource "distant"` and `LightSource "point"`

Also create `assets/scenes/bunny.pbrt` that uses `Shape "plymesh"` referencing the
existing `assets/models/bunny.obj` (used as a stand-in for PLY to validate the code path).

---

### Step 2 — Lexer (`PbrtLexer.h` / `PbrtLexer.cpp`)

Token types:
```cpp
enum class PbrtTokenType { Ident, String, Number, LBracket, RBracket, LBrace, RBrace, Eof };
struct PbrtToken { PbrtTokenType type; std::string value; int line; std::string file; };
```

Lexer rules:
| Input | Token |
|-------|-------|
| `# …\n` | Skip (comment) |
| `"…"` | `String` with contents (no outer quotes) |
| `[` `]` `{` `}` | Bracket / Brace tokens |
| Letter start | `Ident` (e.g. `WorldBegin`, `Shape`) |
| Digit / `-` / `.` | `Number` (stored as raw string, parsed to float/int by parser) |

The lexer holds a **file stack** (`std::stack<FileState>`) where each `FileState` is an
`std::ifstream` + current line number + file path. When the parser issues an `Include`
directive, the lexer pushes a new `FileState`; on EOF it pops back to the parent.

Public interface:
```cpp
class PbrtLexer {
public:
    explicit PbrtLexer(const std::string& filepath);
    PbrtToken Next();       // Advance and return next token
    PbrtToken Peek();       // Look ahead without consuming
    void      PushFile(const std::string& filepath);  // For Include
    std::string CurrentLocation() const;              // "file:line" for errors
private:
    struct FileState { std::ifstream stream; int line; std::string path; };
    std::stack<FileState>  m_FileStack;
    std::optional<PbrtToken> m_Lookahead;
    PbrtToken ReadNext();
};
```

---

### Step 3 — Parser (`PbrtParser.h` / `PbrtParser.cpp`)

#### Internal types (in `PbrtParser.h`)

```cpp
struct PbrtCameraParams {
    bool defined = false;
    glm::vec3 eye{0,0,5}, look{0,0,0}, up{0,1,0};
    float fov = 45.0f;
};

struct PbrtMesh {
    std::vector<Vertex>   vertices;  // CTM baked in
    std::vector<uint32_t> indices;
    Material              material;
};

struct PbrtParseResult {
    std::vector<PbrtMesh>               meshes;
    PbrtCameraParams                    camera;
    std::vector<std::unique_ptr<Light>> lights;
};
```

#### Graphics state stack

```cpp
struct GraphicsState {
    glm::mat4 ctm{1.0f};
    Material  material;                              // active material
    bool      hasMaterial = false;
    std::map<std::string, Material>         namedMaterials;
    std::map<std::string, Ref<rhi::Texture>> namedTextures;
};
```
`AttributeBegin` / `{` pushes a copy; `AttributeEnd` / `}` pops.

Note: `namedMaterials` and `namedTextures` are intentionally **inside** the stack frame so
they survive `AttributeEnd` (they are copied to the child frame). This matches PBRT semantics
where named entities are visible across attribute blocks.

Actually, the correct PBRT semantics is that named materials and textures are *global* (not
scoped to attribute blocks). Store them **outside** the stack in the parser:
```cpp
std::map<std::string, Material>          m_NamedMaterials;
std::map<std::string, Ref<rhi::Texture>> m_NamedTextures;
```
Only `ctm`, `material`, `hasMaterial` are pushed/popped.

#### Parameter list parsing

Helper `parseParams()` consumes `"type name" [v …]` or `"type name" v` blocks until a
non-String/non-bracket token is found, returning:
```cpp
using ParamValue = std::variant<float, int, std::string,
                                std::vector<float>, std::vector<int>>;
std::map<std::string, ParamValue> parseParams();
```
Convenience accessors on top of the map:
```cpp
glm::vec3 getVec3(map, "name", default);
float     getFloat(map, "name", default);
int       getInt(map, "name", default);
std::string getString(map, "name", default);
```

#### Directive dispatch

**Global section** (before `WorldBegin`):

| Token | Action |
|-------|--------|
| `LookAt` | Read 9 floats → eye, look, up |
| `Camera` | Read type string + params; extract `fov` if type is `"perspective"` |
| `Film` / `Sampler` / `Integrator` / `Accelerator` | Read type + params, discard |
| `Transform` | Read `[16 floats]` → set CTM (column-major) |
| `Translate` | Read 3 floats → CTM = CTM * translate(x,y,z) |
| `Rotate` | Read 4 floats (angle, ax, ay, az) → CTM = CTM * rotate |
| `Scale` | Read 3 floats → CTM = CTM * scale |
| `Identity` | CTM = identity |
| `ConcatTransform` | Read `[16 floats]` → CTM = CTM * M |
| `WorldBegin` | Enter world section loop |

**World section** (between `WorldBegin` and `WorldEnd`):

| Token | Action |
|-------|--------|
| `AttributeBegin` / `LBrace` | Push state |
| `AttributeEnd` / `RBrace` | Pop state |
| `Transform` / `Translate` / `Rotate` / `Scale` / `ConcatTransform` / `Identity` | Update CTM |
| `Material` | Parse type + params → build `Material`, store in `state.material` |
| `MakeNamedMaterial` | Parse name + params → build `Material`, store in `m_NamedMaterials[name]` |
| `NamedMaterial` | Read name string → `state.material = m_NamedMaterials[name]` |
| `Texture` | Parse name, type, class, params → if class `"imagemap"`, load image → `m_NamedTextures[name]` |
| `Shape` | Parse type + params → build meshes (see below) |
| `LightSource` | Parse type + params → build light |
| `AreaLightSource` | Parse type + params, skip (deferred to 5.2) |
| `Include` | Read filename string → `lexer.PushFile(resolvedPath)` |
| `ReverseOrientation` | Flip normal sign flag (track in state) |
| `ObjectBegin` / `ObjectEnd` / `ObjectInstance` | Skip (deferred to 5.2) |
| `WorldEnd` | Break loop |

#### Shape building

**TriangleMesh**:
```
params["P"]       → float array → vec3 positions
params["N"]       → float array → vec3 normals (optional)
params["st"]      → float array → vec2 UVs (optional, interleaved u0 v0 u1 v1 …)
params["indices"] → int array   → triangle index list
```
Transform:
```cpp
glm::mat4 M    = state.ctm;
glm::mat3 Minv = glm::transpose(glm::inverse(glm::mat3(M)));
// positions: p' = (M * vec4(p, 1)).xyz
// normals:   n' = normalize(Minv * n)
```
If `N` absent → compute flat normals from cross-products per triangle, then assign to each vertex.
If `st` absent → fill `texCoord = {0,0}`.
Result: one `PbrtMesh` appended.

**PLYMesh**:
```
params["filename"] → string path (relative to .pbrt file directory)
```
1. Resolve to absolute path using `.pbrt` parent directory
2. `Assimp::Importer importer; importer.ReadFile(plyPath, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace)`
3. Extract first mesh: positions + normals + UV channel 0 (same extraction loop as `Model::ProcessMesh()`)
4. Apply CTM to positions and normals (same transform as TriangleMesh)
5. Append to `PbrtParseResult::meshes` with `state.material`

#### Material building (`buildMaterial(type, params)`)

| PBRT type | albedo | roughness | metallic |
|-----------|--------|-----------|----------|
| `"diffuse"` | `rgb reflectance` or `spectrum reflectance` → vec3 | 1.0 | 0.0 |
| `"coateddiffuse"` | `rgb reflectance` | `float roughness` (default 0.5) | 0.0 |
| `"conductor"` | white (1,1,1) | avg(`uroughness`, `vroughness`) (default 0.1) | 1.0 |
| `"dielectric"` | white (1,1,1) | avg(`uroughness`, `vroughness`) (default 0.1) | 0.0 |
| `"mirror"` | white (1,1,1) | 0.0 | 1.0 |
| default | grey (0.5,0.5,0.5) | 0.5 | 0.0 |

Texture references: if `"texture reflectance"` param → look up `m_NamedTextures[name]` →
`material.SetAlbedoMap(texture)`. Other texture types (normal, roughness) deferred to 5.2.

#### Light building (`buildLight(type, params)`)

| PBRT type | MetaGFX | Key params |
|-----------|---------|-----------|
| `"distant"` | `DirectionalLight` | direction = normalize(`to` − `from`); color = `L` (default white); intensity = `scale` (default 1) |
| `"point"` | `PointLight` | position = `from`; color = `I`; intensity = `scale` |
| `"infinite"` | skip | |
| others | skip | |

---

### Step 4 — Loader (`PbrtLoader.h` / `PbrtLoader.cpp`)

Public API:
```cpp
struct PbrtLoadResult {
    std::unique_ptr<Model> model;   // all PBRT meshes combined
    PbrtCameraParams       camera;  // valid if camera.defined == true
    // lights added directly to scene* passed to Load()
};

class PbrtLoader {
public:
    static PbrtLoadResult Load(rhi::GraphicsDevice* device,
                               const std::string& filepath,
                               Scene* scene = nullptr);
};
```

`Load()` implementation:
1. Create `PbrtParser` with the device pointer (needed for texture loading)
2. Call `PbrtParser::Parse(filepath)` → `PbrtParseResult`
3. If `scene != nullptr`: `scene->ClearLights()`, then for each light in result: `scene->AddLight(std::move(light))`
4. Create `auto model = std::make_unique<Model>()`
5. For each `PbrtMesh` in result:
   a. Create `auto mesh = std::make_unique<Mesh>()`
   b. Call `mesh->Initialize(device, pbrtMesh.vertices, pbrtMesh.indices)`
   c. Create `auto mat = std::make_unique<Material>(pbrtMesh.material)`
   d. Call `mesh->SetMaterial(std::move(mat))`
   e. Call `model->AddMesh(std::move(mesh))`
6. Return `{ std::move(model), result.camera }`

---

### Step 5 — CMake (`src/scene/CMakeLists.txt`)

```cmake
list(APPEND SCENE_SOURCES
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

No new `target_link_libraries` needed (Assimp already linked).

---

### Step 6 — Application integration (`src/app/Application.cpp`)

#### `LoadModel()` — detect `.pbrt` extension

At the top of `LoadModel()`, before the existing Assimp path:
```cpp
// Detect PBRT scene files
if (path.size() > 5 && path.rfind(".pbrt") == path.size() - 5) {
    auto result = PbrtLoader::Load(m_Device.get(), path, m_Scene.get());
    if (!result.model || !result.model->IsValid()) {
        METAGFX_ERROR << "PBRT load failed: " << path;
        m_Model = std::make_unique<Model>();
        m_Model->CreateCube(m_Device.get());
        return;
    }
    m_Model = std::move(result.model);
    if (result.camera.defined) {
        float aspect = static_cast<float>(m_Config.width) / m_Config.height;
        float eyeDist = glm::length(result.camera.eye - result.camera.look);
        m_Camera->SetPerspective(result.camera.fov, aspect, 0.1f, eyeDist * 100.0f);
        m_Camera->LookAt(result.camera.look, result.camera.up);
        m_Camera->SetPosition(result.camera.eye);
        m_Camera->SetOrbitTarget(result.camera.look);
    }
    // framing, ground plane, descriptor update — same as Assimp path
    glm::vec3 min, max;
    if (m_Model->GetBoundingBox(min, max)) {
        glm::vec3 center = (min + max) * 0.5f;
        glm::vec3 size   = max - min;
        if (!result.camera.defined)
            m_Camera->FrameBoundingBox(center, size, 1.3f);
        UpdateGroundPlanePosition();
    }
    if (m_Config.graphicsAPI != rhi::GraphicsAPI::WebGPU && m_DescriptorSet[0]) {
        m_Device->WaitIdle();
        auto* firstMat = m_Model->GetMeshes().empty() ? nullptr
                        : m_Model->GetMeshes()[0]->GetMaterial();
        if (firstMat) UpdateModelDescriptorTextures(firstMat);
    }
    return;
}
// existing Assimp path follows...
```

Add the include at top of Application.cpp:
```cpp
#include "metagfx/scene/pbrt/PbrtLoader.h"
```

#### `Init()` — extend model list

```cpp
m_AvailableModels = {
    "/Users/Borja/dev/borja-munoz/metagfx/assets/models/AntiqueCamera.glb",
    "/Users/Borja/dev/borja-munoz/metagfx/assets/models/bunny_tex_coords.obj",
    "/Users/Borja/dev/borja-munoz/metagfx/assets/models/DamagedHelmet.glb",
    "/Users/Borja/dev/borja-munoz/metagfx/assets/models/MetalRoughSpheres.glb",
    "/Users/Borja/dev/borja-munoz/metagfx/assets/scenes/cornell-box.pbrt",  // new
};
```

---

## Files Created / Modified

| File | Status |
|------|--------|
| `assets/scenes/cornell-box.pbrt` | New |
| `include/metagfx/scene/pbrt/PbrtLexer.h` | New |
| `include/metagfx/scene/pbrt/PbrtParser.h` | New |
| `include/metagfx/scene/pbrt/PbrtLoader.h` | New |
| `src/scene/pbrt/PbrtLexer.cpp` | New |
| `src/scene/pbrt/PbrtParser.cpp` | New |
| `src/scene/pbrt/PbrtLoader.cpp` | New |
| `src/scene/CMakeLists.txt` | Append sources/headers |
| `src/app/Application.cpp` | LoadModel .pbrt branch + model list |

---

## Verification

1. `make -j$(sysctl -n hw.ncpu)` → zero errors, pre-existing warnings only
2. Select `cornell-box.pbrt` in the model combo
3. Cornell box walls visible with correct colors (red left, green right, white top/bottom/back)
4. Camera positioned at PBRT eye point looking at box interior
5. Distant light illuminating the scene (IBL off)
6. Select other `.glb` / `.obj` models → still work (regression check)
7. Confirm missing normals/UVs don't crash (flat normal fallback, zero UV fallback)
