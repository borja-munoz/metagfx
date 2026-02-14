# Plan: Milestone 4.3 - Rendering Optimizations

## Context

Milestone 4.2 (WebGPU) is complete. Milestone 4.3 adds four rendering optimizations to support complex scenes at stable framerates: **frustum culling**, **LOD via meshoptimizer**, **instanced rendering with grid demo**, and **performance metrics (CPU + GPU timing)**.

---

## Design Decisions (Confirmed)

| Feature | Decision |
|---------|----------|
| LOD | meshoptimizer auto-simplification at load time; 3 levels (LOD0=full, LOD1=50%, LOD2=20%) |
| LOD selection | Distance-based with ImGui-adjustable thresholds |
| Instancing | ImGui grid demo (N×N copies of current model) |
| Shadow pass | Also instanced (all N×N instances cast shadows) |
| Performance metrics | CPU counters + GPU timing (Vulkan+Metal full, WebGPU best-effort) |

---

## Feature 1: Frustum Culling

### New: `include/metagfx/math/Frustum.h` (header-only)

```cpp
struct Plane { glm::vec3 normal; float distance; };

class Frustum {
public:
    static Frustum FromViewProjection(const glm::mat4& vp);
    bool IntersectsSphere(glm::vec3 center, float radius) const;
    bool IntersectsAABB(glm::vec3 min, glm::vec3 max) const;
private:
    Plane m_Planes[6];  // Left, Right, Bottom, Top, Near, Far
};
```

Plane extraction uses the Gribb-Hartmann method (row-combine VP matrix columns). Both sphere and AABB tests provided since AABB is available from `Model::GetBoundingBox()` and sphere from `GetBoundingSphereRadius()`.

### `Camera.h / Camera.cpp`

- Add `Frustum GetFrustum() const` — computes `Frustum::FromViewProjection(GetViewProjectionMatrix())`

### `Model.h / Model.cpp`

- Cache computed AABB (`m_CachedBoundsMin`, `m_CachedBoundsMax`, `m_CachedSphereRadius`, `m_CachedCenter`) — currently recomputed on every call
- Compute and cache after `LoadFromFile` and `CreateCube/Sphere`
- Keep existing public API (`GetBoundingBox`, `GetBoundingSphereRadius`, `GetCenter`) reading from cache

### `Application.cpp`

In `Render()`, before shadow pass and main pass:
```cpp
Frustum cameraFrustum = m_Camera.GetFrustum();
Frustum lightFrustum  = Frustum::FromViewProjection(lightVP);

// Shadow pass: skip meshes outside light frustum
// Main pass: skip meshes outside camera frustum
```

Cull per-model first (bounding sphere), then per-mesh (mesh AABB approximated by model bounds scaled by relative size). Track `m_CulledMeshCount` for metrics.

---

## Feature 2: LOD (meshoptimizer)

### New dependency: meshoptimizer

`external/CMakeLists.txt`:
```cmake
FetchContent_Declare(meshoptimizer
    GIT_REPOSITORY https://github.com/zeux/meshoptimizer
    GIT_TAG        v0.21)
FetchContent_MakeAvailable(meshoptimizer)
```

Link to `metagfx_scene` via `target_link_libraries`.

### `Mesh.h / Mesh.cpp`

```cpp
struct LODLevel {
    Ref<rhi::Buffer> indexBuffer;
    uint32           indexCount;
};

class Mesh {
    // LOD0 is the existing m_IndexBuffer / m_IndexCount
    std::array<LODLevel, 2> m_LODLevels;  // LOD1 (50%), LOD2 (20%)
    ...
};
```

In `Mesh::Initialize()`, after creating the base index buffer:
```cpp
// Generate LOD1 (50% target) and LOD2 (20% target)
for (int lod = 1; lod <= 2; ++lod) {
    float ratio = (lod == 1) ? 0.5f : 0.2f;
    size_t targetCount = (m_Indices.size() / 3) * ratio * 3;  // keep triangle alignment
    std::vector<uint32> lodIndices(m_Indices.size());
    size_t lodCount = meshopt_simplify(lodIndices.data(),
        m_Indices.data(), m_Indices.size(),
        &m_Vertices[0].position.x, m_Vertices.size(), sizeof(Vertex),
        targetCount, 0.01f * lod);  // higher error allowed for lower LODs
    lodIndices.resize(lodCount);
    // Optimize for GPU cache
    meshopt_optimizeVertexCache(lodIndices.data(), lodIndices.data(), lodCount, m_Vertices.size());
    // Create GPU index buffer for this LOD
    m_LODLevels[lod-1] = { CreateGPUIndexBuffer(device, lodIndices), (uint32)lodCount };
}
```

