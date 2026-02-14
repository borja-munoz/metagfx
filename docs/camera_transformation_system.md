# Camera System

## Overview

The MetaGFX camera system handles view/projection transforms, orbital controls, automatic model framing, and view-frustum culling. It is implemented in `include/metagfx/scene/Camera.h` and `src/scene/Camera.cpp`.

---

## 1. Camera Class

### Projection modes

```cpp
// Perspective (default)
camera->SetPerspective(fovDegrees, aspectRatio, nearPlane, farPlane);

// Orthographic
camera->SetOrthographic(left, right, bottom, top, nearPlane, farPlane);
```

### MVP matrices

```cpp
glm::mat4 view       = camera->GetViewMatrix();
glm::mat4 proj       = camera->GetProjectionMatrix();
glm::mat4 viewProj   = camera->GetViewProjectionMatrix();
```

### Y-axis flip

Vulkan and Metal use a Y-down NDC space; WebGPU uses Y-up (like OpenGL).
Pass `flipY = true` when creating the camera for Vulkan/Metal backends:

```cpp
bool flipY = (api == GraphicsAPI::Vulkan || api == GraphicsAPI::Metal);
auto camera = std::make_unique<Camera>(fov, aspect, nearZ, farZ, flipY);
```

The projection matrix applies a `scale(1, -1, 1)` transform when `flipY` is true.

---

## 2. Controls

### Orbital camera (default mode)

The camera orbits a fixed target point. Used for model inspection.

| Input | Action |
|-------|--------|
| Left-drag | Rotate around target |
| Scroll wheel | Zoom (change orbit distance) |
| Right-drag | Pan target point |

```cpp
camera->SetOrbitTarget(glm::vec3(0.0f));   // Set orbit center
camera->SetOrbitDistance(5.0f);            // Set initial distance
camera->UpdateOrbit(deltaYaw, deltaPitch); // Apply mouse delta
camera->ZoomOrbit(scrollDelta);            // Apply scroll
```

### FPS camera (optional)

WASD + QE movement with mouse-look. Enabled for free-flight through scenes.

| Key | Action |
|-----|--------|
| W/S | Move forward/back |
| A/D | Strafe left/right |
| Q/E | Move down/up |
| Mouse drag | Look |

---

## 3. Automatic Model Framing

When a model is loaded the camera is automatically positioned to frame the entire model with a 30 % margin.

### Algorithm

`Camera::FrameBoundingBox(center, size, marginFactor)`:

1. Apply margin: `adjustedSize = size * marginFactor`
2. Compute bounding sphere radius: `radius = length(adjustedSize) * 0.5`
3. Compute required distance from FOV: `distance = radius / tan(fov * 0.5)`
4. Place camera at **45° yaw, 30° pitch** (front-top-right view):
   ```cpp
   offset.x = distance * cos(pitch) * cos(yaw);
   offset.y = distance * sin(pitch);
   offset.z = distance * cos(pitch) * sin(yaw);
   position  = center + offset;
   ```
5. Configure orbital controls to orbit around `center`
6. Expand the far clipping plane if the model would be clipped:
   ```cpp
   if (distance + radius > farPlane * 0.8f)
       SetPerspective(fov, aspect, nearPlane, (distance + radius) * 2.0f);
   ```

### Usage

```cpp
// Automatically called by Application::LoadModel()
glm::vec3 center = model->GetCenter();
glm::vec3 size   = model->GetSize();
camera->FrameBoundingBox(center, size, 1.3f);  // 30 % margin
```

### Why bounding sphere, not box?

The sphere guarantees the model is fully visible from **any rotation angle**. A box could clip when viewed diagonally.

### Default angle rationale

45° yaw / 30° pitch shows three faces simultaneously (front, top, right), the same convention used in Blender and Maya.

---

## 4. Frustum Culling

The camera exposes its view frustum for CPU-side culling before submitting draw calls to the GPU.

### Getting the frustum

```cpp
Frustum frustum = camera->GetFrustum();
// Equivalent to:
Frustum frustum = Frustum::FromViewProjection(camera->GetViewProjectionMatrix());
```

### Frustum class (`include/metagfx/math/Frustum.h`)

Header-only. Extracts the six clip planes using the Gribb–Hartmann method directly from the view-projection matrix:

```cpp
// Left plane:   row3 + row0
// Right plane:  row3 - row0
// Bottom plane: row3 + row1
// Top plane:    row3 - row1
// Near plane:   row3 + row2
// Far plane:    row3 - row2
```

**Intersection tests**:

```cpp
// Sphere test — fast, used for model/instance culling
bool visible = frustum.IntersectsSphere(center, radius);

// AABB test — tighter fit for large objects
bool visible = frustum.IntersectsAABB(min, max);
```

### Culling strategy in the render loop

#### Non-instanced (single model)

Test the model's bounding sphere once before the mesh loop:

```cpp
if (m_EnableFrustumCulling) {
    Frustum f = camera->GetFrustum();
    modelVisible = f.IntersectsSphere(
        model->GetCenter(), model->GetBoundingSphereRadius() * 1.2f);
}
```

The 1.2× multiplier is a conservative expansion that avoids false-negative culling near frustum edges.

#### Instanced (N×N grid)

Each instance is tested individually. The model center is translated by the instance transform to get the per-instance sphere center:

```cpp
Frustum f       = camera->GetFrustum();
float   radius  = model->GetBoundingSphereRadius() * 1.2f;
glm::vec3 mCenter = model->GetCenter();

std::vector<glm::mat4> visibleTransforms;
for (const auto& t : instanceTransforms) {
    glm::vec3 iCenter = glm::vec3(t * glm::vec4(mCenter, 1.0f));
    if (f.IntersectsSphere(iCenter, radius))
        visibleTransforms.push_back(t);
}

// Upload only visible transforms, draw with reduced instance count
instanceBuffer->CopyData(visibleTransforms.data(),
                         visibleTransforms.size() * sizeof(glm::mat4));
cmd->DrawIndexed(indexCount, visibleTransforms.size());
```

This saves vertex shader invocations proportional to the number of culled instances.

### Performance counters

Culled instances/meshes are tracked in `FrameMetrics::culledMeshes` and displayed in the ImGui **Performance** panel. See [performance_metrics.md](performance_metrics.md).

---

## 5. Uniform Buffer Integration

Camera matrices are uploaded to a double-buffered uniform buffer every frame:

```cpp
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 lightSpaceMatrix;
    glm::vec3 lightDirection;
    float     shadowBias;
};
```

Two buffers are maintained (`m_UniformBuffers[0/1]`), one per frame in flight, to avoid writing data that the GPU is still reading. See [resource_management.md](resource_management.md).

---

## File Locations

| File | Purpose |
|------|---------|
| [include/metagfx/scene/Camera.h](../include/metagfx/scene/Camera.h) | Class declaration, `GetFrustum()` |
| [src/scene/Camera.cpp](../src/scene/Camera.cpp) | Implementation |
| [include/metagfx/math/Frustum.h](../include/metagfx/math/Frustum.h) | Header-only frustum extraction and tests |

---

## Related Documentation

- [Model Loading](model_loading.md) — bounding box / sphere used by culling
- [Resource Management](resource_management.md) — double-buffered uniform buffers
- [Performance Metrics](performance_metrics.md) — culled mesh counter
