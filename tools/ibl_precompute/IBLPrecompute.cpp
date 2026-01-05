// ============================================================================
// tools/ibl_precompute/IBLPrecompute.cpp
// ============================================================================
#include "IBLPrecompute.h"
#include "metagfx/core/Logger.h"
#include "metagfx/utils/TextureUtils.h"

#include <glm/gtc/constants.hpp>
#include <cmath>
#include <iostream>

namespace metagfx {
namespace tools {

// ============================================================================
// CubemapData Helper
// ============================================================================

size_t CubemapData::GetOffset(uint32 face, uint32 mip) const {
    size_t offset = 0;

    // Skip all previous mip levels (all 6 faces)
    for (uint32 m = 0; m < mip; ++m) {
        uint32 mipWidth = GetMipWidth(m);
        uint32 mipHeight = GetMipHeight(m);
        offset += 6 * mipWidth * mipHeight * 4; // 4 channels (RGBA)
    }

    // Add offset for current mip level's previous faces
    uint32 mipWidth = GetMipWidth(mip);
    uint32 mipHeight = GetMipHeight(mip);
    offset += face * mipWidth * mipHeight * 4;

    return offset;
}

// ============================================================================
// IBLPrecompute Implementation
// ============================================================================

bool IBLPrecompute::LoadHDREnvironment(const std::string& filepath) {
    std::cout << "Loading HDR environment: " << filepath << std::endl;

    auto hdrData = utils::LoadHDRImage(filepath, 4);
    if (!hdrData.pixels) {
        std::cerr << "Failed to load HDR image: " << filepath << std::endl;
        return false;
    }

    m_EquirectWidth = hdrData.width;
    m_EquirectHeight = hdrData.height;

    // Copy to internal buffer
    size_t dataSize = m_EquirectWidth * m_EquirectHeight * 4;
    m_EquirectData.resize(dataSize);
    std::memcpy(m_EquirectData.data(), hdrData.pixels, dataSize * sizeof(float));

    utils::FreeHDRImage(hdrData);

    std::cout << "  Loaded: " << m_EquirectWidth << "x" << m_EquirectHeight << std::endl;
    return true;
}

glm::vec3 IBLPrecompute::SampleEquirect(const glm::vec3& direction) const {
    // Convert direction to spherical coordinates
    float phi = std::atan2(direction.z, direction.x);
    float theta = std::acos(direction.y);

    // Map to UV coordinates
    float u = phi / (2.0f * glm::pi<float>()) + 0.5f;
    float v = theta / glm::pi<float>();

    // Clamp and sample
    u = glm::clamp(u, 0.0f, 1.0f);
    v = glm::clamp(v, 0.0f, 1.0f);

    uint32 x = static_cast<uint32>(u * (m_EquirectWidth - 1));
    uint32 y = static_cast<uint32>(v * (m_EquirectHeight - 1));

    size_t index = (y * m_EquirectWidth + x) * 4;

    return glm::vec3(
        m_EquirectData[index + 0],
        m_EquirectData[index + 1],
        m_EquirectData[index + 2]
    );
}

glm::vec3 IBLPrecompute::GetCubemapDirection(uint32 face, float u, float v) {
    // Convert UV [0,1] to NDC [-1,1]
    float uc = 2.0f * u - 1.0f;
    float vc = 2.0f * v - 1.0f;

    switch (face) {
        case 0: return glm::normalize(glm::vec3( 1.0f,   vc,   -uc)); // +X
        case 1: return glm::normalize(glm::vec3(-1.0f,   vc,    uc)); // -X
        case 2: return glm::normalize(glm::vec3(   uc, 1.0f,   -vc)); // +Y
        case 3: return glm::normalize(glm::vec3(   uc, -1.0f,   vc)); // -Y
        case 4: return glm::normalize(glm::vec3(   uc,   vc,  1.0f)); // +Z
        case 5: return glm::normalize(glm::vec3(  -uc,   vc, -1.0f)); // -Z
        default: return glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

CubemapData IBLPrecompute::ConvertEquirectToCubemap(uint32 size) {
    std::cout << "Converting equirectangular to cubemap (" << size << "x" << size << ")..." << std::endl;

    CubemapData cubemap;
    cubemap.width = size;
    cubemap.height = size;
    cubemap.mipLevels = 1;
    cubemap.data.resize(6 * size * size * 4);

    glm::vec3 avgColor(0.0f);
    uint32 pixelCount = 0;

    for (uint32 face = 0; face < 6; ++face) {
        for (uint32 y = 0; y < size; ++y) {
            for (uint32 x = 0; x < size; ++x) {
                float u = (x + 0.5f) / size;
                float v = (y + 0.5f) / size;

                glm::vec3 dir = GetCubemapDirection(face, u, v);
                glm::vec3 color = SampleEquirect(dir);

                size_t index = cubemap.GetOffset(face, 0) + (y * size + x) * 4;
                cubemap.data[index + 0] = color.r;
                cubemap.data[index + 1] = color.g;
                cubemap.data[index + 2] = color.b;
                cubemap.data[index + 3] = 1.0f;

                avgColor += color;
                pixelCount++;
            }
        }
    }

    avgColor /= static_cast<float>(pixelCount);
    std::cout << "  Average color: RGB(" << avgColor.r << ", " << avgColor.g << ", " << avgColor.b << ")" << std::endl;
    std::cout << "  Conversion complete" << std::endl;
    return cubemap;
}

CubemapData IBLPrecompute::GenerateIrradianceMap(const CubemapData& envMap, uint32 size, uint32 sampleCount) {
    std::cout << "Generating irradiance map (" << size << "x" << size << ", " << sampleCount << " samples)..." << std::endl;

    CubemapData irradiance;
    irradiance.width = size;
    irradiance.height = size;
    irradiance.mipLevels = 1;
    irradiance.data.resize(6 * size * size * 4);

    for (uint32 face = 0; face < 6; ++face) {
        std::cout << "  Processing face " << (face + 1) << "/6..." << std::endl;

        for (uint32 y = 0; y < size; ++y) {
            for (uint32 x = 0; x < size; ++x) {
                float u = (x + 0.5f) / size;
                float v = (y + 0.5f) / size;

                glm::vec3 normal = GetCubemapDirection(face, u, v);

                // Convolution: integrate over hemisphere
                glm::vec3 irradianceSum(0.0f);
                float totalWeight = 0.0f;

                // Build tangent space
                glm::vec3 up = std::abs(normal.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
                glm::vec3 bitangent = glm::cross(normal, tangent);

                // Sample hemisphere
                float deltaPhi = (2.0f * glm::pi<float>()) / std::sqrt(static_cast<float>(sampleCount));
                float deltaTheta = (0.5f * glm::pi<float>()) / std::sqrt(static_cast<float>(sampleCount));

                for (float phi = 0.0f; phi < 2.0f * glm::pi<float>(); phi += deltaPhi) {
                    for (float theta = 0.0f; theta < 0.5f * glm::pi<float>(); theta += deltaTheta) {
                        // Spherical to Cartesian (in tangent space)
                        glm::vec3 tangentSample(
                            std::sin(theta) * std::cos(phi),
                            std::sin(theta) * std::sin(phi),
                            std::cos(theta)
                        );

                        // Transform to world space
                        glm::vec3 sampleDir = tangent * tangentSample.x + bitangent * tangentSample.y + normal * tangentSample.z;

                        // Sample environment map (we need to sample from envMap cubemap, not equirect)
                        // For now, sample from the input cubemap at mip 0
                        // This is a simplified approach; for better quality, interpolate between faces
                        glm::vec3 envColor(0.0f);

                        // Determine which cubemap face to sample
                        glm::vec3 absDir = glm::abs(sampleDir);
                        uint32 sampleFace;
                        glm::vec2 sampleUV;

                        if (absDir.x >= absDir.y && absDir.x >= absDir.z) {
                            // X-axis dominant
                            if (sampleDir.x > 0.0f) {
                                sampleFace = 0; // +X
                                sampleUV = glm::vec2(-sampleDir.z / absDir.x, sampleDir.y / absDir.x);
                            } else {
                                sampleFace = 1; // -X
                                sampleUV = glm::vec2(sampleDir.z / absDir.x, sampleDir.y / absDir.x);
                            }
                        } else if (absDir.y >= absDir.z) {
                            // Y-axis dominant
                            if (sampleDir.y > 0.0f) {
                                sampleFace = 2; // +Y
                                sampleUV = glm::vec2(sampleDir.x / absDir.y, -sampleDir.z / absDir.y);
                            } else {
                                sampleFace = 3; // -Y
                                sampleUV = glm::vec2(sampleDir.x / absDir.y, sampleDir.z / absDir.y);
                            }
                        } else {
                            // Z-axis dominant
                            if (sampleDir.z > 0.0f) {
                                sampleFace = 4; // +Z
                                sampleUV = glm::vec2(sampleDir.x / absDir.z, sampleDir.y / absDir.z);
                            } else {
                                sampleFace = 5; // -Z
                                sampleUV = glm::vec2(-sampleDir.x / absDir.z, sampleDir.y / absDir.z);
                            }
                        }

                        // Convert to [0,1] range and sample
                        sampleUV = sampleUV * 0.5f + 0.5f;
                        sampleUV = glm::clamp(sampleUV, 0.0f, 1.0f);

                        uint32 sx = static_cast<uint32>(sampleUV.x * (envMap.width - 1));
                        uint32 sy = static_cast<uint32>(sampleUV.y * (envMap.height - 1));
                        size_t sampleIndex = envMap.GetOffset(sampleFace, 0) + (sy * envMap.width + sx) * 4;

                        envColor = glm::vec3(
                            envMap.data[sampleIndex + 0],
                            envMap.data[sampleIndex + 1],
                            envMap.data[sampleIndex + 2]
                        );

                        // Accumulate with cosine weighting
                        float NdotL = std::max(std::cos(theta), 0.0f);
                        irradianceSum += envColor * NdotL * std::sin(theta);
                        totalWeight += NdotL * std::sin(theta);
                    }
                }

                // Normalize
                if (totalWeight > 0.0f) {
                    irradianceSum *= glm::pi<float>() / totalWeight;
                }

                // Write to output
                size_t index = irradiance.GetOffset(face, 0) + (y * size + x) * 4;
                irradiance.data[index + 0] = irradianceSum.r;
                irradiance.data[index + 1] = irradianceSum.g;
                irradiance.data[index + 2] = irradianceSum.b;
                irradiance.data[index + 3] = 1.0f;
            }
        }
    }

    std::cout << "  Irradiance map complete" << std::endl;
    return irradiance;
}

// Helper functions for GGX importance sampling

glm::vec2 IBLPrecompute::Hammersley(uint32 i, uint32 N) {
    // Van der Corput sequence (base 2)
    uint32 bits = i;
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float vdc = float(bits) * 2.3283064365386963e-10f; // / 0x100000000

    return glm::vec2(float(i) / float(N), vdc);
}

glm::vec3 IBLPrecompute::ImportanceSampleGGX(const glm::vec2& Xi, const glm::vec3& N, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0f * glm::pi<float>() * Xi.x;
    float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);

    // Spherical to cartesian (tangent space)
    glm::vec3 H;
    H.x = sinTheta * std::cos(phi);
    H.y = sinTheta * std::sin(phi);
    H.z = cosTheta;

    // Tangent to world space (use Y-up like GenerateIrradianceMap)
    glm::vec3 up = std::abs(N.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, N));
    glm::vec3 bitangent = glm::cross(N, tangent);

    return glm::normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float IBLPrecompute::DistributionGGX(const glm::vec3& N, const glm::vec3& H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = std::max(glm::dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = glm::pi<float>() * denom * denom;

    return a2 / std::max(denom, 0.0001f);
}

float IBLPrecompute::GeometrySchlickGGX(float NdotV, float roughness) {
    // Note: For IBL, we use a different k than direct lighting
    float a = roughness;
    float k = (a * a) / 2.0f;

    float denom = NdotV * (1.0f - k) + k;
    return NdotV / std::max(denom, 0.0001f);
}

float IBLPrecompute::GeometrySmith(const glm::vec3& N, const glm::vec3& V, const glm::vec3& L, float roughness) {
    float NdotV = std::max(glm::dot(N, V), 0.0f);
    float NdotL = std::max(glm::dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

CubemapData IBLPrecompute::GeneratePrefilteredMap(const CubemapData& envMap, uint32 size, uint32 mipLevels, uint32 sampleCount) {
    std::cout << "Generating prefiltered environment map (" << size << "x" << size << ", " << mipLevels << " mips, " << sampleCount << " samples)..." << std::endl;

    CubemapData prefiltered;
    prefiltered.width = size;
    prefiltered.height = size;
    prefiltered.mipLevels = mipLevels;

    // Calculate total size
    size_t totalSize = 0;
    for (uint32 mip = 0; mip < mipLevels; ++mip) {
        uint32 mipWidth = prefiltered.GetMipWidth(mip);
        uint32 mipHeight = prefiltered.GetMipHeight(mip);
        totalSize += 6 * mipWidth * mipHeight * 4;
    }
    prefiltered.data.resize(totalSize);

    for (uint32 mip = 0; mip < mipLevels; ++mip) {
        float roughness = static_cast<float>(mip) / static_cast<float>(mipLevels - 1);
        uint32 mipWidth = prefiltered.GetMipWidth(mip);
        uint32 mipHeight = prefiltered.GetMipHeight(mip);

        std::cout << "  Processing mip " << mip << "/" << (mipLevels - 1) << " (roughness=" << roughness << ", " << mipWidth << "x" << mipHeight << ")..." << std::endl;

        glm::vec3 mipAvgColor(0.0f);
        uint32 mipPixelCount = 0;

        for (uint32 face = 0; face < 6; ++face) {
            for (uint32 y = 0; y < mipHeight; ++y) {
                for (uint32 x = 0; x < mipWidth; ++x) {
                    float u = (x + 0.5f) / mipWidth;
                    float v = (y + 0.5f) / mipHeight;

                    glm::vec3 N = GetCubemapDirection(face, u, v);
                    glm::vec3 R = N; // Assume view direction == normal (for prefiltering)
                    glm::vec3 V = R;

                    glm::vec3 prefilteredColor(0.0f);
                    float totalWeight = 0.0f;

                    for (uint32 i = 0; i < sampleCount; ++i) {
                        glm::vec2 Xi = Hammersley(i, sampleCount);
                        glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                        glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                        float NdotL = std::max(glm::dot(N, L), 0.0f);

                        if (NdotL > 0.0f) {
                            // Sample environment map in direction L
                            glm::vec3 envColor(0.0f);

                            // Determine face and UV for L
                            glm::vec3 absL = glm::abs(L);
                            uint32 sampleFace;
                            glm::vec2 sampleUV;

                            if (absL.x >= absL.y && absL.x >= absL.z) {
                                if (L.x > 0.0f) {
                                    sampleFace = 0;
                                    sampleUV = glm::vec2(-L.z / absL.x, L.y / absL.x);
                                } else {
                                    sampleFace = 1;
                                    sampleUV = glm::vec2(L.z / absL.x, L.y / absL.x);
                                }
                            } else if (absL.y >= absL.z) {
                                if (L.y > 0.0f) {
                                    sampleFace = 2;
                                    sampleUV = glm::vec2(L.x / absL.y, -L.z / absL.y);
                                } else {
                                    sampleFace = 3;
                                    sampleUV = glm::vec2(L.x / absL.y, L.z / absL.y);
                                }
                            } else {
                                if (L.z > 0.0f) {
                                    sampleFace = 4;
                                    sampleUV = glm::vec2(L.x / absL.z, L.y / absL.z);
                                } else {
                                    sampleFace = 5;
                                    sampleUV = glm::vec2(-L.x / absL.z, L.y / absL.z);
                                }
                            }

                            sampleUV = sampleUV * 0.5f + 0.5f;
                            sampleUV = glm::clamp(sampleUV, 0.0f, 1.0f);

                            uint32 sx = static_cast<uint32>(sampleUV.x * (envMap.width - 1));
                            uint32 sy = static_cast<uint32>(sampleUV.y * (envMap.height - 1));
                            size_t sampleIndex = envMap.GetOffset(sampleFace, 0) + (sy * envMap.width + sx) * 4;

                            envColor = glm::vec3(
                                envMap.data[sampleIndex + 0],
                                envMap.data[sampleIndex + 1],
                                envMap.data[sampleIndex + 2]
                            );

                            prefilteredColor += envColor * NdotL;
                            totalWeight += NdotL;
                        }
                    }

                    if (totalWeight > 0.0f) {
                        prefilteredColor /= totalWeight;
                    }

                    size_t index = prefiltered.GetOffset(face, mip) + (y * mipWidth + x) * 4;
                    prefiltered.data[index + 0] = prefilteredColor.r;
                    prefiltered.data[index + 1] = prefilteredColor.g;
                    prefiltered.data[index + 2] = prefilteredColor.b;
                    prefiltered.data[index + 3] = 1.0f;

                    mipAvgColor += prefilteredColor;
                    mipPixelCount++;
                }
            }
        }

        mipAvgColor /= static_cast<float>(mipPixelCount);
        std::cout << "    Mip " << mip << " average color: RGB("
                  << mipAvgColor.r << ", " << mipAvgColor.g << ", " << mipAvgColor.b << ")" << std::endl;
    }

    std::cout << "  Prefiltered map complete" << std::endl;
    return prefiltered;
}

Texture2DData IBLPrecompute::GenerateBRDFLUT(uint32 size, uint32 sampleCount) {
    std::cout << "Generating BRDF LUT (" << size << "x" << size << ", " << sampleCount << " samples)..." << std::endl;

    Texture2DData lut;
    lut.width = size;
    lut.height = size;
    lut.data.resize(size * size * 4); // RGBA, but we'll only use RG

    for (uint32 y = 0; y < size; ++y) {
        for (uint32 x = 0; x < size; ++x) {
            float NdotV = (x + 0.5f) / size;
            float roughness = (y + 0.5f) / size;

            // Clamp NdotV to avoid division by zero
            NdotV = std::max(NdotV, 0.001f);

            glm::vec3 V;
            V.x = std::sqrt(1.0f - NdotV * NdotV); // sin
            V.y = 0.0f;
            V.z = NdotV; // cos

            glm::vec3 N(0.0f, 0.0f, 1.0f);

            float A = 0.0f;
            float B = 0.0f;

            for (uint32 i = 0; i < sampleCount; ++i) {
                glm::vec2 Xi = Hammersley(i, sampleCount);
                glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

                float NdotL = std::max(L.z, 0.0f);
                float NdotH = std::max(H.z, 0.0f);
                float VdotH = std::max(glm::dot(V, H), 0.0f);

                if (NdotL > 0.0f) {
                    float G = GeometrySmith(N, V, L, roughness);
                    float G_Vis = (G * VdotH) / std::max(NdotH * NdotV, 0.0001f);
                    float Fc = std::pow(1.0f - VdotH, 5.0f);

                    A += (1.0f - Fc) * G_Vis;
                    B += Fc * G_Vis;
                }
            }

            A /= static_cast<float>(sampleCount);
            B /= static_cast<float>(sampleCount);

            size_t index = (y * size + x) * 4;
            lut.data[index + 0] = A;
            lut.data[index + 1] = B;
            lut.data[index + 2] = 0.0f; // Unused
            lut.data[index + 3] = 1.0f; // Unused
        }
    }

    std::cout << "  BRDF LUT complete" << std::endl;
    return lut;
}

} // namespace tools
} // namespace metagfx
