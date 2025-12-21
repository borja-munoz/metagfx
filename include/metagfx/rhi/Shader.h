// ============================================================================
// include/metagfx/rhi/Shader.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Types.h"

namespace metagfx {
namespace rhi {

class Shader {
public:
    virtual ~Shader() = default;
    
    virtual ShaderStage GetStage() const = 0;
    
protected:
    Shader() = default;
};

} // namespace rhi
} // namespace metagfx
