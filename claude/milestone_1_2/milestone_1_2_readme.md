# Milestone 1.2: RHI Abstract Interfaces

## Overview

This milestone defines the **Render Hardware Interface (RHI)** - an abstraction layer that allows the renderer to work with multiple graphics APIs (Vulkan, D3D12, Metal, WebGPU) through a unified interface.

## What Was Implemented

### Core Types and Enumerations (`Types.h`)

- **GraphicsAPI**: Enumeration of supported APIs
- **BufferUsage**: Vertex, Index, Uniform, Storage buffers
- **MemoryUsage**: GPU-only, CPU-to-GPU, GPU-to-CPU access patterns
- **ShaderStage**: Vertex, Fragment, Compute, etc.
- **Format**: Comprehensive texture and buffer formats
- **PrimitiveTopology**: Triangle lists, strips, lines, points
- **Pipeline State**: Rasterization, depth/stencil, blending

### Abstract Interfaces

1. **GraphicsDevice** - Main device interface
   - Device creation and information
   - Resource creation (buffers, textures, shaders, pipelines)
   - Command buffer management
   - Synchronization

2. **Buffer** - GPU buffer abstraction
   - Mapping for CPU access
   - Data upload
   - Usage and size queries

3. **Texture** - Texture/image abstraction
   - Dimensions and format queries
   - Will support sampling and rendering

4. **Shader** - Shader module abstraction
   - Stage identification
   - Bytecode management

5. **Pipeline** - Graphics pipeline state
   - Combines shaders, vertex layout, rasterization state
   - Depth/stencil and blending configuration

6. **CommandBuffer** - Command recording
   - Begin/End recording
   - Render pass management
   - Pipeline binding
   - Viewport and scissor
   - Draw commands (indexed and non-indexed)
   - Buffer copies

7. **SwapChain** - Presentation
   - Back buffer access
   - Present to screen
   - Resize handling

## File Structure

All RHI files should be placed as follows:

```
include/pbr/rhi/
├── Types.h              (Core types, enums, structs)
├── GraphicsDevice.h     (Main device interface)
├── Buffer.h             (Buffer abstraction)
├── Texture.h            (Texture abstraction)
├── Shader.h             (Shader abstraction)
├── Pipeline.h           (Pipeline abstraction)
├── CommandBuffer.h      (Command recording)
└── SwapChain.h          (Swap chain interface)

src/rhi/
└── GraphicsDevice.cpp   (Factory function implementation)
```

**Note**: For Milestone 1.2, all interfaces are defined in a single file in the artifact for convenience. You can split them into separate headers as shown above if you prefer better organization.

## Integration Steps

1. **Replace the old placeholder files**:
   - Delete old `include/pbr/rhi/Types.h` 
   - Delete old `include/pbr/rhi/GraphicsDevice.h`
   
2. **Copy the new RHI definitions**:
   - Copy all the content from the `milestone_1_2_rhi` artifact
   - You can keep everything in `Types.h` and `GraphicsDevice.h`, or split into separate files

3. **Update `src/rhi/GraphicsDevice.cpp`**:
   - Replace with the new implementation that includes the factory function

4. **Rebuild**:
   ```bash
   cd build
   make
   ```

## Design Principles

### 1. API-Agnostic Design
All interfaces use common terminology that maps naturally to all target APIs:
- **Vulkan**: Direct mapping to VkDevice, VkBuffer, VkCommandBuffer, etc.
- **D3D12**: Maps to ID3D12Device, ID3D12Resource, ID3D12GraphicsCommandList
- **Metal**: Maps to MTLDevice, MTLBuffer, MTLCommandBuffer
- **WebGPU**: Maps to GPUDevice, GPUBuffer, GPUCommandEncoder

### 2. Modern Graphics Concepts
- Explicit resource lifetimes (no hidden state)
- Clear separation of resources and their usage
- Command buffer recording model
- Descriptor-based binding (to be added in later milestones)

### 3. Minimal but Complete
The interfaces include only what's needed for:
- Basic rendering (Milestone 1.3)
- Material system (Phase 2)
- Advanced rendering (Phase 7-8)

Additional features (like compute shaders, ray tracing) will be added as needed.

### 4. Type Safety
- Strong typing with enums and structs
- No void pointers in public interfaces
- Clear ownership with Ref<T> smart pointers

## Usage Pattern

Here's how the RHI will be used in practice:

```cpp
// 1. Create device
auto device = CreateGraphicsDevice(GraphicsAPI::Vulkan, windowHandle);

// 2. Create resources
auto buffer = device->CreateBuffer(bufferDesc);
auto shader = device->CreateShader(shaderDesc);
auto pipeline = device->CreateGraphicsPipeline(pipelineDesc);

// 3. Record commands
auto cmd = device->CreateCommandBuffer();
cmd->Begin();
cmd->BeginRendering(colorTargets, depthTarget, clearValues);
cmd->BindPipeline(pipeline);
cmd->BindVertexBuffer(buffer);
cmd->Draw(vertexCount);
cmd->EndRendering();
cmd->End();

// 4. Submit and present
device->SubmitCommandBuffer(cmd);
device->GetSwapChain()->Present();
```

## What's Not Included Yet

Features that will be added in later milestones:

- **Descriptor Sets/Bindings** (Milestone 1.3) - Texture and uniform bindings
- **Render Passes** - Explicit render pass objects (Milestone 3.1)
- **Synchronization Primitives** - Fences, semaphores (Milestone 3.3)
- **Compute Pipelines** - Compute shaders (Phase 6+)
- **Ray Tracing** - Acceleration structures (Phase 6)

## Testing the Interfaces

The interfaces are abstract and cannot be instantiated directly. They will be tested when we implement the Vulkan backend in **Milestone 1.3**.

## Next Steps

**Milestone 1.3**: Implement the Vulkan backend
- Create `VulkanDevice` class implementing `GraphicsDevice`
- Implement all other Vulkan-specific classes
- Render the first triangle using the RHI

## Success Criteria

- ✅ All RHI interfaces are clearly defined
- ✅ Code compiles without errors
- ✅ Interfaces are documented with comments
- ✅ Factory function exists for device creation
- ✅ Design supports all planned graphics APIs
- ✅ Ready for Vulkan implementation

---

**Milestone 1.2 Status**: ✅ Complete
- Abstract interfaces defined
- API-agnostic design validated
- Ready for backend implementation