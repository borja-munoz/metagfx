// ============================================================================
// src/rhi/metal/MetalShader.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalShader.h"

// SPIRV-Cross for SPIR-V to MSL translation
#include <spirv_msl.hpp>

#include <vector>

namespace metagfx {
namespace rhi {

MetalShader::MetalShader(MetalContext& context, const ShaderDesc& desc)
    : m_Context(context)
    , m_Stage(desc.stage) {

    // Convert SPIR-V to MSL using SPIRV-Cross
    std::vector<uint32_t> spirvData(
        reinterpret_cast<const uint32_t*>(desc.code.data()),
        reinterpret_cast<const uint32_t*>(desc.code.data()) + desc.code.size() / sizeof(uint32_t)
    );

    spirv_cross::CompilerMSL mslCompiler(spirvData);

    // Set MSL options
    spirv_cross::CompilerMSL::Options mslOptions;
    mslOptions.platform = spirv_cross::CompilerMSL::Options::macOS;
    mslOptions.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);
    mslCompiler.set_msl_options(mslOptions);

    // Override resource bindings to preserve Vulkan binding indices in MSL
    // IMPORTANT: In Metal, vertex buffers and uniform buffers share the same buffer index namespace!
    // We need to offset uniform/storage buffers to avoid conflicting with vertex buffer at index 0.
    // Use an offset of 10 for descriptor buffers to leave room for vertex buffers (0-9).
    const uint32_t BUFFER_OFFSET = 10;  // Offset for uniform/storage buffers to avoid vertex buffer conflicts

    auto addResourceBindings = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                                   spirv_cross::SPIRType::BaseType expectedType,
                                   bool isBuffer) {
        for (const auto& resource : resources) {
            uint32_t set = mslCompiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            uint32_t binding = mslCompiler.get_decoration(resource.id, spv::DecorationBinding);

            spirv_cross::MSLResourceBinding mslBinding;
            mslBinding.stage = (desc.stage == ShaderStage::Vertex) ? spv::ExecutionModelVertex :
                               (desc.stage == ShaderStage::Fragment) ? spv::ExecutionModelFragment :
                               spv::ExecutionModelGLCompute;
            mslBinding.desc_set = set;
            mslBinding.binding = binding;
            // Offset buffer bindings to avoid conflict with vertex buffers at index 0
            mslBinding.msl_buffer = isBuffer ? (binding + BUFFER_OFFSET) : binding;
            mslBinding.msl_texture = binding;  // Textures have separate namespace, no offset needed
            mslBinding.msl_sampler = binding;  // Samplers have separate namespace, no offset needed

            mslCompiler.add_msl_resource_binding(mslBinding);
        }
    };

    // Get shader resources and add explicit binding overrides
    spirv_cross::ShaderResources resources = mslCompiler.get_shader_resources();
    addResourceBindings(resources.uniform_buffers, spirv_cross::SPIRType::Struct, true);   // Buffers need offset
    addResourceBindings(resources.storage_buffers, spirv_cross::SPIRType::Struct, true);   // Buffers need offset
    addResourceBindings(resources.sampled_images, spirv_cross::SPIRType::SampledImage, false);  // Textures, no offset
    addResourceBindings(resources.separate_images, spirv_cross::SPIRType::Image, false);        // Textures, no offset
    addResourceBindings(resources.separate_samplers, spirv_cross::SPIRType::Sampler, false);    // Samplers, no offset
    addResourceBindings(resources.storage_images, spirv_cross::SPIRType::Image, false);         // Textures, no offset

    // Handle push constants - map to buffer 30 (Metal max is 30, not 31)
    if (!resources.push_constant_buffers.empty()) {
        spirv_cross::MSLResourceBinding pushConstBinding;
        pushConstBinding.stage = (desc.stage == ShaderStage::Vertex) ? spv::ExecutionModelVertex :
                                 (desc.stage == ShaderStage::Fragment) ? spv::ExecutionModelFragment :
                                 spv::ExecutionModelGLCompute;
        pushConstBinding.desc_set = spirv_cross::kPushConstDescSet;
        pushConstBinding.binding = spirv_cross::kPushConstBinding;
        pushConstBinding.msl_buffer = 30;  // Use buffer 30 for push constants (Metal max is 30)
        mslCompiler.add_msl_resource_binding(pushConstBinding);
    }

    std::string mslSource;
    try {
        mslSource = mslCompiler.compile();
    } catch (const spirv_cross::CompilerError& e) {
        MTL_LOG_ERROR("SPIRV-Cross compilation failed: " << e.what());
        return;
    }

    METAGFX_DEBUG << "MSL shader compiled, source length: " << mslSource.size();

    // DEBUG: Print MSL binding info to understand remapping
    static int shaderCount = 0;
    shaderCount++;
    const char* stageName = (desc.stage == ShaderStage::Vertex) ? "VERTEX" :
                            (desc.stage == ShaderStage::Fragment) ? "FRAGMENT" : "OTHER";

    // Log ALL shaders for now to debug the issue
    METAGFX_INFO << "=== MSL Shader " << shaderCount << " (" << stageName << ") ===";

    // Find function signature (contains bindings) - look for the opening brace that ends the signature
    size_t mainPos = mslSource.find("main0(");
    if (mainPos != std::string::npos) {
        // Find the opening brace that starts the function body
        size_t bracePos = mslSource.find("{", mainPos);
        if (bracePos != std::string::npos) {
            std::string signature = mslSource.substr(mainPos, bracePos - mainPos);
            // Print signature in chunks of 200 chars
            for (size_t i = 0; i < signature.size(); i += 200) {
                METAGFX_INFO << "  " << signature.substr(i, 200);
            }
        }
    }
    METAGFX_INFO << "=== END MSL Shader " << shaderCount << " ===";

    // Create Metal library from MSL source
    NS::Error* error = nullptr;
    NS::String* source = NS::String::string(mslSource.c_str(), NS::UTF8StringEncoding);

    MTL::CompileOptions* compileOptions = MTL::CompileOptions::alloc()->init();
    m_Library = m_Context.device->newLibrary(source, compileOptions, &error);
    compileOptions->release();
    source->release();

    if (error || !m_Library) {
        if (error) {
            MTL_LOG_ERROR("Failed to create Metal library: " << error->localizedDescription()->utf8String());
            error->release();
        } else {
            MTL_LOG_ERROR("Failed to create Metal library");
        }
        return;
    }

    // Get the entry point function
    // SPIRV-Cross renames "main" to "main0" in MSL output
    std::string entryPoint = desc.entryPoint.empty() ? "main0" : desc.entryPoint;
    if (entryPoint == "main") {
        entryPoint = "main0";  // SPIRV-Cross convention
    }
    NS::String* functionName = NS::String::string(entryPoint.c_str(), NS::UTF8StringEncoding);
    m_Function = m_Library->newFunction(functionName);
    functionName->release();

    if (!m_Function) {
        MTL_LOG_ERROR("Failed to find function '" << entryPoint << "' in Metal library");
    }
}

MetalShader::~MetalShader() {
    if (m_Function) {
        m_Function->release();
        m_Function = nullptr;
    }
    if (m_Library) {
        m_Library->release();
        m_Library = nullptr;
    }
}

} // namespace rhi
} // namespace metagfx
