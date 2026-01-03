# Automatic Camera Framing

MetaGFX includes automatic camera framing that positions the camera to view the entire model when it's loaded, with proper margins for better visualization.

## Overview

When a model is loaded, the camera automatically:
1. Calculates the model's bounding box (min/max coordinates)
2. Determines the center point and size of the model
3. Positions the camera at an optimal distance and angle
4. Adjusts the far clipping plane if needed to accommodate large models

## Implementation

### Model Bounding Box Calculation

The `Model` class provides several methods to query geometric properties:

```cpp
// Get the axis-aligned bounding box
glm::vec3 min, max;
bool hasBox = model->GetBoundingBox(min, max);

// Get the center of the bounding box
glm::vec3 center = model->GetCenter();

// Get the size (extent) of the bounding box
glm::vec3 size = model->GetSize();

// Get the radius of the bounding sphere
float radius = model->GetBoundingSphereRadius();
```

**How it works:**
- Iterates through all meshes in the model
- For each mesh, examines all vertex positions
- Tracks minimum and maximum coordinates across all vertices
- Calculates derived properties (center, size, radius) from the bounding box

**File locations:**
- [include/metagfx/scene/Model.h](../include/metagfx/scene/Model.h#L75-L96) - Method declarations
- [src/scene/Model.cpp](../src/scene/Model.cpp#L552-L596) - Implementation

### Camera Framing

The `Camera` class provides a `FrameBoundingBox()` method that automatically positions the camera:

```cpp
// Frame the camera to view a bounding box with 30% margin
camera->FrameBoundingBox(center, size, 1.3f);
```

**Parameters:**
- `center`: Center point of the bounding box (glm::vec3)
- `size`: Size/extent of the bounding box (glm::vec3)
- `marginFactor`: Multiplier for the size (default: 1.3)
  - 1.0 = tight fit (no margin)
  - 1.3 = 30% margin (recommended default)
  - 1.5 = 50% margin (more breathing room)

**Algorithm:**

1. **Apply margin**: Multiply size by margin factor
   ```cpp
   glm::vec3 adjustedSize = size * marginFactor;
   ```

2. **Calculate bounding sphere radius**:
   ```cpp
   float radius = glm::length(adjustedSize) * 0.5f;
   ```

3. **Calculate required distance** using FOV and radius:
   ```cpp
   float fovRadians = glm::radians(m_FOV);
   float distance = radius / std::tan(fovRadians * 0.5f);
   ```
   This ensures the entire bounding sphere fits in the view frustum.

4. **Position camera at 45° yaw, 30° pitch** (front-top-right view):
   ```cpp
   float yawRad = glm::radians(45.0f);
   float pitchRad = glm::radians(30.0f);

   glm::vec3 offset;
   offset.x = distance * cos(pitchRad) * cos(yawRad);
   offset.y = distance * sin(pitchRad);
   offset.z = distance * cos(pitchRad) * sin(yawRad);

   position = center + offset;
   ```

5. **Configure orbital camera** to target the model center:
   ```cpp
   m_OrbitTarget = center;
   m_OrbitDistance = distance;
   m_OrbitYaw = 45.0f;
   m_OrbitPitch = 30.0f;
   ```

6. **Adjust far clipping plane** if needed:
   ```cpp
   if (distance + radius > m_FarPlane * 0.8f) {
       SetPerspective(m_FOV, m_AspectRatio, m_NearPlane, (distance + radius) * 2.0f);
   }
   ```

**File locations:**
- [include/metagfx/scene/Camera.h](../include/metagfx/scene/Camera.h#L50-L56) - Method declaration
- [src/scene/Camera.cpp](../src/scene/Camera.cpp#L235-L280) - Implementation

### Application Integration

The `Application` class automatically frames the camera when loading a model:

```cpp
void Application::LoadModel(const std::string& path) {
    // ... load model ...

    // Automatically frame camera to view the entire model
    glm::vec3 center = m_Model->GetCenter();
    glm::vec3 size = m_Model->GetSize();

    // Frame the camera with 30% margin
    m_Camera->FrameBoundingBox(center, size, 1.3f);
}
```

**File location:**
- [src/app/Application.cpp](../src/app/Application.cpp#L309-L324) - LoadModel method

## Usage Examples

### Example 1: Default Automatic Framing

When you load a model (via keyboard shortcuts 1-4 or file drag-and-drop), the camera automatically frames it:

```cpp
LoadModel("path/to/model.obj");
// Camera is now positioned to view the entire model with 30% margin
```

**Console output:**
```
Loading model: /path/to/model.obj
Model loaded: model.obj
Model bounds - Center: (-0.017, 0.110, -0.001)
Model bounds - Size: (0.155, 0.154, 0.120)
Model bounds - Bounding sphere radius: 0.125
Camera framed at position: (0.223, 0.306, 0.238)
```

### Example 2: Custom Margin

You can adjust the margin by calling `FrameBoundingBox()` directly:

```cpp
// Tight fit (no margin)
camera->FrameBoundingBox(center, size, 1.0f);

// Default (30% margin)
camera->FrameBoundingBox(center, size, 1.3f);

// Extra breathing room (50% margin)
camera->FrameBoundingBox(center, size, 1.5f);
```

### Example 3: Manual Bounding Box Query

You can query model bounds without framing the camera:

```cpp
if (model->IsValid()) {
    glm::vec3 min, max;
    if (model->GetBoundingBox(min, max)) {
        METAGFX_INFO << "Model extends from " << min << " to " << max;
    }

    glm::vec3 center = model->GetCenter();
    METAGFX_INFO << "Model centered at: " << center;

    float radius = model->GetBoundingSphereRadius();
    METAGFX_INFO << "Bounding sphere radius: " << radius;
}
```

## Design Decisions

### Why 45° yaw and 30° pitch?

This viewing angle provides a good balance:
- Shows three faces of the model (front, top, right)
- Provides depth perception (not orthogonal)
- Commonly used in 3D modeling software (Blender, Maya, etc.)
- Works well for most model types (characters, objects, buildings)

### Why use bounding sphere instead of box?

The bounding sphere ensures that the **entire model is visible** from any rotation angle. Using the bounding box alone could cause parts of the model to clip when viewed from diagonal angles.

### Why adjust the far clipping plane?

Some models (especially large architectural scenes) may extend beyond the default far plane (100 units). Automatically adjusting the far plane ensures the entire model is rendered, regardless of size.

### Why 30% margin by default?

Testing with various models showed that:
- 0% margin (tight fit) feels cramped
- 10-20% margin is too tight for comfortable viewing
- 30% margin provides good visual balance
- 40-50% margin makes small models appear too distant

## Model Size Variations

The automatic framing handles models of vastly different scales:

### Very Small Models (e.g., jewelry, insects)
- **Bunny**: ~0.15 units
- **Keys**: ~0.02 units
- Camera positions close (0.2-0.3 units away)

### Medium Models (e.g., characters, furniture)
- **Default range**: 1-10 units
- Camera positions at moderate distance (2-20 units away)

### Large Models (e.g., buildings, vehicles)
- **Helicopter**: 50+ units
- **Building**: 100+ units
- Camera positions far away, far plane adjusted automatically

## Interaction with Orbital Camera

After automatic framing, the camera is configured for orbital controls:

- **Orbit target**: Set to model center
- **Orbit distance**: Set to calculated distance
- **Orbit angles**: Set to 45° yaw, 30° pitch

This allows the user to immediately:
- Click-and-drag to rotate around the model
- Scroll to zoom in/out
- Maintain the model center as the focal point

See [docs/orbital_camera.md](orbital_camera.md) for details on orbital camera controls.

## Future Enhancements

Potential improvements for future versions:

1. **Smart viewing angle**: Analyze model geometry to find the "best" viewing angle (e.g., front-facing for characters)
2. **Margin preference**: Allow users to configure default margin in settings
3. **Animation**: Smooth camera transition when framing (instead of instant jump)
4. **Bounding box visualization**: Debug mode to show the calculated bounding box
5. **Per-axis margin**: Different margins for width, height, and depth
6. **Asymmetric models**: Better handling of models with significant asymmetry

## References

- [Model Loading Documentation](model_loading.md)
- [Camera System Documentation](camera_transformation_system.md)
- [Orbital Camera Documentation](orbital_camera.md)

## Summary

Automatic camera framing provides a user-friendly experience by:
- Eliminating manual camera positioning for each model
- Ensuring the entire model is visible with proper margins
- Positioning the camera at an aesthetically pleasing angle
- Configuring orbital controls for intuitive interaction

The implementation is fully automatic but can be customized by calling `FrameBoundingBox()` with different margin factors.
