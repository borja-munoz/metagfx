// ============================================================================
// src/app/Application.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/Pipeline.h"
#include "metagfx/scene/Camera.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <string>

namespace metagfx {

// Forward declarations
namespace rhi {
    class GraphicsDevice;
    class Buffer;
    class Pipeline;
    class VulkanDescriptorSet;
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

    // Camera
    std::unique_ptr<Camera> m_Camera;
    bool m_FirstMouse = true;
    float m_LastX = 640.0f;
    float m_LastY = 360.0f;
    bool m_CameraEnabled = false;
    
    // Uniform buffers
    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };
    
    Ref<rhi::Buffer> m_UniformBuffers[2];  // Double buffering
    std::unique_ptr<rhi::VulkanDescriptorSet> m_DescriptorSet;
    uint32 m_CurrentFrame = 0;
};

} // namespace metagfx