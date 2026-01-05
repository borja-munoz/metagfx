// ============================================================================
// tools/ibl_precompute/DDSWriter.cpp
// ============================================================================
#include "DDSWriter.h"
#include "metagfx/core/Types.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace metagfx {
namespace tools {

// DDS structures (copied from TextureUtils.cpp for standalone tool)
#pragma pack(push, 1)

struct DDSPixelFormat {
    uint32 size;
    uint32 flags;
    uint32 fourCC;
    uint32 RGBBitCount;
    uint32 RBitMask;
    uint32 GBitMask;
    uint32 BBitMask;
    uint32 ABitMask;
};

struct DDSHeader {
    uint32 size;
    uint32 flags;
    uint32 height;
    uint32 width;
    uint32 pitchOrLinearSize;
    uint32 depth;
    uint32 mipMapCount;
    uint32 reserved1[11];
    DDSPixelFormat ddspf;
    uint32 caps;
    uint32 caps2;
    uint32 caps3;
    uint32 caps4;
    uint32 reserved2;
};

struct DDSHeaderDXT10 {
    uint32 dxgiFormat;
    uint32 resourceDimension;
    uint32 miscFlag;
    uint32 arraySize;
    uint32 miscFlags2;
};

#pragma pack(pop)

// DDS constants
constexpr uint32 DDS_MAGIC = 0x20534444; // "DDS "
constexpr uint32 DDSD_CAPS = 0x1;
constexpr uint32 DDSD_HEIGHT = 0x2;
constexpr uint32 DDSD_WIDTH = 0x4;
constexpr uint32 DDSD_PITCH = 0x8;
constexpr uint32 DDSD_PIXELFORMAT = 0x1000;
constexpr uint32 DDSD_MIPMAPCOUNT = 0x20000;
constexpr uint32 DDSD_LINEARSIZE = 0x80000;
constexpr uint32 DDSD_DEPTH = 0x800000;

constexpr uint32 DDSCAPS_COMPLEX = 0x8;
constexpr uint32 DDSCAPS_TEXTURE = 0x1000;
constexpr uint32 DDSCAPS_MIPMAP = 0x400000;

constexpr uint32 DDSCAPS2_CUBEMAP = 0x200;
constexpr uint32 DDSCAPS2_CUBEMAP_POSITIVEX = 0x400;
constexpr uint32 DDSCAPS2_CUBEMAP_NEGATIVEX = 0x800;
constexpr uint32 DDSCAPS2_CUBEMAP_POSITIVEY = 0x1000;
constexpr uint32 DDSCAPS2_CUBEMAP_NEGATIVEY = 0x2000;
constexpr uint32 DDSCAPS2_CUBEMAP_POSITIVEZ = 0x4000;
constexpr uint32 DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x8000;
constexpr uint32 DDSCAPS2_CUBEMAP_ALLFACES =
    DDSCAPS2_CUBEMAP_POSITIVEX | DDSCAPS2_CUBEMAP_NEGATIVEX |
    DDSCAPS2_CUBEMAP_POSITIVEY | DDSCAPS2_CUBEMAP_NEGATIVEY |
    DDSCAPS2_CUBEMAP_POSITIVEZ | DDSCAPS2_CUBEMAP_NEGATIVEZ;

constexpr uint32 DDPF_FOURCC = 0x4;
constexpr uint32 DDPF_RGB = 0x40;
constexpr uint32 DDPF_RGBA = 0x41;

constexpr uint32 FOURCC_DX10 = 0x30315844; // "DX10"

// DXGI formats
constexpr uint32 DXGI_FORMAT_R16G16B16A16_FLOAT = 10;
constexpr uint32 DXGI_FORMAT_R16G16_FLOAT = 34;
constexpr uint32 DXGI_FORMAT_R32G32B32A32_FLOAT = 2;

// D3D10_RESOURCE_DIMENSION
constexpr uint32 D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3;

// Helper: Convert float to float16 (simplified, not full IEEE754)
uint16 FloatToFloat16(float value) {
    // Simple conversion (not production quality, but sufficient for tool)
    union {
        float f;
        uint32 i;
    } v;
    v.f = value;

    uint32 sign = (v.i >> 16) & 0x8000;
    uint32 exponent = ((v.i >> 23) & 0xFF) - 112;
    uint32 mantissa = (v.i >> 13) & 0x3FF;

    if (exponent <= 0) {
        return static_cast<uint16>(sign);
    } else if (exponent >= 31) {
        return static_cast<uint16>(sign | 0x7C00);
    }

    return static_cast<uint16>(sign | (exponent << 10) | mantissa);
}

bool DDSWriter::WriteCubemap(const std::string& filepath, const CubemapData& cubemap) {
    std::cout << "Writing DDS cubemap: " << filepath << std::endl;

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filepath << std::endl;
        return false;
    }

