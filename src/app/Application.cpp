// ============================================================================
// src/app/Application.cpp
// ============================================================================
#include "Application.h"
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/CommandBuffer.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Pipeline.h"
#include "metagfx/rhi/Sampler.h"
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
#include "metagfx/utils/TextureUtils.h"
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
    // Set up orbital camera centered on origin
    m_Camera->SetPosition(glm::vec3(0.0f, 1.0f, 8.0f));
    m_Camera->SetOrbitTarget(glm::vec3(0.0f, 0.0f, 0.0f));  // Orbit around model center

    // Don't enable relative mouse mode - we use click-and-drag instead
    // SDL_SetWindowRelativeMouseMode(m_Window, false);
    
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

    // Create shared sampler
    rhi::SamplerDesc samplerDesc{};
    samplerDesc.minFilter = rhi::Filter::Linear;
    samplerDesc.magFilter = rhi::Filter::Linear;
    samplerDesc.mipmapMode = rhi::Filter::Linear;
    samplerDesc.addressModeU = rhi::SamplerAddressMode::Repeat;
    samplerDesc.addressModeV = rhi::SamplerAddressMode::Repeat;
    samplerDesc.addressModeW = rhi::SamplerAddressMode::Repeat;
    samplerDesc.anisotropyEnable = true;
    samplerDesc.maxAnisotropy = 16.0f;
    m_LinearRepeatSampler = m_Device->CreateSampler(samplerDesc);

    // Create default UV checker texture (magenta/white pattern) - 128x128 with 8x8 pixel checkers
    // This creates a 16x16 grid of checkers to show UV mapping detail
    constexpr int texSize = 128;
    constexpr int checkerSize = 8;  // 8x8 pixel checker squares
    uint8_t checkerboardPixels[texSize * texSize * 4];
    for (int y = 0; y < texSize; y++) {
        for (int x = 0; x < texSize; x++) {
            int idx = (y * texSize + x) * 4;
            bool isMagenta = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            if (isMagenta) {
                checkerboardPixels[idx + 0] = 255;  // R - Magenta
                checkerboardPixels[idx + 1] = 0;    // G
                checkerboardPixels[idx + 2] = 255;  // B
                checkerboardPixels[idx + 3] = 255;  // A
            } else {
                checkerboardPixels[idx + 0] = 255;  // R - White
                checkerboardPixels[idx + 1] = 255;  // G
                checkerboardPixels[idx + 2] = 255;  // B
                checkerboardPixels[idx + 3] = 255;  // A
            }
        }
    }
    utils::ImageData checkerboardImage{checkerboardPixels, texSize, texSize, 4};
    m_DefaultTexture = utils::CreateTextureFromImage(
        m_Device.get(), checkerboardImage, rhi::Format::R8G8B8A8_UNORM
    );

    // Create default normal map (1x1, pointing up: RGB(128, 128, 255) = normal(0,0,1) in tangent space)
    uint8_t normalPixel[4] = {128, 128, 255, 255};  // RGBA
    utils::ImageData normalImage{normalPixel, 1, 1, 4};
    m_DefaultNormalMap = utils::CreateTextureFromImage(
        m_Device.get(), normalImage, rhi::Format::R8G8B8A8_UNORM
    );

    // Create default white texture (1x1 white pixel) - for missing metallic/roughness/AO
    uint8_t whitePixel[4] = {255, 255, 255, 255};  // RGBA white
    utils::ImageData whiteImage{whitePixel, 1, 1, 4};
    m_DefaultWhiteTexture = utils::CreateTextureFromImage(
        m_Device.get(), whiteImage, rhi::Format::R8G8B8A8_UNORM
    );

    // Create scene and initialize light buffer
    m_Scene = std::make_unique<Scene>();
    m_Scene->InitializeLightBuffer(m_Device.get());

    // Create test lights
    CreateTestLights();

    // Create descriptor set with 8 bindings (Phase 2: PBR textures)
    std::vector<rhi::DescriptorBinding> bindings = {
        {
            0,  // binding = 0 (MVP matrices)
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT,
            m_UniformBuffers[0],
            nullptr,  // texture
            nullptr   // sampler
        },
        {
            1,  // binding = 1 (Material properties)
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            m_MaterialBuffers[0],
            nullptr,  // texture
            nullptr   // sampler
        },
        {
            2,  // binding = 2 (Albedo texture sampler)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_DefaultTexture,
            m_LinearRepeatSampler
        },
        {
            3,  // binding = 3 (Light buffer)
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            m_Scene->GetLightBuffer(),
            nullptr,  // texture
            nullptr   // sampler
        },
        {
            4,  // binding = 4 (Normal map)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_DefaultNormalMap,
            m_LinearRepeatSampler
        },
        {
            5,  // binding = 5 (Metallic map)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_DefaultWhiteTexture,
            m_LinearRepeatSampler
        },
        {
            6,  // binding = 6 (Roughness map)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_DefaultWhiteTexture,
            m_LinearRepeatSampler
        },
        {
            7,  // binding = 7 (AO map)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_DefaultWhiteTexture,
            m_LinearRepeatSampler
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

    // Initialize available models list
    m_AvailableModels = {
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/DamagedHelmet.glb",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/MetalRoughSpheres.glb",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/bunny_tex_coords.obj",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/bunny.obj"
    };
    m_CurrentModelIndex = 0;

    // Load initial model
    LoadModel(m_AvailableModels[m_CurrentModelIndex]);

    METAGFX_INFO << "Controls:";
    METAGFX_INFO << "  WASD/QE - Camera movement";
    METAGFX_INFO << "  Mouse drag - Rotate camera";
    METAGFX_INFO << "  1-4 - Load specific model";
    METAGFX_INFO << "  N - Next model";
    METAGFX_INFO << "  P - Previous model";
    METAGFX_INFO << "  ESC - Exit";
    m_Running = true;
}

void Application::LoadModel(const std::string& path) {
    METAGFX_INFO << "Loading model: " << path;

    m_Model = std::make_unique<Model>();
    if (!m_Model->LoadFromFile(m_Device.get(), path)) {
        METAGFX_WARN << "Failed to load " << path << ", creating fallback cube";
        if (!m_Model->CreateCube(m_Device.get(), 1.0f)) {
            METAGFX_ERROR << "Failed to create fallback cube model";
            m_Model = nullptr;
            return;
        }
    }

    // Extract model name from path for display
    size_t lastSlash = path.find_last_of("/\\");
    std::string modelName = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    METAGFX_INFO << "Model loaded: " << modelName;
}

void Application::LoadNextModel() {
    m_CurrentModelIndex = (m_CurrentModelIndex + 1) % m_AvailableModels.size();
    LoadModel(m_AvailableModels[m_CurrentModelIndex]);
}

void Application::LoadPreviousModel() {
    m_CurrentModelIndex = (m_CurrentModelIndex - 1 + m_AvailableModels.size()) % m_AvailableModels.size();
    LoadModel(m_AvailableModels[m_CurrentModelIndex]);
}

void Application::CreateTestLights() {
    // Key light: Front-top directional light (main illumination)
    auto keyLight = std::make_unique<DirectionalLight>(
        glm::vec3(0.2f, -0.5f, -1.0f),    // Direction: from front-top toward model
        glm::vec3(1.0f, 1.0f, 1.0f),      // Pure white for neutral lighting
        5.0f                               // High intensity for main light
    );
    m_Scene->AddLight(std::move(keyLight));

    // Fill light: Side-back light for fill
    auto fillLight = std::make_unique<DirectionalLight>(
        glm::vec3(-0.7f, 0.0f, 0.5f),     // Direction: from side-back
        glm::vec3(0.8f, 0.9f, 1.0f),      // Slight cool tint
        2.5f                               // Medium intensity for fill
    );
    m_Scene->AddLight(std::move(fillLight));

    // Rim light: Back-top light for edge definition
    auto rimLight = std::make_unique<DirectionalLight>(
        glm::vec3(0.0f, -0.3f, 1.0f),     // Direction: from behind
        glm::vec3(1.0f, 0.95f, 0.85f),    // Warm tint for rim
        2.0f                               // Medium intensity
    );
    m_Scene->AddLight(std::move(rimLight));

    // Point light: Close to model for local highlights
    auto pointLight = std::make_unique<PointLight>(
        glm::vec3(1.0f, 0.5f, -1.5f),     // Position: front-right of model
        10.0f,                             // Range
        glm::vec3(1.0f, 1.0f, 1.0f),      // White color
        8.0f                               // High intensity
    );
    m_Scene->AddLight(std::move(pointLight));

    METAGFX_INFO << "Created " << m_Scene->GetLightCount() << " test lights";
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
    pipelineDesc.rasterization.frontFace = FrontFace::CounterClockwise;  // glTF uses CCW winding order

    // Enable depth testing for proper 3D rendering
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = true;

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
                // Model switching shortcuts
                else if (event.key.key == SDLK_N) {
                    METAGFX_INFO << "Loading next model...";
                    LoadNextModel();
                }
                else if (event.key.key == SDLK_P) {
                    METAGFX_INFO << "Loading previous model...";
                    LoadPreviousModel();
                }
                // Direct model selection (1-4)
                else if (event.key.key == SDLK_1 && m_AvailableModels.size() > 0) {
                    m_CurrentModelIndex = 0;
                    LoadModel(m_AvailableModels[m_CurrentModelIndex]);
                }
                else if (event.key.key == SDLK_2 && m_AvailableModels.size() > 1) {
                    m_CurrentModelIndex = 1;
                    LoadModel(m_AvailableModels[m_CurrentModelIndex]);
                }
                else if (event.key.key == SDLK_3 && m_AvailableModels.size() > 2) {
                    m_CurrentModelIndex = 2;
                    LoadModel(m_AvailableModels[m_CurrentModelIndex]);
                }
                else if (event.key.key == SDLK_4 && m_AvailableModels.size() > 3) {
                    m_CurrentModelIndex = 3;
                    LoadModel(m_AvailableModels[m_CurrentModelIndex]);
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    m_MouseButtonPressed = true;
                    m_FirstMouse = true;  // Reset for new drag
                    METAGFX_INFO << "Mouse button pressed - drag enabled";
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    m_MouseButtonPressed = false;
                    METAGFX_INFO << "Mouse button released";
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (m_MouseButtonPressed) {
                    if (m_FirstMouse) {
                        m_LastX = static_cast<float>(event.motion.x);
                        m_LastY = static_cast<float>(event.motion.y);
                        m_FirstMouse = false;
                    }

                    float xoffset = static_cast<float>(event.motion.x) - m_LastX;
                    float yoffset = m_LastY - static_cast<float>(event.motion.y);
                    m_LastX = static_cast<float>(event.motion.x);
                    m_LastY = static_cast<float>(event.motion.y);

                    // Use orbital camera rotation around target point
                    m_Camera->OrbitAroundTarget(xoffset, yoffset);
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                // Use zoom instead of scroll for orbital camera
                m_Camera->ZoomToTarget(static_cast<float>(event.wheel.y));
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
    // Process keyboard input for camera movement (WASD + QE)
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
    // Model matrix: flip upside down and no Y rotation to see default front
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));  // Flip upside down
    // No Y rotation - let's see what the default orientation shows
    ubo.model = modelMatrix;
    ubo.view = m_Camera->GetViewMatrix();
    ubo.projection = m_Camera->GetProjectionMatrix();

    // FIXME: Descriptor sets all point to buffer[0], so always update buffer[0]
    // TODO: Properly configure double buffering in descriptor sets
    m_UniformBuffers[0]->CopyData(&ubo, sizeof(ubo));

    // Update light buffer before rendering
    m_Scene->UpdateLightBuffer();
    
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
                Material* material = mesh->GetMaterial();

                // Update material buffer for this mesh
                MaterialProperties matProps = material->GetProperties();
                m_MaterialBuffers[0]->CopyData(&matProps, sizeof(matProps));

                // Bind all PBR textures (or defaults)

                // Binding 2: Albedo map
                Ref<rhi::Texture> albedoMap = material->GetAlbedoMap();
                if (albedoMap) {
                    m_DescriptorSet->UpdateTexture(2, albedoMap, m_LinearRepeatSampler);
                } else {
                    m_DescriptorSet->UpdateTexture(2, m_DefaultTexture, m_LinearRepeatSampler);
                }

                // Binding 4: Normal map
                Ref<rhi::Texture> normalMap = material->GetNormalMap();
                if (normalMap) {
                    m_DescriptorSet->UpdateTexture(4, normalMap, m_LinearRepeatSampler);
                } else {
                    m_DescriptorSet->UpdateTexture(4, m_DefaultNormalMap, m_LinearRepeatSampler);
                }

                // Binding 5: Metallic map
                Ref<rhi::Texture> metallicMap = material->GetMetallicMap();
                Ref<rhi::Texture> metallicRoughnessMap = material->GetMetallicRoughnessMap();
                if (metallicRoughnessMap) {
                    // Use combined texture for both metallic (binding 5) and roughness (binding 6)
                    m_DescriptorSet->UpdateTexture(5, metallicRoughnessMap, m_LinearRepeatSampler);
                    m_DescriptorSet->UpdateTexture(6, metallicRoughnessMap, m_LinearRepeatSampler);
                } else {
                    if (metallicMap) {
                        m_DescriptorSet->UpdateTexture(5, metallicMap, m_LinearRepeatSampler);
                    } else {
                        m_DescriptorSet->UpdateTexture(5, m_DefaultWhiteTexture, m_LinearRepeatSampler);
                    }

                    // Binding 6: Roughness map
                    Ref<rhi::Texture> roughnessMap = material->GetRoughnessMap();
                    if (roughnessMap) {
                        m_DescriptorSet->UpdateTexture(6, roughnessMap, m_LinearRepeatSampler);
                    } else {
                        m_DescriptorSet->UpdateTexture(6, m_DefaultWhiteTexture, m_LinearRepeatSampler);
                    }
                }

                // Binding 7: AO map
                Ref<rhi::Texture> aoMap = material->GetAOMap();
                if (aoMap) {
                    m_DescriptorSet->UpdateTexture(7, aoMap, m_LinearRepeatSampler);
                } else {
                    m_DescriptorSet->UpdateTexture(7, m_DefaultWhiteTexture, m_LinearRepeatSampler);
                }

                // Re-bind descriptor set after texture updates
                vkCmd->BindDescriptorSet(vkPipeline->GetLayout(),
                                        m_DescriptorSet->GetSet(0));

                // Push material flags and exposure (offset 16 bytes after cameraPosition vec4)
                uint32_t flags = material->GetTextureFlags();
                float exposure = 1.0f;  // Default exposure value

                // Debug: Log flags on first frame
                static bool loggedOnce = false;
                if (!loggedOnce) {
                    METAGFX_INFO << "Material texture flags: 0x" << std::hex << flags << std::dec
                                 << " (HasAlbedo=" << ((flags & 0x1) != 0)
                                 << ", HasNormal=" << ((flags & 0x2) != 0)
                                 << ", HasMetallic=" << ((flags & 0x4) != 0)
                                 << ", HasRoughness=" << ((flags & 0x8) != 0)
                                 << ", HasMetallicRoughness=" << ((flags & 0x10) != 0)
                                 << ", HasAO=" << ((flags & 0x20) != 0) << ")";
                    loggedOnce = true;
                }

                // Push flags (offset 16, size 4)
                vkCmd->PushConstants(vkPipeline->GetLayout(),
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    16, sizeof(uint32_t), &flags);

                // Push exposure (offset 20, size 4)
                vkCmd->PushConstants(vkPipeline->GetLayout(),
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    20, sizeof(float), &exposure);

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

    // Clean up scene and model
    m_Scene.reset();
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
    m_DefaultTexture.reset();         // Clean up before device
    m_DefaultNormalMap.reset();       // Clean up before device
    m_DefaultWhiteTexture.reset();    // Clean up before device
    m_LinearRepeatSampler.reset();    // Clean up before device
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

