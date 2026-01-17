// ============================================================================
// include/metagfx/rhi/metal/MetalShader.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Shader.h"
#include "MetalTypes.h"

namespace metagfx {
namespace rhi {

class MetalShader : public Shader {
public:
    MetalShader(MetalContext& context, const ShaderDesc& desc);
    ~MetalShader() override;

    ShaderStage GetStage() const override { return m_Stage; }

    // Metal-specific
    MTL::Library* GetLibrary() const { return m_Library; }
    MTL::Function* GetFunction() const { return m_Function; }

private:
    MetalContext& m_Context;
    MTL::Library* m_Library = nullptr;
    MTL::Function* m_Function = nullptr;
    ShaderStage m_Stage;
};

} // namespace rhi
} // namespace metagfx
