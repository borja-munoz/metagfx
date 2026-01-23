// ============================================================================
// src/rhi/webgpu/WebGPUShader.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUShader.h"
#include "metagfx/core/Logger.h"

// Tint for SPIR-V to WGSL transpilation
#include <tint/tint.h>

#include <vector>

namespace metagfx {
namespace rhi {

WebGPUShader::WebGPUShader(WebGPUContext& context, const ShaderDesc& desc)
    : m_Context(context)
    , m_Stage(desc.stage)
    , m_EntryPoint(desc.entryPoint.empty() ? "main" : desc.entryPoint) {

    // Convert SPIR-V to WGSL using Tint (Google's official SPIR-V → WGSL compiler)
    std::vector<uint32_t> spirvData(
        reinterpret_cast<const uint32_t*>(desc.code.data()),
        reinterpret_cast<const uint32_t*>(desc.code.data()) + desc.code.size() / sizeof(uint32_t)
    );

    // Parse SPIR-V using Tint
    tint::Source::File sourceFile("shader.spv", "");
    tint::Program program = tint::spirv::reader::Read(spirvData, {});

    if (!program.IsValid()) {
        std::string errors;
        for (const auto& diag : program.Diagnostics()) {
            errors += diag.message.str() + "\n";
        }
        WEBGPU_LOG_ERROR("Tint SPIR-V parsing failed: " << errors);
        throw std::runtime_error("Failed to parse SPIR-V shader with Tint");
    }

    WEBGPU_LOG_INFO("Tint successfully parsed SPIR-V shader");

    // Generate WGSL from parsed SPIR-V program
    auto result = tint::wgsl::writer::Generate(program, {});

    if (!result) {
        std::string errors;
        for (const auto& diag : result.Failure().reason.str()) {
            errors += diag;
        }
        WEBGPU_LOG_ERROR("Tint WGSL generation failed: " << errors);
        throw std::runtime_error("Failed to generate WGSL from SPIR-V with Tint");
    }

    m_WGSLSource = result->wgsl;

    WEBGPU_LOG_INFO("Tint successfully transpiled SPIR-V → WGSL for "
                    << (desc.stage == ShaderStage::Vertex ? "vertex" : "fragment")
                    << " stage (" << m_WGSLSource.length() << " bytes)");

    // Debug: Log first 500 characters of WGSL for inspection
    if (m_WGSLSource.length() > 0) {
        std::string preview = m_WGSLSource.substr(0, std::min<size_t>(500, m_WGSLSource.length()));
        WEBGPU_LOG_INFO("WGSL preview: " << preview << (m_WGSLSource.length() > 500 ? "..." : ""));
    }

    // Create WebGPU shader module from WGSL source
    wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
    wgslDesc.code = m_WGSLSource.c_str();

    wgpu::ShaderModuleDescriptor moduleDesc{};
    moduleDesc.nextInChain = &wgslDesc;
    moduleDesc.label = desc.debugName.empty() ? "Shader" : desc.debugName.c_str();

    m_Module = m_Context.device.CreateShaderModule(&moduleDesc);

    if (!m_Module) {
        WEBGPU_LOG_ERROR("Failed to create WebGPU shader module from WGSL");
        throw std::runtime_error("Failed to create WebGPU shader module");
    }

    WEBGPU_LOG_INFO("WebGPU shader module created successfully from Tint-generated WGSL");
}

WebGPUShader::~WebGPUShader() {
    m_Module = nullptr;
}

} // namespace rhi
} // namespace metagfx
