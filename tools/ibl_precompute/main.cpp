// ============================================================================
// tools/ibl_precompute/main.cpp - IBL Precomputation Tool Entry Point
// ============================================================================
#include "IBLPrecompute.h"
#include "DDSWriter.h"
#include "metagfx/core/Types.h"

#include <iostream>
#include <string>
#include <filesystem>

using namespace metagfx;
using namespace metagfx::tools;

void PrintUsage(const char* programName) {
    std::cout << "MetaGFX IBL Precomputation Tool\n";
    std::cout << "================================\n\n";
    std::cout << "Usage: " << programName << " <input_hdr> <output_dir> [options]\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  input_hdr    Path to input HDR equirectangular environment map (.hdr file)\n";
    std::cout << "  output_dir   Directory to write output DDS files\n\n";
    std::cout << "Options:\n";
    std::cout << "  --env-size <size>         Cubemap size for environment (default: 1024)\n";
    std::cout << "  --irr-size <size>         Cubemap size for irradiance (default: 64)\n";
    std::cout << "  --pref-size <size>        Cubemap size for prefiltered map (default: 512)\n";
    std::cout << "  --pref-mips <count>       Number of mip levels for prefiltered map (default: 6)\n";
    std::cout << "  --lut-size <size>         Size of BRDF LUT (default: 512)\n";
    std::cout << "  --samples <count>         Number of samples per pixel (default: 1024)\n";
    std::cout << "  --fast                    Use fewer samples for faster processing (256 samples)\n\n";
    std::cout << "Output files (in output_dir):\n";
    std::cout << "  environment.dds           Original environment cubemap\n";
    std::cout << "  irradiance.dds           Irradiance map for diffuse IBL\n";
    std::cout << "  prefiltered.dds          Prefiltered environment map for specular IBL\n";
    std::cout << "  brdf_lut.dds             BRDF integration lookup table\n\n";
    std::cout << "Example:\n";
    std::cout << "  " << programName << " studio.hdr assets/envmaps/studio/\n";
    std::cout << "  " << programName << " outdoor.hdr assets/envmaps/outdoor/ --fast\n";
}

int main(int argc, char** argv) {
    // Parse command line arguments
    if (argc < 3) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string inputHDR = argv[1];
    std::string outputDir = argv[2];

    // Default settings
    uint32 envSize = 1024;
    uint32 irrSize = 64;
    uint32 prefSize = 512;
    uint32 prefMips = 6;
    uint32 lutSize = 512;
    uint32 samples = 1024;

    // Parse optional arguments
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--env-size" && i + 1 < argc) {
            envSize = std::atoi(argv[++i]);
        } else if (arg == "--irr-size" && i + 1 < argc) {
            irrSize = std::atoi(argv[++i]);
        } else if (arg == "--pref-size" && i + 1 < argc) {
            prefSize = std::atoi(argv[++i]);
        } else if (arg == "--pref-mips" && i + 1 < argc) {
            prefMips = std::atoi(argv[++i]);
        } else if (arg == "--lut-size" && i + 1 < argc) {
            lutSize = std::atoi(argv[++i]);
        } else if (arg == "--samples" && i + 1 < argc) {
            samples = std::atoi(argv[++i]);
        } else if (arg == "--fast") {
            samples = 256;
        }
    }

    // Validate input file
    if (!std::filesystem::exists(inputHDR)) {
        std::cerr << "Error: Input file does not exist: " << inputHDR << std::endl;
        return 1;
    }

    // Create output directory if it doesn't exist
    std::filesystem::create_directories(outputDir);

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "MetaGFX IBL Precomputation Tool\n";
    std::cout << "========================================\n";
    std::cout << "Input HDR:        " << inputHDR << "\n";
    std::cout << "Output directory: " << outputDir << "\n";
    std::cout << "Environment size: " << envSize << "x" << envSize << "\n";
    std::cout << "Irradiance size:  " << irrSize << "x" << irrSize << "\n";
    std::cout << "Prefiltered size: " << prefSize << "x" << prefSize << " (" << prefMips << " mips)\n";
    std::cout << "BRDF LUT size:    " << lutSize << "x" << lutSize << "\n";
    std::cout << "Samples per pixel: " << samples << "\n";
    std::cout << "========================================\n\n";

    // Create IBL precompute instance
    IBLPrecompute ibl;

    // Step 1: Load HDR environment
    if (!ibl.LoadHDREnvironment(inputHDR)) {
        std::cerr << "Error: Failed to load HDR environment\n";
        return 1;
    }

    // Step 2: Convert equirectangular to cubemap
    auto envCubemap = ibl.ConvertEquirectToCubemap(envSize);

    // Step 3: Generate irradiance map
    auto irradianceMap = ibl.GenerateIrradianceMap(envCubemap, irrSize, samples);

    // Step 4: Generate prefiltered environment map
    auto prefilteredMap = ibl.GeneratePrefilteredMap(envCubemap, prefSize, prefMips, samples);

    // Step 5: Generate BRDF LUT (environment-independent, only needs to be done once)
    auto brdfLUT = ibl.GenerateBRDFLUT(lutSize, samples);

    // Step 6: Write output files
    std::cout << "\nWriting output files...\n";

    std::filesystem::path outPath(outputDir);

    bool success = true;
    success &= DDSWriter::WriteCubemap((outPath / "environment.dds").string(), envCubemap);
    success &= DDSWriter::WriteCubemap((outPath / "irradiance.dds").string(), irradianceMap);
    success &= DDSWriter::WriteCubemap((outPath / "prefiltered.dds").string(), prefilteredMap);
    success &= DDSWriter::WriteTexture2D((outPath / "brdf_lut.dds").string(), brdfLUT, true); // 2-channel (RG)

    if (success) {
        std::cout << "\n========================================\n";
        std::cout << "IBL Precomputation Complete!\n";
        std::cout << "========================================\n";
        std::cout << "Output files written to: " << outputDir << "\n";
        std::cout << "  - environment.dds    (" << (envSize * envSize * 6 * 4 * 2 / 1024) << " KB)\n";
        std::cout << "  - irradiance.dds     (" << (irrSize * irrSize * 6 * 4 * 2 / 1024) << " KB)\n";
        std::cout << "  - prefiltered.dds    (varies, ~" << (prefSize * prefSize * 6 * 4 * 2 / 1024) << " KB)\n";
        std::cout << "  - brdf_lut.dds       (" << (lutSize * lutSize * 2 * 2 / 1024) << " KB)\n";
        std::cout << "\nYou can now load these textures in MetaGFX using utils::LoadDDSCubemap()\n";
        return 0;
    } else {
        std::cerr << "\nError: Failed to write one or more output files\n";
        return 1;
    }
}
