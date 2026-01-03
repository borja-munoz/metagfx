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

// PBR texture samplers
layout(binding = 2) uniform sampler2D albedoSampler;
layout(binding = 4) uniform sampler2D normalSampler;
layout(binding = 5) uniform sampler2D metallicSampler;
layout(binding = 6) uniform sampler2D roughnessSampler;
layout(binding = 7) uniform sampler2D aoSampler;

// Light data structure (64 bytes, matches CPU struct)
struct LightData {
    vec4 positionAndType;    // xyz=position, w=type (0=dir, 1=point, 2=spot)
    vec4 directionAndRange;  // xyz=direction, w=range
    vec4 colorAndIntensity;  // rgb=color, w=intensity
    vec4 spotAngles;         // x=innerAngle, y=outerAngle, z=attConst, w=attLinear
};

// Light buffer storage (set 0, binding = 3) - CHANGED TO STORAGE BUFFER for MoltenVK compatibility
// CRITICAL: Must match CPU struct exactly - uint lightCount + uint padding[3]
layout(set = 0, binding = 3, std430) readonly buffer LightBuffer {
    uint lightCount;
    uint padding[3];
    LightData lights[16];
} lightBuffer;

// Push constants for camera position, material flags, and exposure
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;
    uint materialFlags;
    float exposure;
    uint padding[2];
} pushConstants;

// Output color
layout(location = 0) out vec4 outColor;

// Constants
const int LIGHT_TYPE_DIRECTIONAL = 0;
const int LIGHT_TYPE_POINT = 1;
const int LIGHT_TYPE_SPOT = 2;
const float PI = 3.14159265359;

// ============================================================================
// PBR Utility Functions
// ============================================================================

// Normal Distribution Function (GGX/Trowbridge-Reitz)
// Describes the distribution of microfacet normals
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001);
}

// Geometry Function (Smith's Schlick-GGX)
// Describes self-shadowing of microfacets
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;  // Direct lighting

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel Function (Fresnel-Schlick approximation)
// Describes how much light is reflected vs. refracted
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============================================================================
// Tone Mapping
// ============================================================================

// ACES Filmic Tone Mapping
// High-quality tone mapping curve used in film production
// Handles HDR values gracefully and provides better color reproduction
vec3 toneMapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// ============================================================================
// Normal Mapping
// ============================================================================

