// ============================================================================
// include/metagfx/rhi/Texture.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

namespace metagfx {
namespace rhi {

class Texture {
public:
    virtual ~Texture() = default;
    
    virtual uint32 GetWidth() const = 0;
    virtual uint32 GetHeight() const = 0;
    virtual Format GetFormat() const = 0;
    
protected:
    Texture() = default;
};

} // namespace rhi
} // namespace metagfx