    // Write magic number
    file.write(reinterpret_cast<const char*>(&DDS_MAGIC), sizeof(DDS_MAGIC));

    // Prepare DDS header
    DDSHeader header = {};
    header.size = 124;
    header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;
    header.height = cubemap.height;
    header.width = cubemap.width;
    header.mipMapCount = cubemap.mipLevels;
    header.caps = DDSCAPS_TEXTURE | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP;
    header.caps2 = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_ALLFACES;

    // Pixel format: use DX10 extended header for float formats
    header.ddspf.size = 32;
    header.ddspf.flags = DDPF_FOURCC;
    header.ddspf.fourCC = FOURCC_DX10;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write DX10 extended header
    DDSHeaderDXT10 dx10Header = {};
    dx10Header.dxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    dx10Header.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    dx10Header.miscFlag = 0x4; // CUBEMAP flag
    dx10Header.arraySize = 1;
    dx10Header.miscFlags2 = 0;

    file.write(reinterpret_cast<const char*>(&dx10Header), sizeof(dx10Header));

    // Convert float data to float16 and write
    std::vector<uint16> float16Data;
    float16Data.reserve(cubemap.data.size());

    for (float value : cubemap.data) {
        float16Data.push_back(FloatToFloat16(value));
    }

    file.write(reinterpret_cast<const char*>(float16Data.data()), float16Data.size() * sizeof(uint16));

    if (!file) {
        std::cerr << "Failed to write cubemap data to: " << filepath << std::endl;
        return false;
    }

    file.close();
    std::cout << "  Successfully written: " << filepath << std::endl;
    return true;
}

bool DDSWriter::WriteTexture2D(const std::string& filepath, const Texture2DData& texture, bool twoChannel) {
    std::cout << "Writing DDS 2D texture: " << filepath << std::endl;

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filepath << std::endl;
        return false;
    }

    // Write magic number
    file.write(reinterpret_cast<const char*>(&DDS_MAGIC), sizeof(DDS_MAGIC));

    // Prepare DDS header
    DDSHeader header = {};
    header.size = 124;
    header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    header.height = texture.height;
    header.width = texture.width;
    header.mipMapCount = 1;
    header.caps = DDSCAPS_TEXTURE;

    // Pixel format: use DX10 extended header for float formats
    header.ddspf.size = 32;
    header.ddspf.flags = DDPF_FOURCC;
    header.ddspf.fourCC = FOURCC_DX10;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write DX10 extended header
    DDSHeaderDXT10 dx10Header = {};
    dx10Header.dxgiFormat = twoChannel ? DXGI_FORMAT_R16G16_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT;
    dx10Header.resourceDimension = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
    dx10Header.miscFlag = 0;
    dx10Header.arraySize = 1;
    dx10Header.miscFlags2 = 0;

    file.write(reinterpret_cast<const char*>(&dx10Header), sizeof(dx10Header));

    // Convert float data to float16 and write
    std::vector<uint16> float16Data;

    if (twoChannel) {
        // Only write RG channels
        float16Data.reserve(texture.width * texture.height * 2);
        for (size_t i = 0; i < texture.data.size(); i += 4) {
            float16Data.push_back(FloatToFloat16(texture.data[i + 0])); // R
            float16Data.push_back(FloatToFloat16(texture.data[i + 1])); // G
        }
    } else {
        // Write all RGBA channels
        float16Data.reserve(texture.data.size());
        for (float value : texture.data) {
            float16Data.push_back(FloatToFloat16(value));
        }
    }

    file.write(reinterpret_cast<const char*>(float16Data.data()), float16Data.size() * sizeof(uint16));

    if (!file) {
        std::cerr << "Failed to write 2D texture data to: " << filepath << std::endl;
        return false;
    }

    file.close();
    std::cout << "  Successfully written: " << filepath << std::endl;
    return true;
}

} // namespace tools
} // namespace metagfx
