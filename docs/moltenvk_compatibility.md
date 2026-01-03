# MoltenVK Compatibility Guide

This document describes platform-specific compatibility issues and solutions when running MetaGFX on Apple Silicon (macOS) using MoltenVK.

## Overview

MoltenVK is a Vulkan implementation layered on top of Apple's Metal API. While it provides excellent Vulkan compatibility on macOS and iOS, it has certain limitations compared to native Vulkan drivers on Windows and Linux.

## Storage Buffers vs Uniform Buffers

### The Problem

MoltenVK has a significant limitation with **uniform buffers larger than 256 bytes**. On Apple Silicon:

- Uniform buffers are implemented using Metal's constant buffers
- Metal constant buffers have a maximum size of **64KB** but practical limits are much lower
- Large uniform buffers (>256 bytes) may fail to transfer data correctly from CPU to GPU
- This manifests as GPU reading zeros instead of the actual data written by the CPU

In MetaGFX, this affected the **light buffer** which contains:
- Light count (4 bytes)
- Padding (12 bytes)
- Light data array (16 lights × 64 bytes = 1024 bytes)
- **Total: 1040 bytes** (far exceeding the 256-byte safe limit)

### The Solution

Use **storage buffers** (`VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`) instead of uniform buffers for large data structures.

Storage buffers:
- Are implemented using Metal's buffer storage (not constant buffers)
- Support much larger sizes (up to device limits, typically GBs)
- Use `std430` layout (more efficient packing than `std140`)
- Are read-write by default, but can be marked `readonly` in shaders
- Have the same performance characteristics as uniform buffers for read-only access

## Implementation Changes

### 1. Descriptor Type Change

**Application.cpp** - Change descriptor binding type:

```cpp
// BEFORE (uniform buffer - does not work on MoltenVK for large buffers)
{
    3,  // binding = 3 (Light buffer)
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    m_Scene->GetLightBuffer(),
    nullptr,
    nullptr
}

// AFTER (storage buffer - MoltenVK compatible)
{
    3,  // binding = 3 (Light buffer) - CHANGED TO STORAGE BUFFER for MoltenVK compatibility
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    VK_SHADER_STAGE_FRAGMENT_BIT,
    m_Scene->GetLightBuffer(),
    nullptr,
    nullptr
}
```

### 2. Shader Layout Change

**model.frag** - Change from `uniform` to `buffer` with `std430` layout:

```glsl
// BEFORE (uniform buffer with std140 - does not work on MoltenVK)
layout(set = 0, binding = 3, std140) uniform LightBuffer {
    uint lightCount;
    uint padding[3];
    LightData lights[16];
} lightBuffer;

// AFTER (storage buffer with std430 - MoltenVK compatible)
layout(set = 0, binding = 3, std430) readonly buffer LightBuffer {
    uint lightCount;
    uint padding[3];
    LightData lights[16];
} lightBuffer;
```

**Key differences:**
- `uniform` → `buffer` (or `readonly buffer` for read-only access)
- `std140` → `std430` layout qualifier
- Storage buffers use more efficient packing rules (std430)

### 3. Descriptor Set Handling

**VulkanDescriptorSet.cpp** - Support storage buffers in descriptor updates:

```cpp
// Handle both uniform and storage buffers
if (binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
    binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {

    auto vkBuffer = std::static_pointer_cast<VulkanBuffer>(binding.buffer);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = vkBuffer->GetHandle();
    bufferInfo.offset = 0;
    bufferInfo.range = vkBuffer->GetSize();
    bufferInfos.push_back(bufferInfo);

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_DescriptorSets[i];
    descriptorWrite.dstBinding = binding.binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = binding.type;  // Correct type
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfos.back();

    descriptorWrites.push_back(descriptorWrite);
}
```

### 4. Memory Synchronization

Storage buffers require explicit memory synchronization when using CPU-visible memory.

**VulkanBuffer.cpp** - Flush mapped memory ranges:

```cpp
void VulkanBuffer::CopyData(const void* data, uint64 size, uint64 offset) {
    void* mapped = Map();
    memcpy(static_cast<char*>(mapped) + offset, data, size);

    // Flush mapped memory to make writes visible to GPU
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = m_Memory;
    range.offset = offset;
    range.size = size;
    VK_CHECK(vkFlushMappedMemoryRanges(m_Context.device, 1, &range));

    Unmap();
}
```

