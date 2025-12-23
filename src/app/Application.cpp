// ============================================================================
// src/app/Application.cpp
// ============================================================================
#include "Application.h"
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/CommandBuffer.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Pipeline.h"
#include "metagfx/rhi/Shader.h"
#include "metagfx/rhi/SwapChain.h"
#include "metagfx/rhi/Texture.h"
#include "metagfx/rhi/vulkan/VulkanCommandBuffer.h"
#include "metagfx/rhi/vulkan/VulkanDevice.h"
#include "metagfx/rhi/vulkan/VulkanDescriptorSet.h"
#include "metagfx/rhi/vulkan/VulkanPipeline.h"
#include "metagfx/scene/Camera.h"
#include "metagfx/scene/Material.h"
#include "metagfx/scene/Mesh.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>

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

    // Create camera
    m_Camera = std::make_unique<Camera>(
        45.0f,
        static_cast<float>(m_Config.width) / static_cast<float>(m_Config.height),
        0.1f,
        100.0f
    );
    m_Camera->SetPosition(glm::vec3(0.0f, 0.0f, 3.0f));

    // Enable relative mouse mode for camera by default
    SDL_SetWindowRelativeMouseMode(m_Window, true);
    
    // Create uniform buffers (before creating pipeline)
    using namespace rhi;
    BufferDesc uniformBufferDesc{};
    uniformBufferDesc.size = sizeof(UniformBufferObject);
    uniformBufferDesc.usage = BufferUsage::Uniform;
    uniformBufferDesc.memoryUsage = MemoryUsage::CPUToGPU;

    m_UniformBuffers[0] = m_Device->CreateBuffer(uniformBufferDesc);
    m_UniformBuffers[1] = m_Device->CreateBuffer(uniformBufferDesc);

    // Create material buffers (double-buffered)
    BufferDesc materialBufferDesc{};
    materialBufferDesc.size = sizeof(MaterialProperties);
    materialBufferDesc.usage = BufferUsage::Uniform;
    materialBufferDesc.memoryUsage = MemoryUsage::CPUToGPU;

    m_MaterialBuffers[0] = m_Device->CreateBuffer(materialBufferDesc);
    m_MaterialBuffers[1] = m_Device->CreateBuffer(materialBufferDesc);
    
    // Create descriptor set with 2 bindings (test: does extra binding break it?)
    std::vector<rhi::DescriptorBinding> bindings = {
        {
            0,  // binding = 0 (MVP matrices)
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT,
            m_UniformBuffers[0]
        },
        {
            1,  // binding = 1 (Material - not used by shader yet)
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            m_MaterialBuffers[0]
        }
    };
    
    m_DescriptorSet = std::make_unique<rhi::VulkanDescriptorSet>(
        std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->GetContext(),
        bindings
    );

    // Set descriptor set layout on device before creating pipeline
    std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->SetDescriptorSetLayout(
        m_DescriptorSet->GetLayout()
    );

    // Create triangle resources
    CreateTriangle();

    // Create model pipeline
    CreateModelPipeline();

    // Load bunny model
    m_Model = std::make_unique<Model>();
    if (!m_Model->LoadFromFile(m_Device.get(), "../../assets/models/bunny.obj")) {
        METAGFX_WARN << "Failed to load bunny.obj, falling back to procedural cube";
        // Fallback to procedural cube if model loading fails
        if (!m_Model->CreateCube(m_Device.get(), 1.0f)) {
            METAGFX_ERROR << "Failed to create fallback cube model";
            return;
        }
    }

    METAGFX_INFO << "Model loaded successfully";
    METAGFX_INFO << "Controls: WASD to move, QE for up/down, Mouse to look, C to toggle camera, ESC to exit";
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

void Application::CreateModelPipeline() {
    using namespace rhi;

    // Load model shaders (SPIR-V bytecode)
    std::vector<uint8> vertShaderCode = {
        #include "model.vert.spv.inl"
    };

    ShaderDesc vertShaderDesc{};
    vertShaderDesc.stage = ShaderStage::Vertex;
    vertShaderDesc.code = vertShaderCode;
    vertShaderDesc.entryPoint = "main";

    auto vertShader = m_Device->CreateShader(vertShaderDesc);

    std::vector<uint8> fragShaderCode = {
        #include "model.frag.spv.inl"
    };

    ShaderDesc fragShaderDesc{};
    fragShaderDesc.stage = ShaderStage::Fragment;
    fragShaderDesc.code = fragShaderCode;
    fragShaderDesc.entryPoint = "main";

    auto fragShader = m_Device->CreateShader(fragShaderDesc);

    // Create pipeline for models
    PipelineDesc pipelineDesc{};
    pipelineDesc.vertexShader = vertShader;
    pipelineDesc.fragmentShader = fragShader;

    // Vertex input: position (vec3), normal (vec3), texcoord (vec2)
    pipelineDesc.vertexInput.stride = sizeof(Vertex);
    pipelineDesc.vertexInput.attributes = {
        { 0, Format::R32G32B32_SFLOAT, 0 },                          // position at location 0
        { 1, Format::R32G32B32_SFLOAT, sizeof(float) * 3 },          // normal at location 1
        { 2, Format::R32G32_SFLOAT, sizeof(float) * 6 }              // texcoord at location 2
    };

    pipelineDesc.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = CullMode::Back;
    pipelineDesc.rasterization.frontFace = FrontFace::Clockwise;  // Clockwise because projection Y-flip reverses winding

    // Enable depth testing (will be added later, for now just set defaults)
    pipelineDesc.depthStencil.depthTestEnable = false;
    pipelineDesc.depthStencil.depthWriteEnable = false;

    m_ModelPipeline = m_Device->CreateGraphicsPipeline(pipelineDesc);

    METAGFX_INFO << "Model pipeline created";
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
                // Toggle camera control with C key
                if (event.key.key == SDLK_C) {
                    m_CameraEnabled = !m_CameraEnabled;
                    SDL_SetWindowRelativeMouseMode(m_Window, m_CameraEnabled);
                    METAGFX_INFO << "Camera control " << (m_CameraEnabled ? "enabled" : "disabled");
                    m_FirstMouse = true;  // Reset mouse position
                }
                break;
                
            case SDL_EVENT_MOUSE_MOTION:
                if (m_CameraEnabled) {
                    if (m_FirstMouse) {
                        m_LastX = static_cast<float>(event.motion.x);
                        m_LastY = static_cast<float>(event.motion.y);
                        m_FirstMouse = false;
                    }
                    
                    float xoffset = static_cast<float>(event.motion.x) - m_LastX;
                    float yoffset = m_LastY - static_cast<float>(event.motion.y);
                    m_LastX = static_cast<float>(event.motion.x);
                    m_LastY = static_cast<float>(event.motion.y);
                    
                    m_Camera->ProcessMouseMovement(xoffset, yoffset);
                }
                break;
                
            case SDL_EVENT_MOUSE_WHEEL:
                if (m_CameraEnabled) {
                    m_Camera->ProcessMouseScroll(static_cast<float>(event.wheel.y));
                }
                break;
                
            case SDL_EVENT_WINDOW_RESIZED:
                METAGFX_INFO << "Window resized: " << event.window.data1 << "x" << event.window.data2;
                if (m_Device) {
                    m_Device->GetSwapChain()->Resize(event.window.data1, event.window.data2);
                    m_Camera->SetAspectRatio(
                        static_cast<float>(event.window.data1) / 
                        static_cast<float>(event.window.data2)
                    );
                }
                break;
        }
    }
}

