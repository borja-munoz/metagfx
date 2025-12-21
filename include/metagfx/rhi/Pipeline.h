// ============================================================================
// include/metagfx/rhi/Pipeline.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"

namespace metagfx {
namespace rhi {

class Pipeline {
public:
    virtual ~Pipeline() = default;
    
protected:
    Pipeline() = default;
};

} // namespace rhi
} // namespace metagfx
