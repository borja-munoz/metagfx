# Modern Graphics API Design

## Introduction

Modern graphics APIs (Vulkan, Direct3D 12, Metal, WebGPU) represent a significant shift from older APIs like OpenGL and Direct3D 11. They provide explicit, low-level control over GPU hardware, exposing the true complexity of modern graphics architectures while enabling unprecedented performance and control.

This document explains the fundamental concepts and design patterns shared across these modern APIs.

## Evolution and Philosophy

### From Implicit to Explicit

**Legacy APIs (OpenGL, D3D11)**:
- Hidden state management
- Automatic resource synchronization
- Driver-managed memory
- Implicit pipeline transitions
- Heavy driver overhead

**Modern APIs (Vulkan, D3D12, Metal, WebGPU)**:
- Explicit state management
- Manual synchronization
- Application-managed memory
- Explicit resource barriers
- Minimal driver overhead

### Design Philosophy

Modern APIs follow these principles:

1. **Explicit Control**: Applications manage state, memory, and synchronization
2. **Thin Abstraction**: Minimal layer over hardware capabilities
3. **Multi-threading**: Designed for concurrent command buffer recording
4. **Predictability**: Validation happens at pipeline creation, not draw time
5. **Performance**: Eliminate driver guesswork and hidden costs

## Core Concepts

### 1. Graphics Device

The **device** represents a logical connection to the GPU.

**Responsibilities**:
- Resource creation (buffers, textures, pipelines)
- Memory management
- Queue management
- Capability queries

**Key Operations**:
```cpp
// Vulkan
VkDevice device;
vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);

// Direct3D 12
ID3D12Device* device;
D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));

// Metal
id<MTLDevice> device = MTLCreateSystemDefaultDevice();

// WebGPU
GPUDevice device = await adapter.requestDevice();
```

**Physical vs Logical**:
- **Physical Device**: Actual GPU hardware (Vulkan, WebGPU)
- **Logical Device**: Application's interface to the GPU
- **Adapter**: Hardware enumeration (D3D12, WebGPU)

### 2. Command Recording Model

Modern APIs separate command **recording** from **execution**.

**Command Buffer Lifecycle**:
```
Create → Begin → Record Commands → End → Submit → Execute
```

**Benefits**:
- Multi-threaded recording
- Reusable command buffers
- Minimal submission overhead
- Explicit synchronization

**Example Flow**:
```cpp
// Vulkan
VkCommandBuffer cmd;
vkBeginCommandBuffer(cmd, &beginInfo);
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
vkCmdDraw(cmd, vertexCount, 1, 0, 0);
vkEndCommandBuffer(cmd);
vkQueueSubmit(queue, 1, &submitInfo, fence);

// Direct3D 12
ID3D12GraphicsCommandList* cmdList;
cmdList->Reset(allocator, pipelineState);
cmdList->SetGraphicsRootSignature(rootSignature);
cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
cmdList->DrawInstanced(vertexCount, 1, 0, 0);
cmdList->Close();
commandQueue->ExecuteCommandLists(1, &cmdList);

// Metal
id<MTLCommandBuffer> cmdBuffer = [queue commandBuffer];
id<MTLRenderCommandEncoder> encoder = [cmdBuffer renderCommandEncoderWithDescriptor:desc];
[encoder setRenderPipelineState:pipelineState];
[encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
[encoder endEncoding];
[cmdBuffer commit];

// WebGPU
const commandEncoder = device.createCommandEncoder();
const renderPass = commandEncoder.beginRenderPass(descriptor);
renderPass.setPipeline(pipeline);
renderPass.setVertexBuffer(0, vertexBuffer);
renderPass.draw(vertexCount);
renderPass.end();
device.queue.submit([commandEncoder.finish()]);
```

### 3. Graphics Pipeline

The **pipeline** encapsulates the entire rendering configuration.

**Pipeline State Object (PSO)** includes:
- Shader stages
- Vertex input layout
- Input assembly topology
- Rasterization state
- Depth/stencil state
- Color blend state
- Viewport/scissor (sometimes dynamic)

**Design Benefits**:
- All validation at creation time
- Fast binding at draw time
- GPU can precompile optimal code
- Immutable after creation

**Creation Pattern**:
```cpp
// Vulkan
VkGraphicsPipelineCreateInfo pipelineInfo = {};
pipelineInfo.stageCount = 2;
pipelineInfo.pStages = shaderStages;  // Vertex + Fragment
pipelineInfo.pVertexInputState = &vertexInputInfo;
pipelineInfo.pInputAssemblyState = &inputAssembly;
pipelineInfo.pRasterizationState = &rasterizer;
pipelineInfo.pDepthStencilState = &depthStencil;
pipelineInfo.pColorBlendState = &colorBlending;
vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
```

