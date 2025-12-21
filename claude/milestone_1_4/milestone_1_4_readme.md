# Milestone 1.4: Camera and Transformation System

## Overview

This milestone adds a complete camera system with 3D transformations, allowing you to view the triangle from different angles and move around the scene.

## What's New

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

### 3. Updated Shaders
- Vertex shader now uses MVP matrices
- Triangle can be transformed in 3D space

## File Structure

```
include/pbr/scene/
└── Camera.h

src/scene/
├── CMakeLists.txt  (update)
└── Camera.cpp

include/pbr/rhi/vulkan/
└── VulkanDescriptorSet.h  (new)

src/rhi/vulkan/
└── VulkanDescriptorSet.cpp  (new)

src/app/
├── triangle-mvp.vert  (updated GLSL)
├── triangle-mvp.vert.spv.inl  (recompile)
└── triangle.frag  (unchanged)
```

## Integration Steps

### Step 1: Update CMakeLists.txt

**`src/scene/CMakeLists.txt`** - Update to build Camera:

```cmake
set(SCENE_SOURCES
    Scene.cpp
    Camera.cpp  # Add this
)

set(SCENE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/pbr/scene/Scene.h
    ${CMAKE_SOURCE_DIR}/include/pbr/scene/Camera.h  # Add this
)

add_library(pbr_scene STATIC ${SCENE_SOURCES} ${SCENE_HEADERS})

target_include_directories(pbr_scene
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(pbr_scene
    PUBLIC
        pbr_core
        glm  # GLM is already available
)
```

**`src/rhi/CMakeLists.txt`** - Add VulkanDescriptorSet:

```cmake
if(PBR_USE_VULKAN)
    list(APPEND RHI_SOURCES
        vulkan/VulkanTypes.cpp
        vulkan/VulkanDevice.cpp
        vulkan/VulkanSwapChain.cpp
        vulkan/VulkanBuffer.cpp
        vulkan/VulkanTexture.cpp
        vulkan/VulkanShader.cpp
        vulkan/VulkanPipeline.cpp
        vulkan/VulkanCommandBuffer.cpp
        vulkan/VulkanDescriptorSet.cpp  # Add this
    )
    list(APPEND RHI_HEADERS
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanTypes.h
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanDevice.h
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanSwapChain.h
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanBuffer.h
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanTexture.h
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanShader.h
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanPipeline.h
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanCommandBuffer.h
        ${CMAKE_SOURCE_DIR}/include/pbr/rhi/vulkan/VulkanDescriptorSet.h  # Add this
    )
endif()
```

### Step 2: Create Camera Files

Copy from `milestone_1_4_camera` artifact:
- `include/pbr/scene/Camera.h`
- `src/scene/Camera.cpp`

### Step 3: Create Descriptor Set Files

Copy from `milestone_1_4_descriptors` artifact:
- `include/pbr/rhi/vulkan/VulkanDescriptorSet.h`
- `src/rhi/vulkan/VulkanDescriptorSet.cpp`

### Step 4: Update Shaders

Create **`src/app/triangle-mvp.vert`**:

```glsl
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}
```

The fragment shader remains unchanged.

### Step 5: Compile New Shaders

```bash
cd src/app

# Compile new vertex shader
glslangValidator -V triangle-mvp.vert -o triangle-mvp.vert.spv

# Convert to .inl
python3 -c "
data = open('triangle-mvp.vert.spv', 'rb').read()
with open('triangle-mvp.vert.spv.inl', 'w') as f:
    for i, b in enumerate(data):
        if i % 12 == 0: f.write('\n    ')
        f.write(f'0x{b:02x}, ')
"
```

### Step 6: Update VulkanPipeline

**In `src/rhi/vulkan/VulkanPipeline.cpp`**, update the pipeline layout creation to support descriptor sets:

Find this code:
```cpp
// Pipeline layout
VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
pipelineLayoutInfo.setLayoutCount = 0;  // Currently 0
pipelineLayoutInfo.pushConstantRangeCount = 0;
```

