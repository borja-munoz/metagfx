# Camera and Transformation System

## Overview

Complete camera system with 3D transformations, allowing you to view the triangle from different angles and move around the scene.

### 1. Camera Class
- Perspective and orthographic projection
- View matrix calculation
- Position and rotation control
- FPS-style camera movement (WASD + QE)
- Mouse look controls
- Mouse wheel zoom

### 2. Uniform Buffers
- MVP (Model-View-Projection) matrix support
- Double-buffered uniform buffers
- Descriptor sets for binding uniforms to shaders

### 3. Shaders
- Vertex shader uses MVP matrices
- Triangle can be transformed in 3D space

## File Structure

```
include/metagfx/scene/
└── Camera.h

src/scene/
├── CMakeLists.txt  (update)
└── Camera.cpp

include/metagfx/rhi/vulkan/
└── VulkanDescriptorSet.h  (new)

src/rhi/vulkan/
└── VulkanDescriptorSet.cpp  (new)

src/app/
├── triangle.vert  
└── triangle.frag  
```