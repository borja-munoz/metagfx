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

// Albedo texture sampler
layout(binding = 2) uniform sampler2D albedoSampler;

// Push constants for camera position and material flags
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;  // Using vec4 for alignment (xyz = position, w unused)
    uint materialFlags;   // Bit 0: has albedo texture
    uint padding[3];      // Align to 16 bytes
} pushConstants;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional light
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    // Use texture if present, otherwise scalar albedo
    vec3 objectColor;
    if ((pushConstants.materialFlags & 1u) != 0u) {
        objectColor = texture(albedoSampler, fragTexCoord).rgb;
    } else {
        objectColor = material.albedo;
    }

    // Ambient lighting
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular lighting (Blinn-Phong)
    vec3 viewDir = normalize(pushConstants.cameraPosition.xyz - fragPosition);
    vec3 halfDir = normalize(lightDir + viewDir);

    // Derive shininess from roughness (high roughness = low shininess)
    float shininess = mix(256.0, 16.0, material.roughness);
    float spec = pow(max(dot(norm, halfDir), 0.0), shininess);

    // Specular strength (could be material property later)
    float specularStrength = 0.5;
    vec3 specular = specularStrength * spec * lightColor;

    // Combine lighting: (ambient + diffuse) * albedo + specular
    vec3 result = (ambient + diffuse) * objectColor + specular;

    outColor = vec4(result, 1.0);
}
