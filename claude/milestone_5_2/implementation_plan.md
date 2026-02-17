# Milestone 5.2: Advanced PBRT Features – Implementation Plan

## Context

Milestone 5.1 delivered a working PBRT v4 parser that handles `trianglemesh`, `plymesh`, basic material types (diffuse, coateddiffuse, conductor, dielectric, mirror, mix-stub), albedo imagemap textures, distant/point/area lights, and the full transform stack. The contemporary-bathroom scene loads ~70% correctly but is missing:

- `scale` texture operator (used for rug-bump and floor-bump textures)
- `mix` material proper blending (currently a grey diffuse stub)
- Texture maps beyond albedo: normal maps, roughness maps, metallic maps
- `ObjectBegin/ObjectEnd/ObjectInstance` instancing (milestone required)
- `LightSource "infinite"` — IBL from scene-defined environment map
- Additional shapes: sphere, disk (procedural tessellation)
- `LightSource "spot"` → SpotLight
- `measured` BSDF fallback

The goal of 5.2 is to reach "Complete PBRT format support" with complex scenes rendering correctly.

---

## Scope

### Priority 1 – Texture System Expansion (highest visual impact)

**A. Normal / Roughness / Metallic map loading from PBRT Texture directives**

`HandleTexture()` currently only handles `"imagemap"` type and only stores textures in `m_NamedTextures`. The material parameters `"texture normalmap"`, `"texture roughness"`, `"texture metallic"` are never consumed.

Changes required:
- In `HandleTexture()`: Detect texture role from PBRT type token (`"float"` → float texture, `"spectrum"` or `"rgb"` → color texture). Store in separate maps `m_NamedFloatTextures` and `m_NamedColorTextures` alongside `m_NamedTextures`.
- In `BuildMaterial()` / `HandleMaterial()`: After resolving the Material, check for `"texture normalmap"` param → call `mat.SetNormalMap()`. Check for `"texture roughness"` → call `mat.SetRoughnessMap()`. Check for `"texture metallic"` → call `mat.SetMetallicMap()`.
- The GPU textures are created with appropriate formats: `RGBA8_UNORM` for normal maps (linear), `R8_UNORM` or `RGBA8_UNORM` for single-channel roughness/metallic.
- Texture flags `HasNormalMap`, `HasRoughnessMap`, `HasMetallicMap` are already defined in `MaterialTextureFlags` in [include/metagfx/scene/Material.h](include/metagfx/scene/Material.h) — just activate them.

**B. `scale` texture operator**

`Texture "rug-bump" "float" "scale" "float scale" [0.05] "texture tex" ["rug-bump-base"]`

For the rasterizer, displacement/bump maps are not rendered, so `"float"` scale textures can be silently skipped (no GPU texture created; any material referencing them is left untextured in that channel).

For `"spectrum"` or `"rgb"` scale textures (tinting another color texture): resolve the base texture and store it as-is (ignoring the scale factor as an approximation). This preserves albedo textures when nested under a scale.

Implementation:
- In `HandleTexture()`: check texture type string. If `"scale"`, get the `"texture tex"` parameter, resolve the underlying texture name from `m_NamedTextures`. If the base texture resolves to a GPU texture, store that GPU texture under the new name (pass-through). If it is a float texture, log and skip.

### Priority 2 – `mix` Material (proper implementation)

`Material "mix" "material mat1" ["wall_paint"] "material mat2" ["plaster"] "float amount" [0.5]`

Currently returns a grey diffuse stub.

Changes:
- In `BuildMaterial()` for `"mix"` type:
  1. Get `"mat1"` and `"mat2"` as string param (use `GetString()`).
  2. Look up each in `m_NamedMaterials` (already populated by `HandleMakeNamedMaterial()`).
  3. If both found, linearly interpolate: `albedo = mix(mat1.albedo, mat2.albedo, amount)`, same for roughness, metallic, emissive.
  4. Texture: if only one material has a texture, prefer the textured one (blend is approximate in rasterization).
  5. Fallback: if a named material is not found, use the other material unchanged.

Files: [src/scene/pbrt/PbrtParser.cpp](src/scene/pbrt/PbrtParser.cpp) – `BuildMaterial()` around line 702.

### Priority 3 – ObjectBegin / ObjectEnd / ObjectInstance (instancing)

Currently these directives skip all parameters.

Implementation:
- Add `m_NamedObjects: std::map<std::string, std::vector<PbrtMesh>>` to `PbrtParser` private members.
- Add `m_InsideObject: bool = false` and `m_CurrentObjectName: std::string` flags.
- Add `m_ObjectBuffer: std::vector<PbrtMesh>` to accumulate meshes during object definition.
- In `ParseWorldSection()`:
  - `ObjectBegin`: read the name string, set `m_InsideObject = true`, `m_CurrentObjectName = name`, clear `m_ObjectBuffer`. Push current graphics state.
  - `ObjectEnd`: pop graphics state, store `m_ObjectBuffer` into `m_NamedObjects[m_CurrentObjectName]`, set `m_InsideObject = false`.
  - `ObjectInstance`: read name, look up `m_NamedObjects[name]`. For each stored mesh, clone it and transform its vertices by the current CTM (`m_State.ctm`), then append to `m_Result.meshes`.
- In `BuildTriangleMesh()` / `BuildPlyMesh()`: when `m_InsideObject`, push new mesh to `m_ObjectBuffer` instead of `m_Result.meshes`.

