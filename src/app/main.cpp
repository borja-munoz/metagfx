// ============================================================================
// src/app/main.cpp
// ============================================================================
#include "Application.h"
#include "metagfx/core/Logger.h"
#include "metagfx/core/Platform.h"

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

        metagfx::Application app(config);
        app.Run();
    }

    METAGFX_INFO << "Application terminated successfully";

    return 0;
}
