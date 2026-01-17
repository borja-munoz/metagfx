// ============================================================================
// src/app/main.cpp
// ============================================================================
#include "Application.h"
#include "metagfx/core/Logger.h"
#include "metagfx/core/Platform.h"
#include "metagfx/rhi/Types.h"
#include <fstream>
#include <string>

namespace {

// Config file path (in current directory)
const char* CONFIG_FILE = "metagfx.cfg";

metagfx::rhi::GraphicsAPI LoadBackendPreference() {
    // Default to Vulkan
    metagfx::rhi::GraphicsAPI api = metagfx::rhi::GraphicsAPI::Vulkan;

    std::ifstream file(CONFIG_FILE);
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("backend=") == 0) {
                std::string backend = line.substr(8);
                if (backend == "Vulkan") {
                    api = metagfx::rhi::GraphicsAPI::Vulkan;
                } else if (backend == "Metal") {
                    api = metagfx::rhi::GraphicsAPI::Metal;
                } else if (backend == "Direct3D12") {
                    api = metagfx::rhi::GraphicsAPI::Direct3D12;
                } else if (backend == "WebGPU") {
                    api = metagfx::rhi::GraphicsAPI::WebGPU;
                }
                METAGFX_INFO << "Loaded backend preference from config: " << backend;
                break;
            }
        }
        file.close();
    }

    return api;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Initialize logger
    metagfx::Logger::Init();

    METAGFX_INFO << "===========================================";
    METAGFX_INFO << "  MetaGFX - A backend-agnostic physically-based renderer";
    METAGFX_INFO << "  Version: 0.1.0";
    METAGFX_INFO << "  Platform: " << metagfx::PlatformUtils::GetPlatformName();
    METAGFX_INFO << "===========================================";

    // Create and run application
    {
        metagfx::ApplicationConfig config;
        config.title = "MetaGFX";
        config.width = 1280;
        config.height = 720;
        config.vsync = true;
        config.graphicsAPI = LoadBackendPreference();

        metagfx::Application app(config);
        app.Run();
    }

    METAGFX_INFO << "Application terminated successfully";

    return 0;
}