Files:
- [include/metagfx/scene/pbrt/PbrtParser.h](include/metagfx/scene/pbrt/PbrtParser.h) — add members and method stubs.
- [src/scene/pbrt/PbrtParser.cpp](src/scene/pbrt/PbrtParser.cpp) — implement instancing logic.

### Priority 4 – Additional Shapes (sphere, disk)

**Sphere**: `Shape "sphere" "float radius" [r]` — generate UV sphere triangle mesh.
- Tessellate into configurable segments (32×16 or 64×32).
- Apply CTM from `m_State.ctm` to all vertices.
- Append to result meshes with current material.
- Utility: similar logic to `Model::CreateSphere()` at [src/scene/Model.cpp](src/scene/Model.cpp) — extract or reuse tessellation code.

**Disk**: `Shape "disk" "float radius" [r] "float height" [h]` — generate circular fan.
- Center at (0, h, 0), tessellate as triangle fan.
- Apply CTM.

Files: [src/scene/pbrt/PbrtParser.cpp](src/scene/pbrt/PbrtParser.cpp) — add `BuildSphere()` and `BuildDisk()` helpers, call from `HandleShape()`.

### Priority 5 – `LightSource "infinite"` (IBL from scene)

`LightSource "infinite" "string mapname" ["environment.exr"]`

The renderer already supports IBL environment maps loaded at startup. The PBRT scene specifies which HDR map to use.

Changes:
- Add `std::string envMapPath` to `PbrtParseResult` struct in [include/metagfx/scene/pbrt/PbrtParser.h](include/metagfx/scene/pbrt/PbrtParser.h).
- In `HandleLightSource()` for `"infinite"`: resolve `"string mapname"` relative to `m_BaseDir`, store in `m_Result.envMapPath`. Parse `"float scale"` if present and store as `m_Result.envMapScale`.
- In [src/app/Application.cpp](src/app/Application.cpp): after `PbrtParser::Parse()`, if `result.envMapPath` is non-empty, call the existing IBL loading function to set the environment map (the mechanism is already there for manually-loaded environments).

### Priority 6 – `LightSource "spot"`

`LightSource "spot" "point from" [...] "point to" [...] "rgb I" [...] "float coneangle" [30] "float conedeltaangle" [5]`

- Parse from/to for direction, I for color, coneangle and conedeltaangle.
- Create `SpotLight` (already implemented in [include/metagfx/scene/Light.h](include/metagfx/scene/Light.h)).
- Set inner angle = coneangle - conedeltaangle, outer angle = coneangle.

Files: [src/scene/pbrt/PbrtParser.cpp](src/scene/pbrt/PbrtParser.cpp) — `HandleLightSource()`.

### Priority 7 – `measured` Material Fallback

The contemporary-bathroom's bathtube uses `Material "measured" "string bsdffile" ["satin_white_spec.bsdf"]`. This BSDF format is proprietary; we cannot load it.

- In `BuildMaterial()` for `"measured"`: apply a reasonable fallback (white coateddiffuse with moderate roughness, e.g. albedo=0.9 grey, roughness=0.3).
- Remove any error log or warning about unsupported material — replace with an info message noting the BSDF is approximated.

---

## Files to Modify

| File | Changes |
|------|---------|
| [include/metagfx/scene/pbrt/PbrtParser.h](include/metagfx/scene/pbrt/PbrtParser.h) | Add `m_NamedObjects`, `m_InsideObject`, `m_ObjectBuffer` members; `envMapPath`/`envMapScale` to PbrtParseResult; new helpers `BuildSphere()`, `BuildDisk()` |
| [src/scene/pbrt/PbrtParser.cpp](src/scene/pbrt/PbrtParser.cpp) | All parser logic changes (texture system, mix material, instancing, new shapes, infinite light, spot light, measured fallback) |
| [src/app/Application.cpp](src/app/Application.cpp) | Read `result.envMapPath` after PBRT parse, load as IBL environment map if set |

---

## Implementation Order

1. `scale` texture operator (self-contained, no new types)
2. Normal/roughness/metallic map loading (uses existing Material infrastructure)
3. `mix` material (uses existing m_NamedMaterials)
4. `measured` material fallback (trivial)
5. ObjectBegin/ObjectEnd/ObjectInstance (new state, test with instanced scenes)
6. Sphere/Disk shape tessellation
7. `LightSource "spot"`
8. `LightSource "infinite"` + Application.cpp integration

---

## Plan File Location

After approval, the final plan will be saved to:
`claude/milestone_5_2/implementation_plan.md`

---

## Verification

1. **Build**: `cmake .. && make -j$(sysctl -n hw.ncpu)` — must compile with no errors.
2. **Contemporary-bathroom** scene: load in all 3 backends (Vulkan, Metal, WebGPU). Check that:
   - Rug and floor have correct bump-scale textures (or at least albedo textures applied)
   - Mix materials blend correctly (no grey placeholder surfaces)
   - Bathtube renders as white/grey instead of missing
3. **Cornell-box**: still renders correctly (regression check).
4. **Instancing test**: create or find a PBRT scene with `ObjectBegin`/`ObjectInstance` to validate geometry duplication.
5. **Sphere/disk test**: write a small test `.pbrt` file with a sphere shape and verify it tessellates.
6. **IBL test**: use a PBRT scene with `LightSource "infinite"` pointing to an HDR env map and verify the scene picks up the correct sky.
7. Validate across all 3 graphics backends.
