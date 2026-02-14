#version 450

// Per-vertex attributes (binding 0)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Per-instance transform matrix (binding 1, 4 vec4 rows)
layout(location = 3) in vec4 instanceRow0;
layout(location = 4) in vec4 instanceRow1;
layout(location = 5) in vec4 instanceRow2;
layout(location = 6) in vec4 instanceRow3;

// Uniform buffer (VP matrices — model matrix now comes from instance data)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;  // Kept for non-instanced skybox/ground compatibility; unused for models
    mat4 view;
    mat4 projection;
} ubo;

// Outputs to fragment shader
layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    // Reconstruct per-instance model matrix from 4 row inputs
    mat4 instanceModel = mat4(instanceRow0, instanceRow1, instanceRow2, instanceRow3);

    // Transform vertex position
    vec4 worldPos = instanceModel * vec4(inPosition, 1.0);
    fragPosition = worldPos.xyz;

    // Transform normal (using normal matrix to handle non-uniform scaling)
    mat3 normalMatrix = transpose(inverse(mat3(instanceModel)));
    fragNormal = normalize(normalMatrix * inNormal);

    // Pass through texture coordinates
    fragTexCoord = inTexCoord;

    // Final position
    gl_Position = ubo.projection * ubo.view * worldPos;
}