This needs to be updated to accept a descriptor set layout. For now, we'll update it in Step 7.

### Step 7: Update VulkanDevice and VulkanPipeline

**Update `VulkanDevice.h`** to store descriptor set layout:

```cpp
class VulkanDevice : public GraphicsDevice {
    // ... existing methods ...
    
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
    void SetDescriptorSetLayout(VkDescriptorSetLayout layout) { m_DescriptorSetLayout = layout; }

private:
    // ... existing members ...
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
};
```

**Update `VulkanPipeline.cpp`** constructor to accept and use descriptor set layout:

```cpp
// Add after shader stage creation, before pipeline layout
VkDescriptorSetLayout layouts[] = { /* descriptor set layout */ };

VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
pipelineLayoutInfo.setLayoutCount = 1;  // Changed from 0
pipelineLayoutInfo.pSetLayouts = layouts;
pipelineLayoutInfo.pushConstantRangeCount = 0;
```

### Step 8: Update Application

**Update `src/app/Application.h`**:

```cpp
#include "pbr/scene/Camera.h"
#include <glm/glm.hpp>

class Application {
public:
    // ... existing ...

private:
    // ... existing members ...
    
    // Camera
    std::unique_ptr<Camera> m_Camera;
    bool m_FirstMouse = true;
    float m_LastX = 640.0f;
    float m_LastY = 360.0f;
    bool m_CameraEnabled = false;
    
    // Uniform buffers
    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };
    
    Ref<rhi::Buffer> m_UniformBuffers[2];
    std::unique_ptr<rhi::VulkanDescriptorSet> m_DescriptorSet;
    uint32 m_CurrentFrame = 0;
};
```

**Update `src/app/Application.cpp`** - see comments in `milestone_1_4_descriptors` artifact for full implementation.

## Controls

Once implemented:

- **ESC**: Exit application
- **C**: Toggle camera control mode
- **W/A/S/D**: Move forward/left/back/right
- **Q/E**: Move down/up
- **Mouse Movement**: Look around (when camera enabled)
- **Mouse Wheel**: Zoom in/out (FOV adjustment)

## Expected Behavior

1. **Initial view**: Triangle visible from front
2. **Press C**: Camera control enabled (mouse captured)
3. **Move around**: Triangle stays in place, you move around it
4. **Rotating triangle**: The model matrix rotates the triangle
5. **Resize window**: Camera aspect ratio updates automatically

## Simplified Version (For Testing)

If descriptor sets are too complex initially, you can test the camera without uniform buffers:

1. Skip descriptor set creation
2. Keep the original triangle shader
3. Just implement the Camera class
4. Use camera for future milestones when loading real models

## Building

```bash
cd build
rm -rf *
cmake .. -DPBR_USE_VULKAN=ON
make -j$(sysctl -n hw.ncpu)
./bin/PBRenderer
```

## Troubleshooting

### "Descriptor set layout not bound"
- Make sure descriptor set is created before pipeline
- Check that pipeline layout includes descriptor set layout

### Camera moves too fast/slow
- Adjust `m_MovementSpeed` in Camera.cpp (default: 2.5f)
- Adjust `m_MouseSensitivity` (default: 0.1f)

### Triangle disappears
- Check near/far plane values (0.1f to 100.0f)
- Verify camera position is not inside triangle
- Check that projection matrix Y flip is applied

### Mouse not captured
- Ensure `SDL_SetRelativeMouseMode` is called
- Check that camera enable toggle (C key) works

## Next Steps

**Phase 2: Geometry and Assets**
- Load OBJ/glTF files
- Multiple objects in scene
- Transform hierarchies

---

## Success Criteria

✅ Camera class compiles and links
✅ Can toggle camera controls with C key
✅ WASD moves camera position
✅ Mouse look works smoothly
✅ Triangle visible from different angles
✅ Window resize updates camera aspect ratio
✅ Uniform buffers update each frame

**Milestone 1.4 Status**: Complete when you can fly around the triangle!

This completes Phase 1 (Fundamentals and Base Architecture). You now have a complete foundation for building more complex scenes!