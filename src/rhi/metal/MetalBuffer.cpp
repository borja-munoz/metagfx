// ============================================================================
// src/rhi/metal/MetalBuffer.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalBuffer.h"

#include <cstring>

namespace metagfx {
namespace rhi {

MetalBuffer::MetalBuffer(MetalContext& context, const BufferDesc& desc)
    : m_Context(context)
    , m_Size(desc.size)
    , m_Usage(desc.usage)
    , m_MemoryUsage(desc.memoryUsage) {

    MTL::ResourceOptions options = ToMetalResourceOptions(desc.memoryUsage);

    m_Buffer = m_Context.device->newBuffer(desc.size, options);

    if (!m_Buffer) {
        MTL_LOG_ERROR("Failed to create buffer");
    }

    if (desc.debugName) {
        NS::String* label = NS::String::string(desc.debugName, NS::UTF8StringEncoding);
        m_Buffer->setLabel(label);
        label->release();
    }
}

MetalBuffer::~MetalBuffer() {
    if (m_Buffer) {
        m_Buffer->release();
        m_Buffer = nullptr;
    }
}

void* MetalBuffer::Map() {
    // Only shared/managed buffers can be mapped
    if (m_MemoryUsage == MemoryUsage::GPUOnly) {
        MTL_LOG_ERROR("Cannot map GPU-only buffer");
        return nullptr;
    }

    return m_Buffer->contents();
}

void MetalBuffer::Unmap() {
    // Metal shared buffers don't need explicit unmap
    // For managed buffers on macOS, we would call didModifyRange:
#if TARGET_OS_OSX
    if (m_MemoryUsage == MemoryUsage::CPUToGPU) {
        m_Buffer->didModifyRange(NS::Range::Make(0, m_Size));
    }
#endif
}

void MetalBuffer::CopyData(const void* data, uint64 size, uint64 offset) {
    if (m_MemoryUsage == MemoryUsage::GPUOnly) {
        MTL_LOG_ERROR("Cannot copy data to GPU-only buffer directly");
        return;
    }

    void* contents = m_Buffer->contents();
    if (contents) {
        memcpy(static_cast<uint8*>(contents) + offset, data, size);

#if TARGET_OS_OSX
        if (m_MemoryUsage == MemoryUsage::CPUToGPU) {
            m_Buffer->didModifyRange(NS::Range::Make(offset, size));
        }
#endif
    }
}

} // namespace rhi
} // namespace metagfx
