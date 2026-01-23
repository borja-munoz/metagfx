// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUBuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Buffer.h"
#include "WebGPUTypes.h"

namespace metagfx {
namespace rhi {

class WebGPUBuffer : public Buffer {
public:
    WebGPUBuffer(WebGPUContext& context, const BufferDesc& desc);
    ~WebGPUBuffer() override;

    void* Map() override;
    void Unmap() override;
    void CopyData(const void* data, uint64 size, uint64 offset = 0) override;

    uint64 GetSize() const override { return m_Size; }
    BufferUsage GetUsage() const override { return m_Usage; }

    // WebGPU-specific
    wgpu::Buffer GetHandle() const { return m_Buffer; }

private:
    WebGPUContext& m_Context;
    wgpu::Buffer m_Buffer = nullptr;

    uint64 m_Size = 0;
    BufferUsage m_Usage;
    MemoryUsage m_MemoryUsage;

    void* m_MappedData = nullptr;
    bool m_IsMapped = false;
};

} // namespace rhi
} // namespace metagfx
