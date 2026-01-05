// ============================================================================
// tools/ibl_precompute/IBLPrecompute.h - IBL Texture Generation
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace metagfx {
namespace tools {

// Cubemap face data (6 faces)
struct CubemapData {
    std::vector<float> data; // Interleaved RGBA float data for all 6 faces
    uint32 width = 0;
    uint32 height = 0;
    uint32 mipLevels = 1;

    // Get offset to specific face and mip level
    size_t GetOffset(uint32 face, uint32 mip = 0) const;

    // Get mip dimensions
    uint32 GetMipWidth(uint32 mip) const { return std::max(1u, width >> mip); }
    uint32 GetMipHeight(uint32 mip) const { return std::max(1u, height >> mip); }
};

// 2D texture data (e.g., BRDF LUT)
struct Texture2DData {
    std::vector<float> data; // RGBA float data
    uint32 width = 0;
    uint32 height = 0;
};

// IBL Precomputation class
class IBLPrecompute {
public:
    IBLPrecompute() = default;
    ~IBLPrecompute() = default;

    // Load HDR equirectangular environment map
    bool LoadHDREnvironment(const std::string& filepath);

    // Convert equirectangular to cubemap
    CubemapData ConvertEquirectToCubemap(uint32 size);

    // Generate diffuse irradiance map
    CubemapData GenerateIrradianceMap(const CubemapData& envMap, uint32 size, uint32 sampleCount = 1024);

    // Generate prefiltered specular environment map
    CubemapData GeneratePrefilteredMap(const CubemapData& envMap, uint32 size, uint32 mipLevels, uint32 sampleCount = 1024);

    // Generate BRDF integration lookup table
    Texture2DData GenerateBRDFLUT(uint32 size, uint32 sampleCount = 1024);

private:
    // Helper: Sample equirectangular map
    glm::vec3 SampleEquirect(const glm::vec3& direction) const;

    // Helper: Get cubemap direction from UV and face
    static glm::vec3 GetCubemapDirection(uint32 face, float u, float v);

    // Helper: Hammersley sequence for low-discrepancy sampling
    static glm::vec2 Hammersley(uint32 i, uint32 N);

    // Helper: Importance sample GGX distribution
    static glm::vec3 ImportanceSampleGGX(const glm::vec2& Xi, const glm::vec3& N, float roughness);

    // Helper: GGX normal distribution function
    static float DistributionGGX(const glm::vec3& N, const glm::vec3& H, float roughness);

    // Helper: Geometry function (Schlick-GGX)
    static float GeometrySchlickGGX(float NdotV, float roughness);
    static float GeometrySmith(const glm::vec3& N, const glm::vec3& V, const glm::vec3& L, float roughness);

    // Equirectangular environment data
    std::vector<float> m_EquirectData;
    uint32 m_EquirectWidth = 0;
    uint32 m_EquirectHeight = 0;
};

} // namespace tools
} // namespace metagfx
