#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Material uniform
layout(binding = 1) uniform MaterialUBO {
    vec3 albedo;
    float roughness;
    float metallic;
} material;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Simple test: just output material albedo
    outColor = vec4(material.albedo, 1.0);
}
