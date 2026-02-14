# Performance Metrics

MetaGFX displays real-time performance counters in the ImGui **Performance** panel (collapsible header, open by default).

---

## Counters

### Frame Time / FPS

```
Frame Time:   8.33 ms  (120 FPS)
```

Measures the wall-clock time for one complete frame: event processing, update, and render. Computed using `std::chrono::steady_clock`:

```cpp
m_FrameStart = std::chrono::steady_clock::now();
// ... ProcessEvents, Update, Render ...
auto frameEnd = std::chrono::steady_clock::now();
m_Metrics.frameTimeMs =
    std::chrono::duration<float, std::milli>(frameEnd - m_FrameStart).count();
```

**Display rate**: The value shown in the UI is averaged over a 500 ms window to prevent flickering. The smoothed values (`m_DisplayFrameTimeMs`, `m_DisplayFps`) are refreshed roughly twice per second.

**VSync**: With the default present mode (`VK_PRESENT_MODE_MAILBOX_KHR` / Metal/WebGPU equivalents) the frame rate is capped to the monitor refresh rate.

---

### Draw Calls

```
Draw Calls:   4
```

Counts every `DrawIndexed` call issued in the frame across **all passes and features**:

| Source | Condition |
|--------|-----------|
| Shadow pass — model meshes | shadows enabled |
| Main pass — model meshes | model loaded |
| Ground plane | ground plane enabled |
| Skybox | skybox enabled |

With instancing enabled, N copies of a mesh are drawn in a **single** `DrawIndexed` call. The draw call count stays the same regardless of instance count — the triangle count shows the real GPU load instead.

---

### Triangles

```
Triangles:    15.7 K
```

Counts triangles actually submitted to the GPU in the **main pass only** (model meshes):

```cpp
m_Metrics.triangles += mesh->GetIndexCount(lod) / 3 * instanceCount;
```

Factors in both the active LOD level (fewer indices = fewer triangles at distance) and the number of visible instances after frustum culling.

Large numbers are formatted with K/M suffixes for readability.

---

### Culled Meshes

```
Culled:       5 meshes
```

Counts objects (or instances) that were rejected by CPU frustum culling and **not submitted** to the GPU at all.

- **Non-instanced**: incremented by `model->GetMeshCount()` when the entire model's bounding sphere falls outside the frustum.
- **Instanced**: incremented per instance rejected during per-instance sphere tests.

A value of 0 is correct when everything is in view. See [camera_transformation_system.md — Frustum Culling](camera_transformation_system.md#4-frustum-culling) for how the tests work.

---

## ImGui Panel

The **Performance** header is always open by default. The **Optimizations** header directly below it exposes the toggles that affect the counters:

```
▼ Performance
  Frame Time:   8.33 ms  (120 FPS)
  Draw Calls:   4
  Triangles:    15.7 K
  Culled:       5 meshes

▼ Optimizations
  [x] Frustum Culling
  [x] Enable LOD
      LOD1 distance  [====|----] 5.0 m
      LOD2 distance  [========|] 20.0 m
  [ ] Instancing Grid
      Grid Size      [===|-----] 3
      Spacing        [====|----] 2.0 m
```

---

## Implementation Notes

### Why `frameTimeMs` is not reset to zero each frame

`RenderImGui()` runs **inside** `Render()`, before the end-of-frame timestamp is taken. Zeroing `frameTimeMs` at the top of the loop would always show 0 in the UI. Instead, only `drawCalls`, `triangles`, and `culledMeshes` are reset per frame; `frameTimeMs` retains the previous frame's value for display.

### Instance buffer update safety

UI changes to the instancing grid call `m_InstanceBufferDirty = true` rather than immediately replacing the buffer. The replacement happens at the **start of the next `Render()` call**, before any command recording. This prevents `VK_ERROR_DEVICE_LOST` from destroying a buffer that the previous frame's command buffer is still referencing.

---

## File Locations

| File | Purpose |
|------|---------|
| [src/app/Application.h](../src/app/Application.h) | `FrameMetrics` struct, display accumulators |
| [src/app/Application.cpp](../src/app/Application.cpp) | Counter increments, 500 ms smoothing, ImGui display |

---

## Related Documentation

- [Camera System — Frustum Culling](camera_transformation_system.md#4-frustum-culling)
- [Model Loading — LOD](model_loading.md#level-of-detail-lod)
- [Model Loading — Instancing](model_loading.md#instanced-rendering)
- [Resource Management](resource_management.md)
