#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Material uniform (48 bytes for std140 alignment)
layout(binding = 1) uniform MaterialUBO {
    vec3 albedo;       // 12 bytes (offset 0)
    float roughness;   // 4 bytes  (offset 12)
    float metallic;    // 4 bytes  (offset 16)
    vec2 padding1;     // 8 bytes  (offset 20)
    vec3 emissiveFactor; // 12 bytes (offset 28)
    float padding2;    // 4 bytes  (offset 40)
} material;

// PBR texture samplers
layout(binding = 2) uniform sampler2D albedoSampler;
layout(binding = 4) uniform sampler2D normalSampler;
layout(binding = 5) uniform sampler2D metallicSampler;
layout(binding = 6) uniform sampler2D roughnessSampler;
layout(binding = 7) uniform sampler2D aoSampler;

// IBL texture samplers
layout(binding = 8) uniform samplerCube irradianceMap;
layout(binding = 9) uniform samplerCube prefilteredMap;
layout(binding = 10) uniform sampler2D brdfLUT;

// Emissive texture sampler
layout(binding = 11) uniform sampler2D emissiveSampler;

// Shadow map sampler (comparison sampler for PCF)
layout(binding = 12) uniform sampler2DShadow shadowMapSampler;

// Shadow uniform with light-space matrix
layout(binding = 13) uniform ShadowUBO {
    mat4 lightSpaceMatrix;  // Transform from world space to light space
    mat4 model;             // Model matrix (not used in fragment shader, but needed for alignment)
    float shadowBias;       // Bias to prevent shadow acne
    float padding[3];       // Padding for alignment
} shadow;

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

// Push constants for camera position, material flags, exposure, and IBL toggle
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;
    uint materialFlags;
    float exposure;
    uint enableIBL;  // 0 = disabled, 1 = enabled
    float iblIntensity;  // IBL contribution multiplier
    uint shadowDebugMode;  // 0 = normal, 1 = shadow factor, 2 = depth coords
    uint enableShadows;  // 0 = disabled, 1 = enabled
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

// Fresnel-Schlick with roughness for IBL
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
// Shadow Mapping with PCF
// ============================================================================

// Calculate shadow factor using PCF (Percentage Closer Filtering)
// Returns 0.0 for fully shadowed, 1.0 for fully lit
float calculateShadow(vec3 fragPos) {
    // Transform fragment position to light space
    vec4 fragPosLightSpace = shadow.lightSpaceMatrix * vec4(fragPos, 1.0);

    // Perform perspective divide (for orthographic this doesn't change anything, but we keep it for consistency)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0,1] range for texture sampling
    // IMPORTANT: In Vulkan, depth is already in [0,1] range, only X/Y need transformation
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    // projCoords.z is already in [0,1] for Vulkan (no transformation needed)

    // Check if fragment is outside shadow map bounds
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;  // Outside shadow map = fully lit
    }

    // Apply depth bias to prevent shadow acne
    float currentDepth = projCoords.z - shadow.shadowBias;

    // Clamp depth to [0, 1] range
    currentDepth = clamp(currentDepth, 0.0, 1.0);

    // PCF with 3x3 kernel for soft shadows
    float shadowFactor = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMapSampler, 0);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            vec3 sampleCoord = vec3(projCoords.xy + offset, currentDepth);
            // texture() with sampler2DShadow performs hardware PCF comparison
            shadowFactor += texture(shadowMapSampler, sampleCoord);
        }
    }
    shadowFactor /= 9.0;  // Average the 9 samples

    return shadowFactor;
}

// ============================================================================
// PBR Lighting Calculation
// ============================================================================

