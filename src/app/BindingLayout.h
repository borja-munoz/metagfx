// ============================================================================
// src/app/BindingLayout.h
//
// Binding layout constants for descriptor sets.
// These are compile-time constants, so we define BOTH layouts.
// The application must select which layout to use based on the active backend.
// ============================================================================
#pragma once

#include <cstdint>

// WebGPU sparse layout - leaves gaps for Tint-inserted samplers
//
// Tint (SPIR-V → WGSL compiler) automatically splits combined image samplers:
// - GLSL: layout(binding = N) uniform sampler2D tex;
// - WGSL: @binding(N) var tex : texture_2d<f32>; @binding(N+1) var samp : sampler;
//
// This means we must skip binding N+1 for every texture at binding N.

namespace WebGPUBindings {

namespace ModelBindings {
    constexpr uint32_t MVP = 0;
    constexpr uint32_t MATERIAL = 1;
    constexpr uint32_t ALBEDO = 2;
    // 3: albedo sampler (auto-inserted by Tint)
    constexpr uint32_t LIGHTS = 4;
    constexpr uint32_t NORMAL = 5;
    // 6: normal sampler (auto-inserted by Tint)
    constexpr uint32_t METALLIC = 7;
    // 8: metallic sampler (auto-inserted by Tint)
    constexpr uint32_t ROUGHNESS = 9;
    // 10: roughness sampler (auto-inserted by Tint)
    constexpr uint32_t AO = 11;
    // 12: AO sampler (auto-inserted by Tint)
    constexpr uint32_t IRRADIANCE = 13;
    // 14: irradiance sampler (auto-inserted by Tint)
    constexpr uint32_t PREFILTERED = 15;
    // 16: prefiltered sampler (auto-inserted by Tint)
    constexpr uint32_t BRDF_LUT = 17;
    // 18: BRDF LUT sampler (auto-inserted by Tint)
    constexpr uint32_t EMISSIVE = 19;
    // 20: emissive sampler (auto-inserted by Tint)
    constexpr uint32_t SHADOWMAP = 21;
    // 22: shadow sampler (auto-inserted by Tint)
    constexpr uint32_t SHADOW_UBO = 23;
    constexpr uint32_t PUSH_CONSTANTS = 24;
}

namespace SkyboxBindings {
    constexpr uint32_t MVP = 0;
    constexpr uint32_t ENVIRONMENT = 1;
    // 2: environment sampler (auto-inserted by Tint)
    constexpr uint32_t PUSH_CONSTANTS = 3;
}

namespace ShadowBindings {
    constexpr uint32_t SHADOW_UBO = 0;
}

}  // namespace WebGPUBindings

// Vulkan/Metal dense layout - combined image samplers
//
// Vulkan and Metal support combined image samplers where texture + sampler
// use a single binding number. No gaps needed.

namespace VulkanMetalBindings {

namespace ModelBindings {
    constexpr uint32_t MVP = 0;
    constexpr uint32_t MATERIAL = 1;
    constexpr uint32_t ALBEDO = 2;
    constexpr uint32_t LIGHTS = 3;
    constexpr uint32_t NORMAL = 4;
    constexpr uint32_t METALLIC = 5;
    constexpr uint32_t ROUGHNESS = 6;
    constexpr uint32_t AO = 7;
    constexpr uint32_t IRRADIANCE = 8;
    constexpr uint32_t PREFILTERED = 9;
    constexpr uint32_t BRDF_LUT = 10;
    constexpr uint32_t EMISSIVE = 11;
    constexpr uint32_t SHADOWMAP = 12;
    constexpr uint32_t SHADOW_UBO = 13;
    constexpr uint32_t PUSH_CONSTANTS = 14;
}

namespace SkyboxBindings {
    constexpr uint32_t MVP = 0;
    constexpr uint32_t ENVIRONMENT = 1;
    constexpr uint32_t PUSH_CONSTANTS = 2;  // Dense bindings for Vulkan/Metal
}

namespace ShadowBindings {
    constexpr uint32_t SHADOW_UBO = 0;
}

}  // namespace VulkanMetalBindings

// Helper macros to select binding numbers based on GraphicsAPI at the call site
// Usage: BINDING(api, ModelBindings::ALBEDO)
// This expands to either WebGPUBindings::ModelBindings::ALBEDO or VulkanMetalBindings::ModelBindings::ALBEDO
#define BINDING(api, binding) \
    ((api) == metagfx::rhi::GraphicsAPI::WebGPU ? WebGPUBindings::binding : VulkanMetalBindings::binding)

// For now, default to Vulkan/Metal bindings for backwards compatibility
// Code that needs WebGPU bindings should use BINDING() macro or explicitly use WebGPUBindings::
using namespace VulkanMetalBindings;
