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

    // Add MapWrite usage for CPU-writable buffers
    if (m_MemoryUsage == MemoryUsage::CPUToGPU || m_MemoryUsage == MemoryUsage::CPUOnly) {
        usage |= wgpu::BufferUsage::MapWrite;
    }

    // Add MapRead usage for CPU-readable buffers
    if (m_MemoryUsage == MemoryUsage::GPUToCPU || m_MemoryUsage == MemoryUsage::CPUOnly) {
        usage |= wgpu::BufferUsage::MapRead;
    }

    // For GPU-only buffers, we need CopyDst to upload data
    if (m_MemoryUsage == MemoryUsage::GPUOnly) {
        usage |= wgpu::BufferUsage::CopyDst;
    }

    // Create buffer descriptor
    wgpu::BufferDescriptor bufferDesc{};
    bufferDesc.label = desc.debugName ? desc.debugName : "Buffer";
    bufferDesc.size = m_Size;
    bufferDesc.usage = usage;
    bufferDesc.mappedAtCreation = false;

    // Create the buffer
    m_Buffer = m_Context.device.CreateBuffer(&bufferDesc);

    if (!m_Buffer) {
        WEBGPU_LOG_ERROR("Failed to create buffer");
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
    wgpu::MapMode mapMode = wgpu::MapMode::None;
    if (m_MemoryUsage == MemoryUsage::CPUToGPU || m_MemoryUsage == MemoryUsage::CPUOnly) {
        mapMode = wgpu::MapMode::Write;
    } else if (m_MemoryUsage == MemoryUsage::GPUToCPU) {
        mapMode = wgpu::MapMode::Read;
    } else {
        WEBGPU_LOG_ERROR("Cannot map GPU-only buffer");
        return nullptr;
    }

    // Request buffer mapping
    auto callback = [](WGPUBufferMapAsyncStatus status, void* userdata) {
        auto* data = static_cast<MapData*>(userdata);
        data->done = true;

        if (status != WGPUBufferMapAsyncStatus_Success) {
            METAGFX_ERROR << "Buffer mapping failed with status: " << static_cast<int>(status);
        }
    };

    m_Buffer.MapAsync(mapMode, 0, m_Size, callback, &mapData);

    // Wait for mapping to complete (blocking on native platforms)
#ifndef __EMSCRIPTEN__
    while (!mapData.done) {
        m_Context.device.Tick();
    }
#endif

    if (!mapData.done) {
        WEBGPU_LOG_ERROR("Buffer mapping timed out");
        return nullptr;
    }

    // Get mapped range
    if (mapMode == wgpu::MapMode::Write) {
        m_MappedData = m_Buffer.GetMappedRange(0, m_Size);
    } else {
        m_MappedData = const_cast<void*>(m_Buffer.GetConstMappedRange(0, m_Size));
    }

    if (!m_MappedData) {
        WEBGPU_LOG_ERROR("Failed to get mapped range");
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
        WEBGPU_LOG_ERROR("Buffer copy out of bounds: offset=" << offset
                        << ", size=" << size << ", buffer size=" << m_Size);
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
