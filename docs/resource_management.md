# Resource Management and Deferred Deletion

## Overview

MetaGFX implements a deferred deletion queue system to safely manage GPU resource lifetimes when switching models at runtime. This system ensures that resources are not destroyed while they may still be referenced by in-flight GPU command buffers.

## The Problem

In modern graphics APIs like Vulkan, multiple frames can be "in flight" simultaneously - meaning the CPU is preparing frame N+2 while the GPU is still processing frames N and N+1. This is called **double-buffering** or **triple-buffering** depending on the number of frames in flight.

When switching models at runtime, immediately destroying the old model's resources creates a race condition:

```
Frame 1: CPU records commands referencing Model A → GPU starts processing
Frame 2: CPU records commands referencing Model A → GPU starts processing
Frame 3: CPU switches to Model B, destroys Model A ← CRASH!
         GPU is still processing frames 1-2 which reference Model A's buffers/textures
```

The GPU will attempt to access resources that have been freed, causing:
- `VK_ERROR_DEVICE_LOST` errors
- Segmentation faults
- Abort signals
- Application crashes

## The Solution: Deferred Deletion Queue

Instead of immediately destroying resources, MetaGFX queues them for deletion after a safe number of frames have passed.

### Implementation

**Data Structure** ([Application.h:100-104](../src/app/Application.h#L100-L104)):

```cpp
// Deferred deletion queue for old models
struct PendingDeletion {
    std::unique_ptr<Model> model;
    uint32 frameCount;  // Frames to wait before deletion
};
std::vector<PendingDeletion> m_DeletionQueue;
```

**Queuing Resources** ([Application.cpp:287-312](../src/app/Application.cpp#L287-L312)):

```cpp
void Application::LoadModel(const std::string& path) {
    // Add old model to deferred deletion queue instead of destroying immediately
    if (m_Model) {
        METAGFX_INFO << "Adding old model to deletion queue (will be destroyed in 2 frames)";
        // Queue old model for deletion after MAX_FRAMES_IN_FLIGHT frames
        m_DeletionQueue.push_back({std::move(m_Model), 2});
    }

    // Load new model immediately
    m_Model = std::make_unique<Model>();
    // ... loading code ...
}
```

**Processing the Queue** ([Application.cpp:502-511](../src/app/Application.cpp#L502-L511)):

```cpp
// Process deferred deletion queue - decrement frame counters and delete old models
for (auto it = m_DeletionQueue.begin(); it != m_DeletionQueue.end(); ) {
    it->frameCount--;
    if (it->frameCount == 0) {
        METAGFX_INFO << "Destroying deferred model (resources safe to delete)";
        it = m_DeletionQueue.erase(it);
    } else {
        ++it;
    }
}
```

### How It Works

1. **Frame 1**: User presses 'N' to switch models
   - Old model is moved to deletion queue with `frameCount = 2`
   - New model loads immediately
   - Deletion queue: `[{ModelA, frameCount=2}]`

2. **Frame 2**: Main loop processes deletion queue
   - Decrement ModelA's counter: `frameCount = 1`
   - ModelA still alive
   - Deletion queue: `[{ModelA, frameCount=1}]`

3. **Frame 3**: Main loop processes deletion queue
   - Decrement ModelA's counter: `frameCount = 0`
   - ModelA destroyed safely (GPU finished all commands referencing it)
   - Deletion queue: `[]`

### Why 2 Frames?

The constant `MAX_FRAMES_IN_FLIGHT = 2` in [VulkanSwapChain.h:55](../include/metagfx/rhi/vulkan/VulkanSwapChain.h#L55) defines how many frames can be processed simultaneously:

```cpp
static constexpr uint32 MAX_FRAMES_IN_FLIGHT = 2;
```

With double-buffering (2 frames in flight), we need to wait **2 frames** before destroying resources to guarantee that:
- Frame N-2 has completed GPU processing
- Frame N-1 has completed GPU processing
- Current frame N can safely destroy resources from frame N-2

## Synchronization Model

MetaGFX uses Vulkan's standard synchronization primitives:

**Per-Frame Resources**:
- `VkFence m_InFlightFences[MAX_FRAMES_IN_FLIGHT]` - CPU-GPU synchronization
- `VkSemaphore m_ImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT]` - GPU-GPU sync
- `VkSemaphore m_RenderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT]` - GPU-GPU sync

**Frame Flow** ([VulkanSwapChain.cpp:195-234](../src/rhi/vulkan/VulkanSwapChain.cpp#L195-L234)):

```cpp
void VulkanSwapChain::Present() {
    // Present current frame
    vkQueuePresentKHR(m_Context.presentQueue, &presentInfo);

    // Advance to next frame
    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    // Wait for the NEXT frame's fence and reset it
    vkWaitForFences(m_Context.device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(m_Context.device, 1, &m_InFlightFences[m_CurrentFrame]);

    // Acquire next image
    vkAcquireNextImageKHR(m_Context.device, m_SwapChain, UINT64_MAX,
                          m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE,
                          &m_CurrentImageIndex);
}
```

The fence wait at the start of frame N ensures that frame N-2 has completed GPU processing, making it safe to reuse frame N's resources (since they were last used 2 frames ago).

## Alternative Approaches (Not Used)

### 1. vkDeviceWaitIdle() - Global GPU Sync

```cpp
// ❌ NOT USED - Causes VK_ERROR_DEVICE_LOST
vkDeviceWaitIdle(device);  // Wait for ALL GPU work to complete
DestroyModel();
```

**Why not**:
- Calling `WaitIdle()` mid-frame breaks the swap chain synchronization state
- Causes stalls and performance issues
- Can trigger `VK_ERROR_DEVICE_LOST` errors
- Defeats the purpose of asynchronous rendering

### 2. Fence Reset After WaitIdle

```cpp
// ❌ NOT USED - Still causes errors
vkDeviceWaitIdle(device);
vkResetFences(device, fenceCount, fences);
DestroyModel();
```

**Why not**:
- Still disrupts the rendering pipeline
- Fences are in an inconsistent state
- Doesn't solve the fundamental synchronization issue

### 3. Swap Chain Recreation

```cpp
// ❌ NOT USED - Causes application freeze
vkDeviceWaitIdle(device);
swapChain->Resize(width, height);  // Force recreation
DestroyModel();
```

**Why not**:
- Resize calls `WaitIdle()` internally, causing deadlock
- Excessive overhead for simple model switching
- Breaks the frame pacing

## Best Practices

### ✅ DO:
- Use deferred deletion queues for all dynamically loaded resources
- Base the frame count on `MAX_FRAMES_IN_FLIGHT`
- Process the deletion queue at a consistent point in the frame (start or end)
- Log deletion queue operations for debugging

### ❌ DON'T:
- Call `vkDeviceWaitIdle()` during rendering
- Destroy resources immediately after queuing new work
- Assume `shared_ptr` reference counting is sufficient (GPU has no knowledge of it)
- Reset fences manually outside the normal frame flow

## Future Enhancements

Potential improvements to the resource management system:

1. **Generalized Deletion Queue**: Extend to handle all resource types (buffers, textures, pipelines)
2. **Ring Buffer Allocation**: Pre-allocate resource slots and reuse them
3. **Resource Tracking**: Track resource usage per frame for debugging
4. **Automatic Cleanup**: Destroy resources when reference count + frame age allows
5. **Memory Budget Management**: Track GPU memory usage and trigger cleanup when needed

## References

- Vulkan Specification: [Synchronization and Cache Control](https://www.khronos.org/registry/vulkan/specs/1.3/html/chap7.html)
- [Vulkan Guide: Synchronization](https://github.com/KhronosGroup/Vulkan-Guide/blob/master/chapters/synchronization.md)
- [GPU Gems 3: Asynchronous GPU Readbacks](https://developer.nvidia.com/gpugems/gpugems3/part-iv-image-effects/chapter-28-practical-post-process-depth-field)