// Convert tangent-space normal from map to world-space
// Uses derivative-based TBN matrix calculation (no need for tangent vertex attributes)
vec3 getNormalFromMap(vec2 texCoord, vec3 worldPos, vec3 worldNormal) {
    // Sample tangent-space normal from texture
    vec3 tangentNormal = texture(normalSampler, texCoord).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;  // Transform from [0,1] to [-1,1]

    // glTF uses OpenGL convention (Y-up), so no flip needed
    // tangentNormal.y = -tangentNormal.y;  // Uncomment for DirectX-style normal maps

    // Calculate TBN matrix on-the-fly using screen-space derivatives
    vec3 Q1 = dFdx(worldPos);
    vec3 Q2 = dFdy(worldPos);
    vec2 st1 = dFdx(texCoord);
    vec2 st2 = dFdy(texCoord);

    vec3 N = normalize(worldNormal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

// ============================================================================
// PBR Lighting Calculation
// ============================================================================

// Calculate PBR lighting contribution from a single light using Cook-Torrance BRDF
vec3 calculatePBRLighting(LightData light, vec3 fragPos, vec3 normal, vec3 viewDir,
                          vec3 albedo, float roughness, float metallic) {
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

    // Calculate radiance
    vec3 radiance = lightColor * attenuation;

    // Cook-Torrance BRDF
    vec3 N = normal;
    vec3 V = viewDir;
    vec3 L = lightDir;
    vec3 H = normalize(V + L);

    // Calculate F0 (surface reflection at zero incidence)
    // Dielectrics (non-metals): ~0.04 (4% reflection)
    // Metals: use albedo color (absorb diffuse, reflect albedo as specular)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Cook-Torrance BRDF components
    float NDF = DistributionGGX(N, H, roughness);   // Normal Distribution
    float G = GeometrySmith(N, V, L, roughness);    // Geometry shadowing/masking
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0); // Fresnel reflection

    // Specular component (Cook-Torrance)
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    // Energy conservation: kS + kD = 1.0
    vec3 kS = F;  // Specular reflection ratio (Fresnel)
    vec3 kD = vec3(1.0) - kS;  // Diffuse reflection ratio
    kD *= 1.0 - metallic;  // Metallic surfaces have no diffuse reflection

    // Lambert diffuse BRDF
    float NdotL = max(dot(N, L), 0.0);

    // Final lighting contribution: (diffuse + specular) * radiance * NdotL
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    // Sample albedo (texture or material property)
    vec3 albedo;
    if ((pushConstants.materialFlags & (1u << 0)) != 0u) {  // HasAlbedoMap
        albedo = texture(albedoSampler, fragTexCoord).rgb;
    } else {
        albedo = material.albedo;
    }

    // Sample normal map (texture or vertex normal)
    vec3 N;
    if ((pushConstants.materialFlags & (1u << 1)) != 0u) {  // HasNormalMap
        N = getNormalFromMap(fragTexCoord, fragPosition, fragNormal);
    } else {
        N = normalize(fragNormal);
    }

    // Sample metallic, roughness, and AO based on texture flags
    float metallic;
    float roughness;
    float ao;

    if ((pushConstants.materialFlags & (1u << 4)) != 0u) {  // HasMetallicRoughnessMap (glTF)
        // glTF 2.0 standard: R=AO, G=roughness, B=metallic
        vec3 mrSample = texture(metallicSampler, fragTexCoord).rgb;
        ao = mrSample.r;
        roughness = mrSample.g;
        metallic = mrSample.b;
    } else {
        // Separate textures or material properties
        if ((pushConstants.materialFlags & (1u << 2)) != 0u) {  // HasMetallicMap
            metallic = texture(metallicSampler, fragTexCoord).r;
        } else {
            metallic = material.metallic;
        }

        if ((pushConstants.materialFlags & (1u << 3)) != 0u) {  // HasRoughnessMap
            roughness = texture(roughnessSampler, fragTexCoord).r;
        } else {
            roughness = material.roughness;
        }

        if ((pushConstants.materialFlags & (1u << 5)) != 0u) {  // HasAOMap
            ao = texture(aoSampler, fragTexCoord).r;
        } else {
            ao = 1.0;
        }
    }

    // Clamp roughness to prevent artifacts from infinitely sharp specular highlights
    // Minimum value of 0.04 provides reasonable results for smooth surfaces
    roughness = max(roughness, 0.04);

    // Prepare view direction
    vec3 V = normalize(pushConstants.cameraPosition.xyz - fragPosition);

    // Accumulate lighting from all lights using PBR
    vec3 Lo = vec3(0.0);
    for (uint i = 0u; i < lightBuffer.lightCount && i < 16u; i++) {
        Lo += calculatePBRLighting(
            lightBuffer.lights[i],
            fragPosition,
            N,
            V,
            albedo,
            roughness,
            metallic
        );
    }

    // Ambient lighting (simple approximation, will be replaced by IBL in Phase 3)
    // Apply AO to ambient term
    // Note: Using 0.15 as a compromise - too low (0.03) looks too dark without IBL,
    // too high (0.6) washes out the lighting. Will be replaced by proper IBL later.
    vec3 ambient = vec3(0.15) * albedo * ao;

    // Final color: ambient + direct lighting
    vec3 color = ambient + Lo;

    // Apply exposure control
    color = color * pushConstants.exposure;

    // Apply tone mapping (HDR to LDR)
    color = toneMapACES(color);

    // Gamma correction (convert from linear to sRGB)
    color = pow(color, vec3(1.0/2.2));

    // ============================================================================
    // DEBUG VISUALIZATION MODES
    // Uncomment ONE of these lines to visualize different PBR properties
    // ============================================================================

    // Material properties (DEBUG - uncomment to visualize)
    // outColor = vec4(albedo, 1.0);                // Albedo color (base color)
    // outColor = vec4(vec3(metallic), 1.0);        // Metallic map (grayscale)
    // outColor = vec4(vec3(roughness), 1.0);       // Roughness map (grayscale)
    // outColor = vec4(vec3(ao), 1.0);              // Ambient Occlusion (grayscale)

    // Normals (DEBUG - uncomment to visualize)
    // outColor = vec4(N * 0.5 + 0.5, 1.0); return;         // World-space normals as RGB
    // outColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);  // Vertex normals (before normal map)

    // Lighting components (DEBUG - uncomment to visualize)
    // outColor = vec4(ambient, 1.0);               // Ambient contribution only
    // outColor = vec4(Lo, 1.0); return;                    // Direct lighting only
    // outColor = vec4(color, 1.0); return;         // Color before tone mapping

    // DEBUG: Visualize light count
    // outColor = vec4(vec3(float(lightBuffer.lightCount) / 4.0), 1.0); return;

    // Advanced visualization (DEBUG - uncomment to visualize)
    // outColor = vec4(vec3(max(dot(N, V), 0.0)), 1.0);  // N·V (fresnel term)
    // float avgLight = (Lo.r + Lo.g + Lo.b) / 3.0;
    // outColor = vec4(vec3(avgLight), 1.0);        // Grayscale lighting intensity

    // Final output (default)
    outColor = vec4(color, 1.0);
}