**Application.cpp** - Add pipeline barrier before rendering:

```cpp
// CRITICAL: Add buffer memory barrier to ensure light buffer writes are visible to GPU
auto lightBuffer = m_Scene->GetLightBuffer();
if (lightBuffer) {
    auto vkLightBuffer = std::static_pointer_cast<VulkanBuffer>(lightBuffer);
    vkCmd->BufferMemoryBarrier(
        vkLightBuffer->GetHandle(),
        0,  // offset
        vkLightBuffer->GetSize(),  // size
        VK_PIPELINE_STAGE_HOST_BIT,  // srcStage: CPU writes
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  // dstStage: Fragment shader reads
        VK_ACCESS_HOST_WRITE_BIT,  // srcAccess: CPU writes
        VK_ACCESS_UNIFORM_READ_BIT  // dstAccess: Shader uniform reads
    );
}
```

**VulkanCommandBuffer.cpp** - Implement buffer memory barrier:

```cpp
void VulkanCommandBuffer::BufferMemoryBarrier(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size,
                                             VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                             VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.offset = offset;
    barrier.size = size;

    vkCmdPipelineBarrier(
        m_CommandBuffer,
        srcStage,
        dstStage,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr
    );
}
```

## Layout Differences: std140 vs std430

### std140 Layout (Uniform Buffers)

Strict alignment rules designed for uniform buffers:
- Scalars: aligned to 4 bytes
- vec2: aligned to 8 bytes
- vec3/vec4: aligned to 16 bytes
- Arrays: each element aligned to 16 bytes
- Structs: aligned to 16 bytes

Example:
```glsl
layout(std140) uniform Data {
    float a;        // offset 0, size 4
    // padding: 12 bytes
    vec3 b;         // offset 16, size 12
    // padding: 4 bytes
    float c;        // offset 32, size 4
};  // total: 36 bytes, but allocated as 48 bytes (aligned to 16)
```

### std430 Layout (Storage Buffers)

Efficient packing rules designed for storage buffers:
- Scalars: aligned to 4 bytes
- vec2: aligned to 8 bytes
- vec3: aligned to 16 bytes
- vec4: aligned to 16 bytes
- Arrays: each element aligned to its base type
- Structs: aligned to largest member

Example:
```glsl
layout(std430) buffer Data {
    float a;        // offset 0, size 4
    vec3 b;         // offset 16, size 12 (aligned to 16)
    float c;        // offset 28, size 4
};  // total: 32 bytes (no extra padding)
```

### LightData Structure Layout

Our `LightData` struct uses vec4 exclusively, so std140 and std430 produce identical layouts:

```cpp
struct LightData {
    glm::vec4 positionAndType;   // offset 0, size 16
    glm::vec4 directionAndRange; // offset 16, size 16
    glm::vec4 colorAndIntensity; // offset 32, size 16
    glm::vec4 spotAngles;        // offset 48, size 16
};  // total: 64 bytes (identical in std140 and std430)
```

## CPU-GPU Struct Alignment

**CRITICAL**: CPU and GPU struct definitions must match exactly.

### CPU Side (C++)

```cpp
// include/metagfx/scene/Light.h
struct LightData {
    glm::vec4 positionAndType;   // 16 bytes
    glm::vec4 directionAndRange; // 16 bytes
    glm::vec4 colorAndIntensity; // 16 bytes
    glm::vec4 spotAngles;        // 16 bytes
};  // 64 bytes total

struct LightBuffer {
    uint32 lightCount;      // 4 bytes
    uint32 padding[3];      // 12 bytes (align to 16 bytes)
    LightData lights[16];   // 1024 bytes (16 × 64)
};  // 1040 bytes total
```

### GPU Side (GLSL)

```glsl
// src/app/model.frag
struct LightData {
    vec4 positionAndType;    // 16 bytes
    vec4 directionAndRange;  // 16 bytes
    vec4 colorAndIntensity;  // 16 bytes
    vec4 spotAngles;         // 16 bytes
};  // 64 bytes total

layout(set = 0, binding = 3, std430) readonly buffer LightBuffer {
    uint lightCount;        // 4 bytes
    uint padding[3];        // 12 bytes (align to 16 bytes)
    LightData lights[16];   // 1024 bytes (16 × 64)
} lightBuffer;  // 1040 bytes total
```

**Verification**: Use `sizeof()` and `offsetof()` to verify alignment:

