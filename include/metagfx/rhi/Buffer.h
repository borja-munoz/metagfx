// ============================================================================
// include/metagfx/rhi/Buffer.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

namespace metagfx {
namespace rhi {

class Buffer {
public:
    virtual ~Buffer() = default;
    
    virtual void* Map() = 0;
    virtual void Unmap() = 0;
    virtual void CopyData(const void* data, uint64 size, uint64 offset = 0) = 0;
    
    virtual uint64 GetSize() const = 0;
    virtual BufferUsage GetUsage() const = 0;
    
protected:
    Buffer() = default;
};

} // namespace rhi
} // namespace metagfx