// In Update():
void Application::Update(float deltaTime) {
    if (!m_CameraEnabled) return;
    
    // Process keyboard input for camera
    const bool* keyState = SDL_GetKeyboardState(nullptr);
    
    if (keyState[SDL_SCANCODE_W])
        m_Camera->ProcessKeyboard(SDLK_W, deltaTime);
    if (keyState[SDL_SCANCODE_S])
        m_Camera->ProcessKeyboard(SDLK_S, deltaTime);
    if (keyState[SDL_SCANCODE_A])
        m_Camera->ProcessKeyboard(SDLK_A, deltaTime);
    if (keyState[SDL_SCANCODE_D])
        m_Camera->ProcessKeyboard(SDLK_D, deltaTime);
    if (keyState[SDL_SCANCODE_Q])
        m_Camera->ProcessKeyboard(SDLK_Q, deltaTime);
    if (keyState[SDL_SCANCODE_E])
        m_Camera->ProcessKeyboard(SDLK_E, deltaTime);
}

// In Render():
void Application::Render() {
    using namespace rhi;
    
    if (!m_Device) return;
    
    auto swapChain = m_Device->GetSwapChain();
    auto backBuffer = swapChain->GetCurrentBackBuffer();
    
    // Update uniform buffer
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f),
                           static_cast<float>(SDL_GetTicks()) / 1000.0f * glm::radians(45.0f),
                           glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.view = m_Camera->GetViewMatrix();
    ubo.projection = m_Camera->GetProjectionMatrix();

    // FIXME: Descriptor sets all point to buffer[0], so always update buffer[0]
    // TODO: Properly configure double buffering in descriptor sets
    m_UniformBuffers[0]->CopyData(&ubo, sizeof(ubo));
    
    // Create command buffer
    auto cmd = m_Device->CreateCommandBuffer();
    auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
    
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
    
    // Draw the model
    if (m_Model && m_Model->IsValid()) {
        // Bind model pipeline
        cmd->BindPipeline(m_ModelPipeline);

        // Bind descriptor set (using set 0, all buffers point to [0])
        auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(m_ModelPipeline);
        vkCmd->BindDescriptorSet(vkPipeline->GetLayout(),
                                 m_DescriptorSet->GetSet(0));

        // Push camera position for specular lighting
        glm::vec4 cameraPos(m_Camera->GetPosition(), 1.0f);
        vkCmd->PushConstants(vkPipeline->GetLayout(),
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(glm::vec4), &cameraPos);

        // Draw all meshes in the model
        for (const auto& mesh : m_Model->GetMeshes()) {
            if (mesh && mesh->IsValid() && mesh->GetMaterial()) {
                // Update material buffer for this mesh
                MaterialProperties matProps = mesh->GetMaterial()->GetProperties();
                m_MaterialBuffers[0]->CopyData(&matProps, sizeof(matProps));

                // Bind and draw
                cmd->BindVertexBuffer(mesh->GetVertexBuffer());
                cmd->BindIndexBuffer(mesh->GetIndexBuffer());
                cmd->DrawIndexed(mesh->GetIndexCount());
            }
        }
    }

    cmd->EndRendering();
    cmd->End();
    
    // Submit and present
    m_Device->SubmitCommandBuffer(cmd);
    swapChain->Present();
    
    // Advance frame
    m_CurrentFrame = (m_CurrentFrame + 1) % 2;
}

void Application::Shutdown() {
    if (m_Device) {
        m_Device->WaitIdle();
    }

    // Clean up model
    if (m_Model) {
        m_Model->Cleanup();
        m_Model.reset();
    }

    m_ModelPipeline.reset();
    m_Pipeline.reset();
    m_VertexBuffer.reset();
    m_UniformBuffers[0].reset();
    m_UniformBuffers[1].reset();
    m_MaterialBuffers[0].reset();
    m_MaterialBuffers[1].reset();
    m_DescriptorSet.reset();
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

