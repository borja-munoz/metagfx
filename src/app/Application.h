// ============================================================================
// src/app/Application.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/Pipeline.h"
#include <SDL3/SDL.h>
#include <string>

namespace metagfx {

// Forward declarations
namespace rhi {
    class GraphicsDevice;
    class Buffer;
    class Pipeline;
}

struct ApplicationConfig {
    std::string title = "MetaGFX";
    uint32 width = 1280;
    uint32 height = 720;
    bool vsync = true;
};

class Application {
public:
    Application(const ApplicationConfig& config);
    ~Application();

    void Run();
    void Shutdown();

private:
    void Init();
    void CreateTriangle();
    void ProcessEvents();
    void Update(float deltaTime);
    void Render();

    ApplicationConfig m_Config;
    SDL_Window* m_Window = nullptr;
    bool m_Running = false;
    
    // Graphics resources
    Ref<rhi::GraphicsDevice> m_Device;
    Ref<rhi::Buffer> m_VertexBuffer;
    Ref<rhi::Pipeline> m_Pipeline;
};

} // namespace metagfx