// Calculate PBR lighting contribution from a single light using Cook-Torrance BRDF
vec3 calculatePBRLighting(LightData light, vec3 fragPos, vec3 normal, vec3 viewDir,
                          vec3 albedo, float roughness, float metallic, float shadowFactor) {
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

    // Final lighting contribution: (diffuse + specular) * radiance * NdotL * shadow
    return (kD * albedo / PI + specular) * radiance * NdotL * shadowFactor;
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

    // Prepare view direction and calculate base reflectivity
    vec3 V = normalize(pushConstants.cameraPosition.xyz - fragPosition);
    float NdotV = max(dot(N, V), 0.0);

    // Calculate F0 (surface reflection at zero incidence)
    // For dielectrics, F0 is typically 0.04
    // For metals, F0 is the albedo color
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Calculate shadow factor (for directional light shadows)
    // If shadows are disabled, use 1.0 (fully lit)
    float shadowFactor = (pushConstants.enableShadows != 0u) ? calculateShadow(fragPosition) : 1.0;

    // Accumulate lighting from all lights using PBR
    vec3 Lo = vec3(0.0);
    for (uint i = 0u; i < lightBuffer.lightCount && i < 16u; i++) {
        // Apply shadows ONLY to the first directional light (the shadow casting light)
        int lightType = int(lightBuffer.lights[i].positionAndType.w);
        float lightShadow = (i == 0u && lightType == LIGHT_TYPE_DIRECTIONAL) ? shadowFactor : 1.0;

        Lo += calculatePBRLighting(
            lightBuffer.lights[i],
            fragPosition,
            N,
            V,
            albedo,
            roughness,
            metallic,
            lightShadow
        );
    }

    // ============================================================================
    // Image-Based Lighting (IBL)
    // ============================================================================

    // Declare IBL variables for debug visualization
    vec3 ambient;
    vec3 diffuseIBL = vec3(0.0);
    vec3 specularIBL = vec3(0.0);
    vec3 irradiance = vec3(0.0);
    vec3 prefilteredColor = vec3(0.0);
    vec2 brdf = vec2(0.0);

    if (pushConstants.enableIBL != 0u) {
        // IBL enabled: use environment maps for realistic ambient lighting

        // Reflection vector for specular IBL
        vec3 R = reflect(-V, N);

        // --- Diffuse IBL (Irradiance) ---
        // Sample irradiance map with the normal direction
        irradiance = texture(irradianceMap, N).rgb;

        // Calculate diffuse component
        // kD represents the refracted light (diffuse)
        // For energy conservation: kD = 1 - kS (where kS is Fresnel)
        vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kD = (1.0 - F) * (1.0 - metallic); // Metals have no diffuse

        diffuseIBL = kD * irradiance * albedo;

        // --- Specular IBL (Prefiltered Environment + BRDF LUT) ---
        // Sample prefiltered environment map based on roughness
        const float MAX_REFLECTION_LOD = 5.0; // Number of mip levels - 1
        float lod = roughness * MAX_REFLECTION_LOD;
        prefilteredColor = textureLod(prefilteredMap, R, lod).rgb;

        // Sample BRDF integration map (split-sum approximation)
        brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;

        // Combine prefiltered color with BRDF
        specularIBL = prefilteredColor * (F * brdf.x + brdf.y);

        // Combine diffuse and specular IBL
        // Scale by user-controlled intensity
        ambient = (diffuseIBL + specularIBL) * pushConstants.iblIntensity;
    } else {
        // IBL disabled: use simple constant ambient lighting
        ambient = vec3(0.03) * albedo * ao;
    }

    // Final color: IBL ambient + direct lighting
    vec3 color = ambient + Lo;

    // ============================================================================
    // EMISSIVE CONTRIBUTION
    // ============================================================================
    // Emissive light is self-illumination and is added AFTER lighting but BEFORE tone mapping
    // This ensures emissive materials can "bloom" in HDR and appear to glow
    vec3 emissive = vec3(0.0);
    if ((pushConstants.materialFlags & (1u << 6)) != 0u) {  // HasEmissiveMap
        emissive = texture(emissiveSampler, fragTexCoord).rgb * material.emissiveFactor;
    } else {
        emissive = material.emissiveFactor;
    }
    color += emissive;

    // Apply exposure control
    color = color * pushConstants.exposure;

    // Apply tone mapping (HDR to LDR)
    // NOTE: Using simple clamp instead of ACES - ACES was causing black artifacts with IBL
    color = clamp(color, 0.0, 1.0);

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
    // outColor = vec4(ambient, 1.0); return;       // IBL ambient contribution only
    // outColor = vec4(Lo, 1.0); return;            // Direct lighting only
    // outColor = vec4(color, 1.0); return;         // Color before tone mapping

    // IBL components (DEBUG - uncomment to visualize IBL)
    // outColor = vec4(diffuseIBL * 2.0, 1.0); return;    // Diffuse IBL only (scaled 2x for viewing)
    // outColor = vec4(specularIBL, 1.0); return;   // Specular IBL only (prefiltered + BRDF)
    // outColor = vec4(irradiance * 0.5, 1.0); return;    // Raw irradiance map sample (scaled for viewing)
    // outColor = vec4(prefilteredColor * 0.5, 1.0); return; // Raw prefiltered map sample (scaled for viewing)
    // outColor = vec4(vec3(brdf, 0.0), 1.0); return;  // BRDF LUT (RG only)

    // DEBUG: Visualize light count
    // outColor = vec4(vec3(float(lightBuffer.lightCount) / 4.0), 1.0); return;

    // Advanced visualization (DEBUG - uncomment to visualize)
    // outColor = vec4(vec3(max(dot(N, V), 0.0)), 1.0);  // N·V (fresnel term)
    // float avgLight = (Lo.r + Lo.g + Lo.b) / 3.0;
    // outColor = vec4(vec3(avgLight), 1.0);        // Grayscale lighting intensity

    // Shadow map visualization modes (for debugging)
    if (pushConstants.shadowDebugMode == 1u) {
        // Mode 1: Show shadow factor (white = lit, black = shadowed)
        outColor = vec4(vec3(shadowFactor), 1.0);
        return;
    } else if (pushConstants.shadowDebugMode == 2u) {
        // Mode 2: Show vertex normal as color (normals should definitely vary!)
        // Normals are in -1 to 1 range, remap to 0-1 for visualization
        vec3 normalColor = fragNormal * 0.5 + 0.5;
        outColor = vec4(normalColor, 1.0);
        return;
    } else if (pushConstants.shadowDebugMode == 3u) {
        // Mode 3: Show light-space depth coordinates
        vec4 fragPosLightSpace = shadow.lightSpaceMatrix * vec4(fragPosition, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

        // DIAGNOSTIC: Show if matrix is working
        // If fragPosition varies but fragPosLightSpace doesn't, the matrix is identity/broken
        // Visualize world-space position (should show gradients)
        vec3 worldViz = abs(fragPosition) / 10.0;  // Normalize by 10 units

        // Show NDC coordinates after light transform
        // IMPORTANT: In Vulkan, depth (Z) is already in [0,1], only X/Y need transformation
        vec3 ndcViz;
        ndcViz.x = projCoords.x * 0.5 + 0.5;
        ndcViz.y = projCoords.y * 0.5 + 0.5;
        ndcViz.z = projCoords.z;  // Already in [0,1] for Vulkan

        // Split screen: left half = world position, right half = light-space position
        if (fragTexCoord.x < 0.5) {
            // Left: world position (should show color gradients)
            outColor = vec4(worldViz, 1.0);
        } else {
            // Right: light-space position
            if (ndcViz.x < 0.0 || ndcViz.x > 1.0 || ndcViz.y < 0.0 || ndcViz.y > 1.0) {
                outColor = vec4(1.0, 0.0, 1.0, 1.0);  // Magenta = out of XY bounds
            } else if (ndcViz.z < 0.0 || ndcViz.z > 1.0) {
                outColor = vec4(1.0, 1.0, 0.0, 1.0);  // Yellow = out of depth bounds
            } else {
                outColor = vec4(ndcViz, 1.0);
            }
        }
        return;
    } else if (pushConstants.shadowDebugMode == 4u) {
        // Mode 4: Sample shadow map depth directly and visualize
        vec4 fragPosLightSpace = shadow.lightSpaceMatrix * vec4(fragPosition, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        // Transform X/Y to [0,1], Z is already in [0,1] for Vulkan
        projCoords.xy = projCoords.xy * 0.5 + 0.5;

        // Check bounds
        if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);  // Red = out of bounds
            return;
        }

        // Sample the shadow map depth (note: can't directly read depth from sampler2DShadow)
        // Instead, let's visualize the projected coordinates and current fragment depth
        float currentDepth = projCoords.z;

        // Visualize: R = projected X, G = projected Y, B = current depth
        outColor = vec4(projCoords.x, projCoords.y, currentDepth, 1.0);
        return;
    } else if (pushConstants.shadowDebugMode == 5u) {
        // Mode 5: Show just the shadow factor as grayscale (simpler than mode 1)
        // This helps see if ANY shadowing is happening
        float sf = (pushConstants.enableShadows != 0u) ? calculateShadow(fragPosition) : 1.0;
        outColor = vec4(vec3(sf), 1.0);
        return;
    } else if (pushConstants.shadowDebugMode == 6u) {
        // Mode 6: Show detailed shadow sampling debug info
        vec4 fragPosLightSpace = shadow.lightSpaceMatrix * vec4(fragPosition, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
        projCoords.xy = projCoords.xy * 0.5 + 0.5;

        // Check if in bounds
        if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);  // Red = out of XY bounds
            return;
        }
        if (projCoords.z < 0.0 || projCoords.z > 1.0) {
            outColor = vec4(1.0, 1.0, 0.0, 1.0);  // Yellow = out of Z bounds
            return;
        }

        // Sample shadow map at center (no PCF)
        float currentDepth = projCoords.z - shadow.shadowBias;
        currentDepth = clamp(currentDepth, 0.0, 1.0);
        float shadowSample = texture(shadowMapSampler, vec3(projCoords.xy, currentDepth));

        // Show: R = current depth, G = shadow sample result, B = 0
        outColor = vec4(currentDepth, shadowSample, 0.0, 1.0);
        return;
    }

    // Final output (default)
    outColor = vec4(color, 1.0);
}
