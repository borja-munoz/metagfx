#version 450

// Shadow map vertex shader - depth-only rendering from light's perspective

// Uniform buffer with light space matrix
layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix;  // Light's view-projection matrix
    mat4 model;             // Kept for struct compatibility; unused for instanced draws
    float shadowBias;       // Padding to match fragment shader UBO
    float padding[3];
} ubo;

// Per-vertex attributes (binding 0)
layout(location = 0) in vec3 inPosition;
// Note: We don't need normals or UVs for depth-only shadow pass

// Per-instance transform matrix (binding 1, 4 vec4 rows)
layout(location = 3) in vec4 instanceRow0;
layout(location = 4) in vec4 instanceRow1;
layout(location = 5) in vec4 instanceRow2;
layout(location = 6) in vec4 instanceRow3;

void main() {
    // Reconstruct per-instance model matrix from 4 row inputs
    mat4 instanceModel = mat4(instanceRow0, instanceRow1, instanceRow2, instanceRow3);

    // Transform vertex to light space (NDC)
    gl_Position = ubo.lightSpaceMatrix * instanceModel * vec4(inPosition, 1.0);
}
