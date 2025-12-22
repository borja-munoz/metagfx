#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Uniform buffer (MVP matrices)
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

// Outputs to fragment shader
layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    // Transform vertex position
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragPosition = worldPos.xyz;
    
    // Transform normal (using normal matrix to handle non-uniform scaling)
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    fragNormal = normalize(normalMatrix * inNormal);
    
    // Pass through texture coordinates
    fragTexCoord = inTexCoord;
    
    // Final position
    gl_Position = ubo.projection * ubo.view * worldPos;
}