Add `Ref<rhi::Buffer> GetIndexBuffer(int lod) const` and `uint32 GetIndexCount(int lod) const`.

### `Application.cpp`

ImGui controls (new "LOD" section):
```
LOD
├─ Enable LOD: [checkbox]
├─ LOD1 Distance: [5.0 m slider]
└─ LOD2 Distance: [20.0 m slider]
```

LOD selection in render loop:
```cpp
float dist = glm::length(m_Camera.GetPosition() - model->GetCenter());
int lod = 0;
if (dist > m_LOD2Distance) lod = 2;
else if (dist > m_LOD1Distance) lod = 1;

cmd->BindIndexBuffer(mesh->GetIndexBuffer(lod));
cmd->DrawIndexed(mesh->GetIndexCount(lod), ...);
```

---

## Feature 3: Instanced Rendering (Grid Demo)

### Shader changes

**`src/app/model.vert`** — add per-instance transform replacing `ubo.model`:
```glsl
// Per-instance attributes (binding 1, locations 3-6)
layout(location = 3) in vec4 instanceRow0;
layout(location = 4) in vec4 instanceRow1;
layout(location = 5) in vec4 instanceRow2;
layout(location = 6) in vec4 instanceRow3;

void main() {
    mat4 instanceTransform = mat4(instanceRow0, instanceRow1, instanceRow2, instanceRow3);
    // Replace: ubo.model * vec4(inPosition, 1.0)
    // With:    instanceTransform * vec4(inPosition, 1.0)
}
```

Recompile to `.spv.inl` after shader changes.

**`src/app/shadowmap.vert`** — same per-instance transform for shadow pass.

### Pipeline changes (all 3 backends)

In `PipelineDesc` for **both** model and shadow pipelines:
```cpp
// Binding 0: per-vertex data (existing)
VertexInputBinding { binding=0, stride=sizeof(Vertex), inputRate=Vertex }
// Binding 1: per-instance data (NEW)
VertexInputBinding { binding=1, stride=sizeof(glm::mat4), inputRate=Instance }

// 4 additional attributes for the mat4 rows
VertexAttribute { location=3, binding=1, format=R32G32B32A32_SFLOAT, offset=0  }
VertexAttribute { location=4, binding=1, format=R32G32B32A32_SFLOAT, offset=16 }
VertexAttribute { location=5, binding=1, format=R32G32B32A32_SFLOAT, offset=32 }
VertexAttribute { location=6, binding=1, format=R32G32B32A32_SFLOAT, offset=48 }
```

All backends already support `VertexInputRate::Instance` in their pipeline creation code path — needs to be wired.

### `CommandBuffer.h / BindVertexBuffer`

Add `BindVertexBuffer(buffer, slot)` overload (or use existing slot parameter) so instance buffer can be bound to slot 1 separately from the mesh vertex buffer at slot 0.

### `Application.h / Application.cpp`

```cpp
// New members
bool             m_EnableInstancing = false;
int              m_InstanceGridSize = 3;   // N×N grid
float            m_InstanceSpacing  = 2.0f;
Ref<rhi::Buffer> m_InstanceBuffer;         // N×N mat4s
```

Instance buffer creation:
```cpp
void Application::UpdateInstanceBuffer() {
    int N = m_InstanceGridSize;
    std::vector<glm::mat4> transforms;
    for (int x = 0; x < N; ++x)
        for (int z = 0; z < N; ++z) {
            float px = (x - N/2) * m_InstanceSpacing;
            float pz = (z - N/2) * m_InstanceSpacing;
            transforms.push_back(glm::translate(glm::mat4(1.0f), {px, 0, pz}));
        }
    m_InstanceBuffer = ...; // upload transforms
}
```

Render loop changes:
```cpp
int instanceCount = m_EnableInstancing ? m_InstanceGridSize * m_InstanceGridSize : 1;
cmd->BindVertexBuffer(mesh->GetVertexBuffer(), 0);
cmd->BindVertexBuffer(m_InstanceBuffer, 1);  // slot 1
cmd->BindIndexBuffer(mesh->GetIndexBuffer(lod));
cmd->DrawIndexed(mesh->GetIndexCount(lod), instanceCount);
```

