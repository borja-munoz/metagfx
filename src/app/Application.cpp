// ============================================================================
// src/app/Application.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "Application.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/core/Logger.h"
#include "Application.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/Shader.h"
#include "metagfx/rhi/Pipeline.h"
#include "metagfx/rhi/CommandBuffer.h"
#include "metagfx/rhi/SwapChain.h"
#include "metagfx/rhi/Texture.h"
#include <SDL3/SDL.h>

namespace metagfx {

Application::Application(const ApplicationConfig& config)
    : m_Config(config) {
    Init();
}

Application::~Application() {
    Shutdown();
}

void Application::Init() {
    METAGFX_INFO << "Initializing application...";
    
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        METAGFX_CRITICAL << "Failed to initialize SDL: " << SDL_GetError();
        return;
    }

    METAGFX_INFO << "SDL initialized successfully";

    // Create window
    uint32_t windowFlags = SDL_WINDOW_RESIZABLE;

#ifdef METAGFX_USE_VULKAN
    windowFlags |= SDL_WINDOW_VULKAN;
    METAGFX_INFO << "Creating window with Vulkan support";
#endif
    
    m_Window = SDL_CreateWindow(
        m_Config.title.c_str(),
        m_Config.width,
        m_Config.height,
        windowFlags
    );
    
    if (!m_Window) {
        METAGFX_CRITICAL << "Failed to create window: " << SDL_GetError();
        SDL_Quit();
        return;
    }

    METAGFX_INFO << "Window created: " << m_Config.width << "x" << m_Config.height;
    
    // Create graphics device
#ifdef METAGFX_USE_VULKAN
    m_Device = rhi::CreateGraphicsDevice(rhi::GraphicsAPI::Vulkan, m_Window);
    if (!m_Device) {
        METAGFX_ERROR << "Failed to create graphics device";
        return;
    }
#endif
    
    // Create triangle resources
    CreateTriangle();
    
    m_Running = true;
}

void Application::CreateTriangle() {
    using namespace rhi;
    
    // Vertex data: position (vec3) + color (vec3)
    float vertices[] = {
         0.0f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,  // Top (red)
        -0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,  // Bottom-left (green)
         0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f   // Bottom-right (blue)
    };
    
    // Create vertex buffer
    BufferDesc vertexBufferDesc{};
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = BufferUsage::Vertex;
    vertexBufferDesc.memoryUsage = MemoryUsage::CPUToGPU;
    
    m_VertexBuffer = m_Device->CreateBuffer(vertexBufferDesc);
    m_VertexBuffer->CopyData(vertices, sizeof(vertices));
    
    // Create shaders (SPIR-V bytecode)
    // Vertex shader
    std::vector<uint8> vertShaderCode = {
        // This is a minimal SPIR-V bytecode for a simple pass-through vertex shader
        // In practice, you'd compile from GLSL using glslangValidator or similar
        #include "triangle.vert.spv.inl"
    };
    
    ShaderDesc vertShaderDesc{};
    vertShaderDesc.stage = ShaderStage::Vertex;
    vertShaderDesc.code = vertShaderCode;
    vertShaderDesc.entryPoint = "main";
    
    auto vertShader = m_Device->CreateShader(vertShaderDesc);
    
    // Fragment shader
    std::vector<uint8> fragShaderCode = {
        #include "triangle.frag.spv.inl"
    };
    
    ShaderDesc fragShaderDesc{};
    fragShaderDesc.stage = ShaderStage::Fragment;
    fragShaderDesc.code = fragShaderCode;
    fragShaderDesc.entryPoint = "main";
    
    auto fragShader = m_Device->CreateShader(fragShaderDesc);
    
    // Create pipeline
    PipelineDesc pipelineDesc{};
    pipelineDesc.vertexShader = vertShader;
    pipelineDesc.fragmentShader = fragShader;
    
    // Vertex input: position and color
    pipelineDesc.vertexInput.stride = sizeof(float) * 6;
    pipelineDesc.vertexInput.attributes = {
        { 0, Format::R32G32B32_SFLOAT, 0 },                // position at location 0
        { 1, Format::R32G32B32_SFLOAT, sizeof(float) * 3 } // color at location 1
    };
    
    pipelineDesc.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = CullMode::None;
    
    m_Pipeline = m_Device->CreateGraphicsPipeline(pipelineDesc);
    
    METAGFX_INFO << "Triangle resources created";
}

void Application::Run() {
    METAGFX_INFO << "Starting main loop...";
    
    uint64_t lastTime = SDL_GetTicksNS();
    
    while (m_Running) {
        // Calculate delta time
        uint64_t currentTime = SDL_GetTicksNS();
        float deltaTime = (currentTime - lastTime) / 1000000000.0f;
        lastTime = currentTime;
        
        ProcessEvents();
        Update(deltaTime);
        Render();
    }

    METAGFX_INFO << "Main loop ended";
}

void Application::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                METAGFX_INFO << "Quit event received";
                m_Running = false;
                break;
                
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    METAGFX_INFO << "Escape key pressed";
                    m_Running = false;
                }
                break;
                
            case SDL_EVENT_WINDOW_RESIZED:
                METAGFX_INFO << "Window resized: " << event.window.data1 << "x" << event.window.data2;
                if (m_Device) {
                    m_Device->GetSwapChain()->Resize(event.window.data1, event.window.data2);
                }
                break;
        }
    }
}

void Application::Update(float deltaTime) {
    (void)deltaTime;
}

void Application::Render() {
    using namespace rhi;
    
    if (!m_Device) return;
    
    auto swapChain = m_Device->GetSwapChain();
    auto backBuffer = swapChain->GetCurrentBackBuffer();
    
    // Create command buffer
    auto cmd = m_Device->CreateCommandBuffer();
    cmd->Begin();
    
    // Begin rendering
    ClearValue clearValue{};
    clearValue.color[0] = 0.1f;
    clearValue.color[1] = 0.1f;
    clearValue.color[2] = 0.15f;
    clearValue.color[3] = 1.0f;
    
    cmd->BeginRendering({ backBuffer }, nullptr, { clearValue });
    
    // Set viewport and scissor
    Viewport viewport{};
    viewport.width = static_cast<float>(swapChain->GetWidth());
    viewport.height = static_cast<float>(swapChain->GetHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd->SetViewport(viewport);
    
    Rect2D scissor{};
    scissor.width = swapChain->GetWidth();
    scissor.height = swapChain->GetHeight();
    cmd->SetScissor(scissor);
    
    // Draw triangle
    cmd->BindPipeline(m_Pipeline);
    cmd->BindVertexBuffer(m_VertexBuffer);
    cmd->Draw(3);
    
    cmd->EndRendering();
    cmd->End();
    
    // Submit and present
    m_Device->SubmitCommandBuffer(cmd);
    swapChain->Present();
}

void Application::Shutdown() {
    if (m_Device) {
        m_Device->WaitIdle();
    }
    
    m_Pipeline.reset();
    m_VertexBuffer.reset();
    m_Device.reset();
    
    if (m_Window) {
        METAGFX_INFO << "Destroying window...";
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }

    METAGFX_INFO << "Shutting down SDL...";
    SDL_Quit();
}

} // namespace metagfx

