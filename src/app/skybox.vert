#version 450

// Vertex positions for a cube (will be used as directions)
layout(location = 0) in vec3 inPosition;

// Output texture coordinates (3D direction vector)
layout(location = 0) out vec3 fragTexCoord;

// Uniform buffer with view and projection matrices
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

void main() {
    // Remove translation from view matrix (keep only rotation)
    // This makes the skybox follow the camera rotation but not position
    mat4 viewNoTranslation = mat4(mat3(ubo.view));

    // Transform position
    vec4 pos = ubo.projection * viewNoTranslation * vec4(inPosition, 1.0);

    // Set z = w to ensure depth is always 1.0 (farthest possible)
    // This ensures skybox is always behind everything else
    gl_Position = pos.xyww;

    // Use vertex position as texture coordinate direction
    fragTexCoord = inPosition;
}
