// ============================================================================
// tools/ibl_precompute/DDSWriter.h - DDS File Writer
// ============================================================================
#pragma once

#include "IBLPrecompute.h"
#include <string>

namespace metagfx {
namespace tools {

// DDS file writer for cubemaps and 2D textures
class DDSWriter {
public:
    // Write cubemap to DDS file (R16G16B16A16_FLOAT format)
    static bool WriteCubemap(const std::string& filepath, const CubemapData& cubemap);

    // Write 2D texture to DDS file (R16G16_FLOAT format for BRDF LUT)
    static bool WriteTexture2D(const std::string& filepath, const Texture2DData& texture, bool twoChannel = false);
};

} // namespace tools
} // namespace metagfx