ImGui controls (new "Instancing" section):
```
Instancing
├─ Enable Grid: [checkbox]
├─ Grid Size: [1 - 7 slider] (1×1 = single instance)
└─ Spacing: [1.0 - 5.0 m slider]
```

---

## Feature 4: Performance Metrics (CPU + GPU Timing)

### CPU counters (all backends)

New `Application.h` members:
```cpp
struct FrameMetrics {
    uint32 drawCalls;
    uint32 triangles;
    uint32 culledMeshes;
    float  frameTimeMs;
    float  gpuShadowPassMs;
    float  gpuMainPassMs;
};
FrameMetrics m_Metrics{};
std::chrono::steady_clock::time_point m_FrameStart;
```

### GPU timing — Vulkan

In `VulkanDevice` or `Application` (Vulkan path):
- Create `VkQueryPool` with `VK_QUERY_TYPE_TIMESTAMP`, 8 slots (4 begin+end pairs: shadow, main, imgui, spare)
- `vkCmdWriteTimestamp()` at start and end of each render pass
- `vkGetQueryPoolResults()` after present, convert using `timestampPeriod`

### GPU timing — Metal

- `MTLCommandBuffer` exposes `gpuStartTime` / `gpuEndTime` after completion
- Use `addCompletedHandler` on the Metal command buffer
- For per-pass granularity, use separate command buffers for shadow and main passes (already how Metal submit works in `MetalCommandBuffer`)

### GPU timing — WebGPU (best effort)

- At device init, check `wgpu::Feature::TimestampQuery`
- If supported: create `wgpu::QuerySet`, resolve timestamps to buffer after each pass
- If NOT supported: fall back to CPU-side `std::chrono` timing around `queue.Submit()` (inaccurate but non-zero)

### ImGui display

New **"Performance"** section at the top of the ImGui panel:
```
Performance
├─ Frame Time:    12.3 ms  (81 FPS)
├─ GPU Shadow:     0.8 ms
├─ GPU Main:       9.1 ms
├─ Draw Calls:    24
├─ Triangles:  1,247,832
├─ Culled:        0 meshes
└─ Active LOD:    0 / 1 / 2 (per mesh count)
```

---

## File Summary

| File | Change |
|------|--------|
| `external/CMakeLists.txt` | Add meshoptimizer FetchContent |
| `include/metagfx/math/Frustum.h` | **New** — header-only Frustum class |
| `include/metagfx/scene/Camera.h` | Add `GetFrustum()` |
| `src/scene/Camera.cpp` | Implement `GetFrustum()` |
| `include/metagfx/scene/Mesh.h` | Add `LODLevel` struct + `m_LODLevels[2]`; `GetIndexBuffer(lod)` |
| `src/scene/Mesh.cpp` | Generate LOD levels with meshoptimizer in `Initialize()` |
| `include/metagfx/scene/Model.h` | Add cached bounding volume members |
| `src/scene/Model.cpp` | Cache bounds after load |
| `src/app/model.vert` | Per-instance transform attributes (locations 3-6); recompile to `.spv.inl` |
| `src/app/shadowmap.vert` | Same per-instance attributes; recompile to `.spv.inl` |
| `src/app/Application.h` | `FrameMetrics`, instancing members, GPU query pool members |
| `src/app/Application.cpp` | Frustum cull loop; LOD selection; instanced draw calls; metrics ImGui section; GPU timing |
| `src/app/CMakeLists.txt` | Link meshoptimizer |
| `src/rhi/vulkan/VulkanCommandBuffer.cpp` | Wire `BindVertexBuffer(buffer, slot)` |
| `src/rhi/metal/MetalCommandBuffer.cpp` | Wire `BindVertexBuffer(buffer, slot)` |
| `src/rhi/webgpu/WebGPUCommandBuffer.cpp` | Wire `BindVertexBuffer(buffer, slot)` |

---

## Verification

1. **Frustum culling**: Rotate camera so model is behind viewer → draw call count drops to 0 in ImGui metrics; moving camera back shows model → count restores
2. **LOD**: Move camera very far from model → ImGui shows LOD2 active; triangle count drops significantly; visual difference is subtle
3. **Instancing**: Enable 3×3 grid in ImGui → 9 copies of model render in a grid; metrics show 9× triangles but same draw call count per mesh as single instance; all 9 cast shadows
4. **GPU timing**: Shadow pass time + main pass time appear in ImGui on all backends
5. **All three backends**: Build and run on Vulkan, Metal, WebGPU — all features work and metrics display correctly