**Pipeline Stages**:
```
Input Assembly → Vertex Shader → Tessellation → Geometry Shader →
Rasterization → Fragment Shader → Color Blending → Output
```

### 4. Resources

**Resource Types**:
- **Buffers**: Linear memory (vertices, indices, uniforms, storage)
- **Textures/Images**: Multi-dimensional data with formats
- **Samplers**: Texture sampling configuration

**Buffer Usage Patterns**:

```cpp
// Vertex Buffer
BufferDesc vertexBufferDesc = {
    .size = vertexCount * sizeof(Vertex),
    .usage = VERTEX_BUFFER | TRANSFER_DST,
    .memoryType = GPU_ONLY
};

// Index Buffer
BufferDesc indexBufferDesc = {
    .size = indexCount * sizeof(uint32_t),
    .usage = INDEX_BUFFER | TRANSFER_DST,
    .memoryType = GPU_ONLY
};

// Uniform Buffer
BufferDesc uniformBufferDesc = {
    .size = sizeof(UniformData),
    .usage = UNIFORM_BUFFER,
    .memoryType = CPU_TO_GPU  // Updated frequently
};

// Storage Buffer
BufferDesc storageBufferDesc = {
    .size = dataSize,
    .usage = STORAGE_BUFFER | TRANSFER_SRC | TRANSFER_DST,
    .memoryType = GPU_ONLY
};
```

**Memory Management**:

Modern APIs require explicit memory management:

1. **Allocate Memory**: Request memory from device
2. **Bind Resources**: Associate buffers/textures with memory
3. **Map/Unmap**: Access CPU-visible memory
4. **Transfer**: Copy between memory types

**Memory Types**:
- **Device Local**: GPU-only, fastest (VRAM)
- **Host Visible**: CPU can write, GPU can read
- **Host Coherent**: Automatic synchronization
- **Host Cached**: Cached on CPU side

### 5. Shaders

**Shader Stages**:

1. **Vertex Shader**: Per-vertex processing
   - Transform positions
   - Calculate per-vertex data
   - Prepare interpolation inputs

2. **Fragment Shader**: Per-pixel processing
   - Calculate final color
   - Texture sampling
   - Lighting calculations

3. **Compute Shader**: General-purpose GPU computation
   - Parallel processing
   - No fixed pipeline
   - Arbitrary data structures

4. **Geometry Shader**: Optional primitive processing
   - Generate/discard primitives
   - Point sprites, line expansion

5. **Tessellation Shaders**: Subdivision surfaces
   - Control shader: Tessellation factors
   - Evaluation shader: Generate vertices

**Shader Languages**:

| API | Primary Language | Alternative |
|-----|-----------------|-------------|
| Vulkan | SPIR-V (binary) | GLSL, HLSL → SPIR-V |
| Direct3D 12 | HLSL (DXIL) | - |
| Metal | Metal Shading Language | - |
| WebGPU | WGSL | SPIR-V (deprecated) |

**Shader Compilation Pipeline**:

```
Source Code (GLSL/HLSL/MSL)
    ↓
Compiler (glslangValidator/DXC/Metal Compiler)
    ↓
Intermediate Representation (SPIR-V/DXIL/AIR)
    ↓
Runtime Compilation (optional)
    ↓
GPU-Specific Machine Code
```

**SPIR-V Example** (Vulkan):
```cpp
// GLSL Source
#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
}

// Compile to SPIR-V
// glslangValidator -V shader.vert -o shader.vert.spv

// Load in application
std::vector<uint32_t> spirvCode = loadFile("shader.vert.spv");
VkShaderModuleCreateInfo createInfo = {};
createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
createInfo.pCode = spirvCode.data();
vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
```

**Shader Reflection**:
- Inspect shader inputs/outputs
- Discover resource bindings
- Validate pipeline compatibility

### 6. Binding Model

The **binding model** connects CPU-side resources to GPU-side shaders.

**Descriptor Sets** (Vulkan):
```cpp
// Layout: Defines what resources shader expects
VkDescriptorSetLayoutBinding bindings[] = {
    {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    },
    {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    }
};

// Set: Actual resource bindings
VkDescriptorBufferInfo bufferInfo = {
    .buffer = uniformBuffer,
    .offset = 0,
    .range = sizeof(UniformData)
};

VkDescriptorImageInfo imageInfo = {
    .sampler = sampler,
    .imageView = textureView,
    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
};

VkWriteDescriptorSet writes[] = {
    {
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &bufferInfo
    },
    {
        .dstSet = descriptorSet,
        .dstBinding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo
    }
};

vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

// Bind at draw time
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
```

