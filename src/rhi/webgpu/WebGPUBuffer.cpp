// ============================================================================
// src/rhi/webgpu/WebGPUBuffer.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUBuffer.h"
#include "metagfx/core/Logger.h"

#include <cstring>

namespace metagfx {
namespace rhi {

WebGPUBuffer::WebGPUBuffer(WebGPUContext& context, const BufferDesc& desc)
    : m_Context(context)
    , m_Size(desc.size)
    , m_Usage(desc.usage)
    , m_MemoryUsage(desc.memoryUsage) {

    // Convert buffer usage flags
    wgpu::BufferUsage usage = ToWebGPUBufferUsage(desc.usage);

    // WebGPU buffer usage rules:
    // - MapWrite/MapRead can ONLY be combined with CopySrc/CopyDst
    // - For CPUToGPU buffers (uniforms, vertex, index), use queue.WriteBuffer() instead of mapping
    // - Only add MapWrite for staging/readback buffers that don't have other usages

    // Add MapRead for GPU→CPU readback buffers
    if (m_MemoryUsage == MemoryUsage::GPUToCPU) {
        usage |= wgpu::BufferUsage::MapRead;
        usage |= wgpu::BufferUsage::CopyDst;  // Allow GPU to write to this buffer
    }

    // For CPUToGPU and GPUOnly buffers, use queue.WriteBuffer() for uploads
    // queue.WriteBuffer() requires CopyDst usage
    if (m_MemoryUsage == MemoryUsage::CPUToGPU || m_MemoryUsage == MemoryUsage::GPUOnly) {
        usage |= wgpu::BufferUsage::CopyDst;  // Required for queue.WriteBuffer()
    }

    // WebGPU requires uniform buffer sizes to be multiples of minUniformBufferOffsetAlignment (256 bytes)
    m_AllocatedSize = m_Size;
    if (static_cast<int>(desc.usage) & static_cast<int>(BufferUsage::Uniform)) {
        const uint64 alignment = m_Context.minUniformBufferOffsetAlignment;  // Usually 256
        m_AllocatedSize = ((m_Size + alignment - 1) / alignment) * alignment;
    }

    // Create buffer descriptor
    wgpu::BufferDescriptor bufferDesc{};
    bufferDesc.label = desc.debugName ? desc.debugName : "Buffer";
    bufferDesc.size = m_AllocatedSize;
    bufferDesc.usage = usage;
    bufferDesc.mappedAtCreation = false;

    // Create the buffer
    m_Buffer = m_Context.device.CreateBuffer(&bufferDesc);

    if (!m_Buffer) {
        METAGFX_ERROR << "Failed to create buffer";
        throw std::runtime_error("Failed to create WebGPU buffer");
    }
}

WebGPUBuffer::~WebGPUBuffer() {
    if (m_IsMapped) {
        Unmap();
    }

    m_Buffer = nullptr;
}

void* WebGPUBuffer::Map() {
    if (m_IsMapped) {
        return m_MappedData;
    }

    // WebGPU mapping is asynchronous, but we'll use a synchronous wrapper
    struct MapData {
        void* data = nullptr;
        bool done = false;
    };

    MapData mapData;

    // Determine map mode based on memory usage
    // Note: In WebGPU, CPUToGPU buffers (uniforms, vertex, index) cannot be mapped
    // They use queue.WriteBuffer() instead
    wgpu::MapMode mapMode = wgpu::MapMode::None;
    if (m_MemoryUsage == MemoryUsage::GPUToCPU) {
        mapMode = wgpu::MapMode::Read;
    } else {
        METAGFX_ERROR << "Cannot map buffer with memory usage: " << static_cast<int>(m_MemoryUsage)
                        << " (CPUToGPU buffers use queue.WriteBuffer instead)";
        return nullptr;
    }

    // Request buffer mapping using modern Dawn API
    auto callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void* userdata1, void* /* userdata2 */) {
        auto* data = static_cast<MapData*>(userdata1);
        data->done = true;

        if (status != WGPUMapAsyncStatus_Success) {
            const char* msgStr = message.data ? message.data : "Unknown error";
            METAGFX_ERROR << "Buffer mapping failed with status: " << static_cast<int>(status) << ", message: " << msgStr;
        }
    };

    WGPUBufferMapCallbackInfo callbackInfo{};
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = callback;
    callbackInfo.userdata1 = &mapData;

    WGPUBuffer buffer = m_Buffer.Get();
    wgpuBufferMapAsync(buffer, static_cast<WGPUMapMode>(mapMode), 0, m_Size, callbackInfo);

    // Wait for mapping to complete (blocking on native platforms)
#ifndef __EMSCRIPTEN__
    while (!mapData.done) {
        m_Context.device.Tick();
    }
#endif

    if (!mapData.done) {
        METAGFX_ERROR << "Buffer mapping timed out";
        return nullptr;
    }

    // Get mapped range
    if (mapMode == wgpu::MapMode::Write) {
        m_MappedData = m_Buffer.GetMappedRange(0, m_Size);
    } else {
        m_MappedData = const_cast<void*>(m_Buffer.GetConstMappedRange(0, m_Size));
    }

    if (!m_MappedData) {
        METAGFX_ERROR << "Failed to get mapped range";
        return nullptr;
    }

    m_IsMapped = true;
    return m_MappedData;
}

void WebGPUBuffer::Unmap() {
    if (!m_IsMapped) {
        return;
    }

    m_Buffer.Unmap();
    m_MappedData = nullptr;
    m_IsMapped = false;
}

void WebGPUBuffer::CopyData(const void* data, uint64 size, uint64 offset) {
    if (!data || size == 0) {
        return;
    }

    if (offset + size > m_Size) {
        METAGFX_ERROR << "Buffer copy out of bounds: offset=" << offset
                        << ", size=" << size << ", buffer size=" << m_Size;
        return;
    }

    // For small updates or CPU-writable buffers, use queue.WriteBuffer
    if (m_MemoryUsage == MemoryUsage::CPUToGPU || size <= 65536) {  // 64KB threshold
        m_Context.queue.WriteBuffer(m_Buffer, offset, data, size);
    } else {
        // For larger GPU-only buffers, we could use a staging buffer approach
        // For now, just use WriteBuffer (Dawn handles this efficiently)
        m_Context.queue.WriteBuffer(m_Buffer, offset, data, size);
    }
}

} // namespace rhi
} // namespace metagfx
