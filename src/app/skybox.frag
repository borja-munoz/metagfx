#version 450

// Input texture coordinates (3D direction vector from vertex shader)
layout(location = 0) in vec3 fragTexCoord;

// Output color
layout(location = 0) out vec4 outColor;

// Environment cubemap sampler
layout(binding = 1) uniform samplerCube environmentMap;

// Uniform buffer for skybox parameters
// (converted from push constants for WebGPU compatibility)
// Use binding = 2 for Vulkan/Metal (dense bindings 0, 1, 2)
// WebGPU's Tint will auto-remap this to binding 3 when converting SPIR-V to WGSL
layout(binding = 2) uniform PushConstants {
    float exposure;     // HDR exposure
    float lod;          // Mipmap level (0 = sharp, higher = blurred)
} pushConstants;

void main() {
    // Sample environment cubemap with specified LOD
    vec3 color = textureLod(environmentMap, fragTexCoord, pushConstants.lod).rgb;

    // Apply exposure
    color = color * pushConstants.exposure;

    // Tone mapping (simple clamp)
    color = clamp(color, 0.0, 1.0);

    // Gamma correction (linear to sRGB)
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
