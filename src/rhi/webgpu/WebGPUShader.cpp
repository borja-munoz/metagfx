// ============================================================================
// src/rhi/webgpu/WebGPUShader.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUShader.h"
#include "metagfx/core/Logger.h"

// Tint for SPIR-V → WGSL transpilation (high-level API)
#include "src/tint/api/tint.h"

#include <vector>
#include <cstring>
#include <fstream>

namespace metagfx {
namespace rhi {

WebGPUShader::WebGPUShader(WebGPUContext& context, const ShaderDesc& desc)
    : m_Context(context)
    , m_Stage(desc.stage)
    , m_EntryPoint(desc.entryPoint.empty() ? "main" : desc.entryPoint) {

    // Convert SPIR-V bytecode to uint32_t vector for Tint
    std::vector<uint32_t> spirvData(
        reinterpret_cast<const uint32_t*>(desc.code.data()),
        reinterpret_cast<const uint32_t*>(desc.code.data()) + desc.code.size() / sizeof(uint32_t)
    );

    METAGFX_INFO << "Transpiling SPIR-V to WGSL for "
                    << (desc.stage == ShaderStage::Vertex ? "vertex" : "fragment") << " shader...";

    // Initialize Tint library (idempotent, safe to call multiple times)
    tint::Initialize();

    // Transpile SPIR-V to WGSL using Tint's high-level API
    tint::wgsl::writer::Options wgslOptions{};
    wgslOptions.allowed_features = tint::wgsl::AllowedFeatures::Everything();
    wgslOptions.allow_non_uniform_derivatives = true;  // Allow texture sampling in non-uniform control flow
    auto wgslResult = tint::SpirvToWgsl(spirvData, wgslOptions);

    if (wgslResult != tint::Success) {
        std::string errors = wgslResult.Failure().reason;
        METAGFX_ERROR << "Tint SPIR-V → WGSL transpilation failed: " << errors;
        throw std::runtime_error("Failed to transpile SPIR-V to WGSL with Tint");
    }

    m_WGSLSource = wgslResult.Get();

    METAGFX_INFO << "Tint successfully transpiled SPIR-V → WGSL"
                    << " (" << m_WGSLSource.length() << " bytes)";

    // Debug: Log first 500 characters of WGSL for inspection
    if (m_WGSLSource.length() > 0) {
        std::string preview = m_WGSLSource.substr(0, std::min<size_t>(500, m_WGSLSource.length()));
        METAGFX_INFO << "WGSL preview:\n" << preview << (m_WGSLSource.length() > 500 ? "\n..." : "");

        // Save full WGSL to file for debugging
        static int shaderCount = 0;
        std::string filename = "/tmp/shader_" + std::to_string(shaderCount++) + ".wgsl";
        std::ofstream wgslFile(filename);
        if (wgslFile) {
            wgslFile << m_WGSLSource;
            wgslFile.close();
            METAGFX_INFO << "Saved WGSL to " << filename;
        }
    }

    // Step 3: Create WebGPU shader module from WGSL source

    // Create WebGPU shader module from WGSL source (modern Dawn API)
    WGPUShaderSourceWGSL wgslSource{};
    wgslSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslSource.code.data = m_WGSLSource.c_str();
    wgslSource.code.length = m_WGSLSource.length();

    WGPUShaderModuleDescriptor moduleDesc{};
    moduleDesc.nextInChain = &wgslSource.chain;

    const char* labelStr = (desc.debugName && desc.debugName[0]) ? desc.debugName : "Shader";
    moduleDesc.label.data = labelStr;
    moduleDesc.label.length = strlen(labelStr);

    WGPUDevice device = m_Context.device.Get();
    WGPUShaderModule wgpuModule = wgpuDeviceCreateShaderModule(device, &moduleDesc);
    m_Module = wgpu::ShaderModule::Acquire(wgpuModule);

    if (!m_Module) {
        METAGFX_ERROR << "Failed to create WebGPU shader module from WGSL";
        throw std::runtime_error("Failed to create WebGPU shader module");
    }

    METAGFX_INFO << "WebGPU shader module created successfully";
}

WebGPUShader::~WebGPUShader() {
    m_Module = nullptr;
}

} // namespace rhi
} // namespace metagfx