```cpp
static_assert(sizeof(LightData) == 64, "LightData size mismatch");
static_assert(offsetof(LightData, positionAndType) == 0, "Offset mismatch");
static_assert(offsetof(LightData, directionAndRange) == 16, "Offset mismatch");
static_assert(offsetof(LightData, colorAndIntensity) == 32, "Offset mismatch");
static_assert(offsetof(LightData, spotAngles) == 48, "Offset mismatch");

static_assert(sizeof(LightBuffer) == 1040, "LightBuffer size mismatch");
static_assert(offsetof(LightBuffer, lightCount) == 0, "Offset mismatch");
static_assert(offsetof(LightBuffer, lights) == 16, "Offset mismatch");
```

## When to Use Storage Buffers vs Uniform Buffers

### Use Uniform Buffers When:
- Data size is small (< 256 bytes on MoltenVK)
- Data is accessed frequently with random access patterns
- Data changes every frame (MVP matrices, camera position)
- Maximum portability is required

### Use Storage Buffers When:
- Data size is large (> 256 bytes, especially on MoltenVK)
- Data is array-like (lights, particles, instances)
- Need dynamic array sizes
- Want more efficient packing (std430)
- Running on MoltenVK (Apple Silicon)

## Performance Considerations

On native Vulkan drivers (Windows/Linux):
- Uniform buffers and storage buffers have similar read performance
- Uniform buffers may have slightly better cache locality
- Storage buffers support write operations (not used in our case)

On MoltenVK (Apple Silicon):
- Storage buffers are **required** for buffers > 256 bytes
- Performance is equivalent to uniform buffers for read-only access
- std430 layout reduces padding, saving memory bandwidth

## Cross-Platform Testing

When developing for multiple platforms:

1. **Test on MoltenVK early**: Issues with large uniform buffers only appear on Apple Silicon
2. **Use storage buffers proactively**: If data might exceed 256 bytes, use storage buffers
3. **Add validation**: Use `sizeof()` and `offsetof()` to verify struct layouts
4. **Test memory synchronization**: Ensure `vkFlushMappedMemoryRanges()` is called
5. **Add pipeline barriers**: Synchronize CPU writes with GPU reads

## Debugging Tips

### Symptom: GPU reads zeros instead of actual data

**Possible causes:**
1. Uniform buffer exceeds MoltenVK limit (256 bytes)
2. Missing `vkFlushMappedMemoryRanges()` call
3. Missing pipeline barrier
4. CPU-GPU struct layout mismatch

**Solution:**
1. Switch to storage buffer (descriptor type + shader)
2. Add memory flush in `CopyData()`
3. Add pipeline barrier before rendering
4. Verify struct layouts with `sizeof()` and `offsetof()`

### Symptom: Flat colored output (no lighting variation)

**Possible causes:**
1. Light direction data is zeros (see above)
2. Struct field order mismatch between CPU and GPU
3. Incorrect padding causing offset shifts

**Solution:**
1. Add debug visualization in shader (output light direction as color)
2. Print hex dump of CPU buffer data
3. Verify `offsetof()` matches shader field order
4. Ensure padding matches on both sides

### Debug Visualization Example

```glsl
// In fragment shader - visualize light direction
vec3 debugDir = lightBuffer.lights[0].directionAndRange.xyz;
outColor = vec4(debugDir * 0.5 + 0.5, 1.0);  // Map [-1,1] to [0,1] for RGB
return;
```

If this shows (0.5, 0.5, 0.5) gray → GPU is reading zeros → storage buffer solution needed.

## References

- [Vulkan Specification: Descriptor Types](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#descriptorsets-types)
- [GLSL Specification: Interface Blocks](https://www.khronos.org/opengl/wiki/Interface_Block_(GLSL))
- [MoltenVK Runtime User Guide](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md)
- [Vulkan Memory Synchronization](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#synchronization)

## Summary

For large buffers (>256 bytes) on MoltenVK:
1. Use `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` instead of `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`
2. Use `std430` layout instead of `std140` in shaders
3. Use `readonly buffer` instead of `uniform` in shader declarations
4. Add `vkFlushMappedMemoryRanges()` after CPU writes
5. Add pipeline barrier before GPU reads
6. Verify CPU-GPU struct layouts match exactly

This ensures maximum compatibility across all Vulkan implementations while maintaining optimal performance.
