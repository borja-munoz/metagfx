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

// Light data structure (64 bytes, matches CPU struct)
struct LightData {
    vec4 positionAndType;    // xyz=position, w=type (0=dir, 1=point, 2=spot)
    vec4 directionAndRange;  // xyz=direction, w=range
    vec4 colorAndIntensity;  // rgb=color, w=intensity
    vec4 spotAngles;         // x=innerAngle, y=outerAngle, z=attConst, w=attLinear
};

// Light buffer uniform (binding 3)
layout(binding = 3) uniform LightBuffer {
    uint lightCount;
    uint padding[3];
    LightData lights[16];
} lightBuffer;

// Push constants for camera position and material flags
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;
    uint materialFlags;
    uint padding[3];
} pushConstants;

// Output color
layout(location = 0) out vec4 outColor;

// Constants
const int LIGHT_TYPE_DIRECTIONAL = 0;
const int LIGHT_TYPE_POINT = 1;
const int LIGHT_TYPE_SPOT = 2;

// Calculate lighting contribution from a single light
vec3 calculateLightContribution(LightData light, vec3 fragPos, vec3 normal, vec3 viewDir, vec3 albedo, float roughness) {
    int lightType = int(light.positionAndType.w);
    vec3 lightColor = light.colorAndIntensity.rgb * light.colorAndIntensity.w;

    // Compute light direction and attenuation
    vec3 lightDir;
    float attenuation = 1.0;

    if (lightType == LIGHT_TYPE_DIRECTIONAL) {
        // Directional light (parallel rays)
        lightDir = normalize(-light.directionAndRange.xyz);
        // No attenuation for directional lights

    } else if (lightType == LIGHT_TYPE_POINT) {
        // Point light (omnidirectional)
        vec3 lightPos = light.positionAndType.xyz;
        vec3 lightToFrag = fragPos - lightPos;
        float distance = length(lightToFrag);
        lightDir = normalize(-lightToFrag);

        // Attenuation: 1 / (constant + linear * d + quadratic * d^2)
        float range = light.directionAndRange.w;
        float attConst = light.spotAngles.z;
        float attLinear = light.spotAngles.w;
        float attQuadratic = 1.0 / (range * range);
        attenuation = 1.0 / (attConst + attLinear * distance + attQuadratic * distance * distance);

    } else if (lightType == LIGHT_TYPE_SPOT) {
        // Spot light (cone)
        vec3 lightPos = light.positionAndType.xyz;
        vec3 lightToFrag = fragPos - lightPos;
        float distance = length(lightToFrag);
        lightDir = normalize(-lightToFrag);

        // Distance attenuation (same as point light)
        float range = light.directionAndRange.w;
        float attConst = light.spotAngles.z;
        float attLinear = light.spotAngles.w;
        float attQuadratic = 1.0 / (range * range);
        float distAttenuation = 1.0 / (attConst + attLinear * distance + attQuadratic * distance * distance);

        // Cone attenuation (smooth falloff between inner and outer angles)
        vec3 spotDir = normalize(light.directionAndRange.xyz);
        float theta = dot(lightDir, -spotDir);
        float innerCutoff = cos(light.spotAngles.x);
        float outerCutoff = cos(light.spotAngles.y);
        float epsilon = innerCutoff - outerCutoff;
        float coneAttenuation = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);

        attenuation = distAttenuation * coneAttenuation;
    }

    // Blinn-Phong lighting components

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (Blinn-Phong)
    vec3 halfDir = normalize(lightDir + viewDir);
    float shininess = mix(256.0, 16.0, roughness);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
    float specularStrength = 0.5;
    vec3 specular = specularStrength * spec * lightColor;

    // Combine with attenuation
    return attenuation * (diffuse * albedo + specular);
}

void main() {
    // Get albedo color (texture or material property)
    vec3 albedo;
    if ((pushConstants.materialFlags & 1u) != 0u) {
        albedo = texture(albedoSampler, fragTexCoord).rgb;
    } else {
        albedo = material.albedo;
    }

    // Prepare common vectors
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(pushConstants.cameraPosition.xyz - fragPosition);

    // Ambient lighting (global illumination approximation)
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * albedo;

    // Accumulate lighting from all lights
    vec3 lighting = vec3(0.0);
    for (uint i = 0u; i < lightBuffer.lightCount && i < 16u; i++) {
        lighting += calculateLightContribution(
            lightBuffer.lights[i],
            fragPosition,
            normal,
            viewDir,
            albedo,
            material.roughness
        );
    }

    // Final color: ambient + accumulated lighting
    vec3 result = ambient + lighting;

    outColor = vec4(result, 1.0);
}
