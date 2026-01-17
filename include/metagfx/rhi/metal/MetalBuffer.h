// ============================================================================
// include/metagfx/rhi/metal/MetalBuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Buffer.h"
#include "MetalTypes.h"

namespace metagfx {
namespace rhi {

class MetalBuffer : public Buffer {
public:
    MetalBuffer(MetalContext& context, const BufferDesc& desc);
    ~MetalBuffer() override;

    void* Map() override;
    void Unmap() override;
    void CopyData(const void* data, uint64 size, uint64 offset = 0) override;

    uint64 GetSize() const override { return m_Size; }
    BufferUsage GetUsage() const override { return m_Usage; }

    // Metal-specific
    MTL::Buffer* GetHandle() const { return m_Buffer; }

private:
    MetalContext& m_Context;
    MTL::Buffer* m_Buffer = nullptr;

    uint64 m_Size = 0;
    BufferUsage m_Usage;
    MemoryUsage m_MemoryUsage;
};

} // namespace rhi
} // namespace metagfx
