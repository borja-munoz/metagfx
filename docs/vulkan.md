# Vulkan implementation

Complete Vulkan backend for the RHI.

## Key Implementation Details

### Synchronization

The swap chain uses double-buffering with:
- **Image available semaphore**: Signals when image is ready to render
- **Render finished semaphore**: Signals when rendering is complete
- **In-flight fence**: Prevents CPU from getting too far ahead

### Memory Management

- Buffers use simple memory allocation (one allocation per buffer)
- Memory type selection based on usage flags
- For production, consider using VMA (Vulkan Memory Allocator)

### Render Pass

MetaGFX currently uses the **legacy Vulkan render pass API** (`vkCmdBeginRenderPass` / `vkCmdEndRenderPass`), not the modern dynamic rendering extension (`VK_KHR_dynamic_rendering`).

**Current Implementation:**
- Render pass and framebuffer are created dynamically per-frame in `BeginRendering()`
- Simple single-subpass configuration with one color attachment
- Clear on load (`VK_ATTACHMENT_LOAD_OP_CLEAR`), store on finish (`VK_ATTACHMENT_STORE_OP_STORE`)
- Render pass and framebuffer are destroyed in the command buffer destructor

**Design Rationale:**
The abstract RHI uses forward-thinking API names (`BeginRendering()` / `EndRendering()`) that mirror Vulkan 1.3's dynamic rendering, but the Vulkan backend implements this using traditional render passes for maximum compatibility. This approach:
- Works on all Vulkan 1.0+ implementations, including MoltenVK on macOS
- Provides a clean migration path to dynamic rendering in the future
- Keeps the RHI abstraction modern and future-proof

**Trade-offs:**
- Creating render passes and framebuffers per-frame has some CPU overhead
- Dynamic rendering (Vulkan 1.3+) would be slightly more efficient and eliminate this overhead
- For production, consider either caching render passes/framebuffers or migrating to `VK_KHR_dynamic_rendering`

**Future Optimization:**
A future milestone may add dynamic rendering support as an optional code path, using the legacy render pass API as a fallback for older drivers and MoltenVK.

### Pipeline State

- Vertex input layout matches shader inputs
- Dynamic viewport and scissor
- No descriptor sets yet (will add in Phase 2)
- Simple rasterization state

## Vulkan Backend Classes

1. **VulkanDevice** - Main device implementation
   - Instance and device creation
   - Queue management
   - Command pool
   - Resource creation

2. **VulkanSwapChain** - Presentation
   - Swap chain creation and management
   - Image acquisition and presentation
   - Synchronization primitives
   - Resize handling

3. **VulkanBuffer** - GPU buffers
   - Buffer creation and memory allocation
   - CPU-GPU data transfer
   - Memory mapping

4. **VulkanTexture** - Images
   - Wrap swap chain images
   - Image views
   - (Custom texture creation stubbed for later)

5. **VulkanShader** - Shader modules
   - SPIR-V shader module creation
   - Entry point management

6. **VulkanPipeline** - Graphics pipeline
   - Complete pipeline state
   - Vertex input layout
   - Rasterization, depth, blending states
   - Dynamic viewport and scissor

7. **VulkanCommandBuffer** - Command recording
   - Render pass management
   - Draw commands
   - Pipeline binding
   - Viewport and scissor setting

## File Structure

```
include/metagfx/rhi/vulkan/
├── VulkanTypes.h
├── VulkanDevice.h
├── VulkanSwapChain.h
├── VulkanBuffer.h
├── VulkanTexture.h
├── VulkanShader.h
├── VulkanPipeline.h
└── VulkanCommandBuffer.h

src/rhi/vulkan/
├── VulkanTypes.cpp
├── VulkanDevice.cpp
├── VulkanSwapChain.cpp
├── VulkanBuffer.cpp
├── VulkanTexture.cpp
├── VulkanShader.cpp
├── VulkanPipeline.cpp
└── VulkanCommandBuffer.cpp

src/app/
├── triangle.vert           (GLSL source)
├── triangle.frag           (GLSL source)
├── triangle.vert.spv.inl   (Compiled SPIR-V)
└── triangle.frag.spv.inl   (Compiled SPIR-V)
```

## Shader Setup

1. Create the GLSL Shaders

2. Compile Shaders to SPIR-V

Compile the GLSL shaders to SPIR-V bytecode using `glslangValidator` (comes with Vulkan SDK):

```bash
cd src/app

# Compile vertex shader
glslangValidator -V triangle.vert -o triangle.vert.spv

# Compile fragment shader
glslangValidator -V triangle.frag -o triangle.frag.spv
```

3. Convert SPIR-V to C++ Include Files

Using Python:

```bash
chmod +x convert_spv.py
python3 convert_spv.py triangle.vert.spv triangle.vert.spv.inl
python3 convert_spv.py triangle.frag.spv triangle.frag.spv.inl
```

The `.inl` files will contain arrays of bytes like:

**`triangle.vert.spv.inl`:**
```cpp
0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00, 0x0b, 0x00, 0x08, 0x00,
0x2e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x02, 0x00,
// ... many more bytes ...
```