**Root Signature** (Direct3D 12):
```cpp
// Define parameter slots
D3D12_ROOT_PARAMETER parameters[2];
parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;  // Constant buffer
parameters[0].Descriptor.ShaderRegister = 0;
parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;  // SRV
parameters[1].DescriptorTable.NumDescriptorRanges = 1;

D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
rootSigDesc.NumParameters = 2;
rootSigDesc.pParameters = parameters;

// Bind at draw time
cmdList->SetGraphicsRootConstantBufferView(0, constantBufferAddress);
cmdList->SetGraphicsRootDescriptorTable(1, descriptorHeap->GetGPUDescriptorHandleForHeapStart());
```

**Binding Frequency Patterns**:
- **Per-Frame**: Camera matrices, time, global state (set 0)
- **Per-Material**: Textures, material properties (set 1)
- **Per-Object**: Transform matrices (set 2 or push constants)

### 7. Swap Chain

The **swap chain** manages the presentation surface and back buffers.

**Purpose**:
- Provide images for rendering
- Present completed frames to screen
- Handle v-sync and frame pacing
- Manage window resize

**Double/Triple Buffering**:
```
Frame N-1: Presenting to screen
Frame N:   GPU rendering
Frame N+1: CPU preparing commands
```

**Presentation Modes**:
- **Immediate**: No v-sync, possible tearing
- **FIFO**: V-synced, queue fills up
- **Mailbox**: V-synced, replaces queued image
- **FIFO Relaxed**: Adaptive v-sync

**Typical Flow**:
```cpp
// Vulkan
uint32_t imageIndex;
vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
                      imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

// Render to swapChain->images[imageIndex]
recordCommandBuffer(imageIndex);
vkQueueSubmit(queue, &submitInfo, renderFinishedFence);

VkPresentInfoKHR presentInfo = {};
presentInfo.swapchainCount = 1;
presentInfo.pSwapchains = &swapChain;
presentInfo.pImageIndices = &imageIndex;
vkQueuePresentKHR(presentQueue, &presentInfo);
```

### 8. Synchronization

Modern APIs require explicit synchronization between GPU operations.

**Synchronization Primitives**:

1. **Fences**: CPU waits for GPU
   ```cpp
   vkQueueSubmit(queue, &submitInfo, fence);
   vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
   ```

2. **Semaphores**: GPU waits for GPU (between queues)
   ```cpp
   submitInfo.waitSemaphoreCount = 1;
   submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
   submitInfo.signalSemaphoreCount = 1;
   submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
   ```

3. **Barriers**: Resource state transitions
   ```cpp
   VkImageMemoryBarrier barrier = {};
   barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   barrier.srcAccessMask = 0;
   barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
   vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr,
                        0, nullptr, 1, &barrier);
   ```

4. **Events**: Fine-grained GPU-GPU sync within command buffer

**Resource States** (D3D12):
```cpp
// Transition resource state
D3D12_RESOURCE_BARRIER barrier = {};
barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
barrier.Transition.pResource = resource;
barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
cmdList->ResourceBarrier(1, &barrier);
```

## API Comparison

### Vulkan

**Strengths**:
- Cross-platform (Windows, Linux, Android, macOS via MoltenVK)
- Explicit and powerful
- Excellent tooling (RenderDoc, validation layers)
- Industry standard

**Characteristics**:
- Most verbose API
- SPIR-V intermediate representation
- Explicit memory management
- Extensive validation layers

**Best For**: Cross-platform applications, maximum control

### Direct3D 12

**Strengths**:
- Native Windows support
- Excellent Xbox integration
- PIX profiling tools
- Similar philosophy to Vulkan

**Characteristics**:
- HLSL shading language
- Root signatures for binding
- Resource heaps
- Command lists and bundles

**Best For**: Windows-exclusive applications, Xbox development

### Metal

**Strengths**:
- Optimized for Apple hardware
- Simpler than Vulkan/D3D12
- Excellent performance on Apple devices
- Unified memory architecture benefits

**Characteristics**:
- Metal Shading Language (MSL)
- Cleaner API design
- Automatic memory management (more implicit)
- Command encoders

**Best For**: iOS, macOS exclusive applications

### WebGPU

**Strengths**:
- Web browser support
- Portable to native (via Dawn/wgpu)
- Modern design learning from predecessors
- Safe by design

**Characteristics**:
- WGSL shading language
- Promise-based async operations
- Sandboxed security model
- Simplified compared to Vulkan

**Best For**: Web applications, cross-platform with safety guarantees

## Common Patterns

### Frame Rendering Loop

