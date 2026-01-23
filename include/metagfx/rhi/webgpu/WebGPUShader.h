// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUShader.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Shader.h"
#include "WebGPUTypes.h"

namespace metagfx {
namespace rhi {

class WebGPUShader : public Shader {
public:
    WebGPUShader(WebGPUContext& context, const ShaderDesc& desc);
    ~WebGPUShader() override;

    ShaderStage GetStage() const override { return m_Stage; }

    // WebGPU-specific
    wgpu::ShaderModule GetModule() const { return m_Module; }
    const std::string& GetEntryPoint() const { return m_EntryPoint; }
    const std::string& GetWGSLSource() const { return m_WGSLSource; }

private:
    WebGPUContext& m_Context;
    wgpu::ShaderModule m_Module = nullptr;
    ShaderStage m_Stage;
    std::string m_EntryPoint;
    std::string m_WGSLSource;  // Store WGSL source for debugging
};

} // namespace rhi
} // namespace metagfx
