// ============================================================================
// include/metagfx/rhi/SwapChain.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

namespace metagfx {
namespace rhi {

class SwapChain {
public:
    virtual ~SwapChain() = default;
    
    virtual void Present() = 0;
    virtual void Resize(uint32 width, uint32 height) = 0;
    
    virtual Ref<Texture> GetCurrentBackBuffer() = 0;
    virtual uint32 GetWidth() const = 0;
    virtual uint32 GetHeight() const = 0;
    virtual Format GetFormat() const = 0;
    
protected:
    SwapChain() = default;
};

} // namespace rhi
} // namespace metagfx