```cpp
void RenderFrame() {
    // 1. Wait for previous frame
    WaitForFence(frameFence[currentFrame]);

    // 2. Acquire next swap chain image
    uint32_t imageIndex = AcquireNextImage(swapChain);

    // 3. Update resources
    UpdateUniformBuffer(currentFrame);

    // 4. Record commands
    CommandBuffer cmd = commandBuffers[currentFrame];
    cmd->Begin();
    cmd->BeginRenderPass(swapChainImage[imageIndex]);
    cmd->BindPipeline(pipeline);
    cmd->BindDescriptorSet(descriptorSet[currentFrame]);
    cmd->BindVertexBuffer(vertexBuffer);
    cmd->Draw(vertexCount);
    cmd->EndRenderPass();
    cmd->End();

    // 5. Submit commands
    SubmitCommandBuffer(cmd, frameFence[currentFrame]);

    // 6. Present
    PresentImage(swapChain, imageIndex);

    // 7. Advance frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

### Resource Upload Pattern

```cpp
// Staging buffer pattern (GPU-only resources)
void UploadMeshData(const MeshData& data) {
    // 1. Create staging buffer (CPU-visible)
    Buffer stagingBuffer = CreateBuffer(
        data.size,
        TRANSFER_SRC,
        CPU_TO_GPU
    );

    // 2. Copy data to staging buffer
    void* mapped = stagingBuffer->Map();
    memcpy(mapped, data.vertices, data.size);
    stagingBuffer->Unmap();

    // 3. Create device-local buffer (GPU-only)
    Buffer deviceBuffer = CreateBuffer(
        data.size,
        VERTEX_BUFFER | TRANSFER_DST,
        GPU_ONLY
    );

    // 4. Copy staging → device buffer
    CommandBuffer cmd = CreateCommandBuffer();
    cmd->Begin();
    cmd->CopyBuffer(stagingBuffer, deviceBuffer, data.size);
    cmd->End();
    SubmitAndWait(cmd);

    // 5. Destroy staging buffer
    stagingBuffer->Destroy();
}
```

### Pipeline Cache Pattern

```cpp
// Save pipeline compilation results
PipelineCache cache = LoadPipelineCache("pipeline_cache.bin");

Pipeline pipeline = CreateGraphicsPipeline(pipelineDesc, cache);

// Save for next run
SavePipelineCache(cache, "pipeline_cache.bin");
```

## Best Practices

### Performance

1. **Batch Draw Calls**: Minimize state changes
2. **Instancing**: Render multiple objects in one call
3. **Indirect Drawing**: GPU-driven rendering
4. **Async Compute**: Overlap graphics and compute
5. **Multi-threaded Recording**: Parallel command buffer building

### Memory Management

1. **Pool Allocations**: Reduce allocation overhead
2. **Suballocation**: Single allocation for multiple resources
3. **Memory Budgets**: Track and limit memory usage
4. **Staging Buffers**: Efficient CPU→GPU transfers
5. **Persistent Mapping**: Keep buffers mapped when frequently updated

### Synchronization

1. **Frame Pacing**: Limit frames in flight (typically 2-3)
2. **Coarse Barriers**: Minimize pipeline stalls
3. **Async Transfers**: Separate transfer queue
4. **Timeline Semaphores**: More flexible synchronization

### Validation and Debugging

1. **Validation Layers**: Enable during development (Vulkan)
2. **Debug Names**: Label all resources for tools
3. **GPU Captures**: Use RenderDoc, PIX, Xcode GPU Debugger
4. **Profiling**: Measure GPU time, memory bandwidth

## MetaGFX RHI Design

The MetaGFX RHI abstracts these concepts:

**Unified Abstractions**:
- `GraphicsDevice` → Vulkan Device, D3D12 Device, Metal Device
- `CommandBuffer` → Command buffer/list/encoder
- `Pipeline` → PSO/RenderPipelineState
- `Buffer` → Buffer resource
- `SwapChain` → Swap chain/Drawable

**Design Decisions**:
- SPIR-V as shader interchange format
- Explicit resource barriers where needed
- Ref-counted resources for safety
- Modern command recording model
- Descriptor set abstraction

**Benefits**:
- Write once, run on any backend
- Learn modern graphics concepts once
- Optimal performance per platform
- Future-proof architecture

## Further Reading

### Official Documentation
- [Vulkan Specification](https://www.khronos.org/vulkan/)
- [Direct3D 12 Programming Guide](https://docs.microsoft.com/en-us/windows/win32/direct3d12)
- [Metal Documentation](https://developer.apple.com/metal/)
- [WebGPU Specification](https://www.w3.org/TR/webgpu/)

### Learning Resources
- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [Learning DirectX 12](https://www.3dgep.com/learning-directx-12-1/)
- [Metal By Example](https://metalbyexample.com/)
- [WebGPU Fundamentals](https://webgpufundamentals.org/)

### Tools
- **RenderDoc**: Graphics debugger (Vulkan, D3D12)
- **PIX**: DirectX 12 profiler and debugger
- **Xcode GPU Debugger**: Metal debugging
- **Chrome DevTools**: WebGPU debugging
