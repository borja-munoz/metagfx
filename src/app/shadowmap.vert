#version 450

// Shadow map vertex shader - depth-only rendering from light's perspective

// Uniform buffer with light space matrix and model matrix
layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix;  // Light's view-projection matrix
    mat4 model;             // Model matrix (identity for now)
    float shadowBias;       // Padding to match fragment shader UBO
    float padding[3];
} ubo;

// Input vertex attributes
layout(location = 0) in vec3 inPosition;
// Note: We don't need normals or UVs for depth-only shadow pass

void main() {
    // Transform vertex to light space (NDC)
    gl_Position = ubo.lightSpaceMatrix * ubo.model * vec4(inPosition, 1.0);
}
