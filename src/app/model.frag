#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Output color
layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional light
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    
    // Base color (will be replaced with material color in Phase 2.2)
    vec3 objectColor = vec3(0.7, 0.7, 0.7);
    
    // Ambient lighting
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Combine lighting
    vec3 result = (ambient + diffuse) * objectColor;
    
    outColor = vec4(result, 1.0);
}