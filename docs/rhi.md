# RHI Abstract Interfaces

The **Render Hardware Interface (RHI)** is an abstraction layer that allows the renderer to work with multiple graphics APIs (Vulkan, D3D12, Metal, WebGPU) through a unified interface.

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

## Core Types and Enumerations (`Types.h`)

- **GraphicsAPI**: Enumeration of supported APIs
- **BufferUsage**: Vertex, Index, Uniform, Storage buffers
- **MemoryUsage**: GPU-only, CPU-to-GPU, GPU-to-CPU access patterns
- **ShaderStage**: Vertex, Fragment, Compute, etc.
- **Format**: Comprehensive texture and buffer formats
- **PrimitiveTopology**: Triangle lists, strips, lines, points
- **Pipeline State**: Rasterization, depth/stencil, blending

## Abstract Interfaces

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
   - Supports sampling and rendering targets
   - Depth textures for shadow mapping

4. **Sampler** - Texture sampling configuration
   - Filtering modes (linear, nearest)
   - Address modes (repeat, clamp, mirror)
   - Comparison samplers for shadow mapping

5. **Framebuffer** - Render target abstraction
   - Color and depth attachments
   - Used for off-screen rendering (shadow maps, post-processing)
   - Depth-only framebuffers for shadow mapping

6. **Shader** - Shader module abstraction
   - Stage identification
   - Bytecode management

7. **Pipeline** - Graphics pipeline state
   - Combines shaders, vertex layout, rasterization state
   - Depth/stencil and blending configuration

8. **CommandBuffer** - Command recording
   - Begin/End recording
   - Render pass management
   - Pipeline binding
   - Viewport and scissor
   - Draw commands (indexed and non-indexed)
   - Buffer copies

9. **SwapChain** - Presentation
   - Back buffer access
   - Present to screen
   - Resize handling

## File Structure

```
include/metagfx/rhi/
├── Types.h              (Core types, enums, structs)
├── GraphicsDevice.h     (Main device interface)
├── Buffer.h             (Buffer abstraction)
├── Texture.h            (Texture abstraction)
├── Sampler.h            (Sampler abstraction)
├── Framebuffer.h        (Framebuffer abstraction)
├── Shader.h             (Shader abstraction)
├── Pipeline.h           (Pipeline abstraction)
├── CommandBuffer.h      (Command recording)
└── SwapChain.h          (Swap chain interface)

src/rhi/
└── GraphicsDevice.cpp   (Factory function implementation)
```