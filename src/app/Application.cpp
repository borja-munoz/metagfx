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
#include "metagfx/rhi/vulkan/VulkanBuffer.h"
#include "metagfx/rhi/vulkan/VulkanCommandBuffer.h"
#include "metagfx/rhi/vulkan/VulkanDevice.h"
#include "metagfx/rhi/vulkan/VulkanDescriptorSet.h"
#include "metagfx/rhi/vulkan/VulkanPipeline.h"
#include "metagfx/rhi/vulkan/VulkanSwapChain.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"
#include "metagfx/scene/Camera.h"
#include "metagfx/scene/Material.h"
#include "metagfx/scene/Mesh.h"
#include "metagfx/scene/ShadowMap.h"
#include "metagfx/utils/TextureUtils.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

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

    // Create ground plane material buffer (dedicated to avoid conflicts)
    m_GroundPlaneMaterialBuffer = m_Device->CreateBuffer(materialBufferDesc);

    // Create shadow uniform buffer
    struct ShadowUBO {
        glm::mat4 lightSpaceMatrix;
        glm::mat4 model;  // Model matrix
        float shadowBias;
        float padding[3];
    };
    BufferDesc shadowBufferDesc{};
    shadowBufferDesc.size = sizeof(ShadowUBO);
    shadowBufferDesc.usage = BufferUsage::Uniform;
    shadowBufferDesc.memoryUsage = MemoryUsage::CPUToGPU;
    m_ShadowUniformBuffer = m_Device->CreateBuffer(shadowBufferDesc);

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

    // Create default black texture (1x1 black pixel) - for missing emissive map
    uint8_t blackPixel[4] = {0, 0, 0, 255};  // RGBA black (no emission)
    utils::ImageData blackImage{blackPixel, 1, 1, 4};
    m_DefaultBlackTexture = utils::CreateTextureFromImage(
        m_Device.get(), blackImage, rhi::Format::R8G8B8A8_UNORM
    );

    // Create depth buffer for 3D rendering
    auto swapChain = m_Device->GetSwapChain();
    rhi::TextureDesc depthDesc{};
    depthDesc.width = swapChain->GetWidth();
    depthDesc.height = swapChain->GetHeight();
    depthDesc.format = rhi::Format::D32_SFLOAT;
    depthDesc.usage = rhi::TextureUsage::DepthStencilAttachment;
    depthDesc.debugName = "DepthBuffer";
    m_DepthBuffer = m_Device->CreateTexture(depthDesc);

    // Create cubemap sampler for IBL textures
    rhi::SamplerDesc cubemapSamplerDesc{};
    cubemapSamplerDesc.minFilter = rhi::Filter::Linear;
    cubemapSamplerDesc.magFilter = rhi::Filter::Linear;
    cubemapSamplerDesc.mipmapMode = rhi::Filter::Linear;  // Enable mipmap filtering for prefiltered map
    cubemapSamplerDesc.addressModeU = rhi::SamplerAddressMode::ClampToEdge;
    cubemapSamplerDesc.addressModeV = rhi::SamplerAddressMode::ClampToEdge;
    cubemapSamplerDesc.addressModeW = rhi::SamplerAddressMode::ClampToEdge;
    m_CubemapSampler = m_Device->CreateSampler(cubemapSamplerDesc);

    // Load IBL textures (Image-Based Lighting)
    // These textures are pre-computed using the ibl_precompute tool
    METAGFX_INFO << "Loading IBL textures...";
    m_IrradianceMap = utils::LoadDDSCubemap(m_Device.get(),
        "/Users/Borja/dev/borja-munoz/metagfx/assets/envmaps/irradiance.dds");
    m_PrefilteredMap = utils::LoadDDSCubemap(m_Device.get(),
        "/Users/Borja/dev/borja-munoz/metagfx/assets/envmaps/prefiltered.dds");
    m_BRDF_LUT = utils::LoadDDS2DTexture(m_Device.get(),
        "/Users/Borja/dev/borja-munoz/metagfx/assets/envmaps/brdf_lut.dds");
    m_EnvironmentMap = utils::LoadDDSCubemap(m_Device.get(),
        "/Users/Borja/dev/borja-munoz/metagfx/assets/envmaps/environment.dds");

    if (!m_IrradianceMap || !m_PrefilteredMap || !m_BRDF_LUT) {
        METAGFX_WARN << "Failed to load IBL textures! Using fallback textures.";
        METAGFX_WARN << "IBL will be disabled. Generate textures using: ./bin/tools/ibl_precompute <input.hdr> assets/envmaps/studio/";

        // Create fallback 1x1 black cubemap for irradiance and prefiltered
        rhi::TextureDesc cubemapDesc{};
        cubemapDesc.type = rhi::TextureType::TextureCube;
        cubemapDesc.width = 1;
        cubemapDesc.height = 1;
        cubemapDesc.arrayLayers = 6;
        cubemapDesc.format = rhi::Format::R8G8B8A8_UNORM;
        cubemapDesc.usage = rhi::TextureUsage::Sampled;

        if (!m_IrradianceMap) {
            m_IrradianceMap = m_Device->CreateTexture(cubemapDesc);
            uint8_t blackPixels[6 * 4] = {0}; // 6 faces, RGBA black
            m_IrradianceMap->UploadData(blackPixels, sizeof(blackPixels));
        }

        if (!m_PrefilteredMap) {
            m_PrefilteredMap = m_Device->CreateTexture(cubemapDesc);
            uint8_t blackPixels[6 * 4] = {0};
            m_PrefilteredMap->UploadData(blackPixels, sizeof(blackPixels));
        }

        // Use white texture for BRDF LUT fallback
        if (!m_BRDF_LUT) {
            m_BRDF_LUT = m_DefaultWhiteTexture;
        }

        // Disable IBL by default if textures failed to load
        m_EnableIBL = false;
    } else {
        METAGFX_INFO << "IBL textures loaded successfully";
    }

    // Create scene and initialize light buffer
    m_Scene = std::make_unique<Scene>();
    m_Scene->InitializeLightBuffer(m_Device.get());

    // Create test lights
    CreateTestLights();

    // Upload light data to GPU before creating descriptor sets
    m_Scene->UpdateLightBuffer();

    // Create shadow map (2048x2048 default resolution)
    m_ShadowMap = std::make_unique<ShadowMap>(m_Device, 2048, 2048);

    // Create descriptor set with 14 bindings (added shadow map sampler and shadow UBO)
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
            3,  // binding = 3 (Light buffer) - CHANGED TO STORAGE BUFFER for MoltenVK compatibility
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
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
        },
        {
            8,  // binding = 8 (Irradiance cubemap for diffuse IBL)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_IrradianceMap,
            m_CubemapSampler
        },
        {
            9,  // binding = 9 (Prefiltered cubemap for specular IBL)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_PrefilteredMap,
            m_CubemapSampler
        },
        {
            10,  // binding = 10 (BRDF LUT for split-sum approximation)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_BRDF_LUT,
            m_LinearRepeatSampler  // 2D texture, use regular sampler
        },
        {
            11,  // binding = 11 (Emissive texture)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_DefaultBlackTexture,  // Default: no emission
            m_LinearRepeatSampler
        },
        {
            12,  // binding = 12 (Shadow map sampler - comparison sampler)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_ShadowMap->GetDepthTexture(),
            m_ShadowMap->GetSampler()
        },
        {
            13,  // binding = 13 (Shadow UBO - light space matrix + bias)
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            m_ShadowUniformBuffer,
            nullptr,  // texture
            nullptr   // sampler
        }
    };

    m_DescriptorSet = std::make_unique<rhi::VulkanDescriptorSet>(
        std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->GetContext(),
        bindings
    );

    // Create ground plane descriptor set (same layout as model, but separate instance)
    // We MUST use the same layout because they share the same pipeline
    std::vector<rhi::DescriptorBinding> groundPlaneBindings = bindings;  // Copy all bindings

    // CRITICAL: Array index corresponds to binding number!
    // Index 0 = binding 0 (MVP), Index 1 = binding 1 (Material), Index 2 = binding 2 (Albedo), etc.

    // Use dedicated material buffer for ground plane (binding 1 = index 1)
    groundPlaneBindings[1].buffer = m_GroundPlaneMaterialBuffer;  // Dedicated material buffer

    // Set default textures for ground plane
    groundPlaneBindings[2].texture = m_DefaultWhiteTexture;   // binding 2: Albedo
    groundPlaneBindings[4].texture = m_DefaultNormalMap;      // binding 4: Normal
    groundPlaneBindings[5].texture = m_DefaultWhiteTexture;   // binding 5: Metallic
    groundPlaneBindings[6].texture = m_DefaultWhiteTexture;   // binding 6: Roughness
    groundPlaneBindings[7].texture = m_DefaultWhiteTexture;   // binding 7: AO
    groundPlaneBindings[11].texture = m_DefaultBlackTexture;  // binding 11: Emissive (black = no glow)

    m_GroundPlaneDescriptorSet = std::make_unique<rhi::VulkanDescriptorSet>(
        std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->GetContext(),
        groundPlaneBindings
    );

    // Create shadow descriptor set (for shadow pass rendering)
    std::vector<rhi::DescriptorBinding> shadowBindings = {
        {
            0,  // binding = 0 (Shadow UBO with light space matrix)
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT,
            m_ShadowUniformBuffer,
            nullptr,  // texture
            nullptr   // sampler
        }
    };

    m_ShadowDescriptorSet = std::make_unique<rhi::VulkanDescriptorSet>(
        std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->GetContext(),
        shadowBindings
    );

    // Create skybox descriptor set with 2 bindings
    std::vector<rhi::DescriptorBinding> skyboxBindings = {
        {
            0,  // binding = 0 (MVP matrices - shared with main renderer)
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT,
            m_UniformBuffers[0],
            nullptr,  // texture
            nullptr   // sampler
        },
        {
            1,  // binding = 1 (Environment cubemap)
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr,  // buffer
            m_EnvironmentMap,
            m_CubemapSampler
        }
    };

    m_SkyboxDescriptorSet = std::make_unique<rhi::VulkanDescriptorSet>(
        std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->GetContext(),
        skyboxBindings
    );

    // Set descriptor set layout on device before creating pipeline
    std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->SetDescriptorSetLayout(
        m_DescriptorSet->GetLayout()
    );

    // Create triangle resources
    CreateTriangle();

    // Create model pipeline
    CreateModelPipeline();

    // Create skybox pipeline with skybox descriptor set layout
    std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->SetDescriptorSetLayout(
        m_SkyboxDescriptorSet->GetLayout()
    );
    CreateSkyboxPipeline();

    // Create shadow pipeline with shadow descriptor set layout
    std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->SetDescriptorSetLayout(
        m_ShadowDescriptorSet->GetLayout()
    );
    CreateShadowPipeline();

    // Restore main descriptor set layout
    std::static_pointer_cast<rhi::VulkanDevice>(m_Device)->SetDescriptorSetLayout(
        m_DescriptorSet->GetLayout()
    );

    // Create skybox cube geometry
    CreateSkyboxCube();

    // Initialize available models list
    m_AvailableModels = {
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/AntiqueCamera.glb",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/bunny_tex_coords.obj",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/DamagedHelmet.glb",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/MetalRoughSpheres.glb"
    };
    m_CurrentModelIndex = 2;  

    // Load initial model
    LoadModel(m_AvailableModels[m_CurrentModelIndex]);

    // Create ground plane for shadow visualization
    CreateGroundPlane();

    METAGFX_INFO << "Controls:";
    METAGFX_INFO << "  WASD/QE - Camera movement";
    METAGFX_INFO << "  Mouse drag - Rotate camera";
    METAGFX_INFO << "  1-4 - Load specific model";
    METAGFX_INFO << "  N - Next model";
    METAGFX_INFO << "  P - Previous model";
    METAGFX_INFO << "  ESC - Exit";

    // Initialize ImGui
    InitImGui();

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

    // Automatically frame camera to view the entire model
    glm::vec3 center = m_Model->GetCenter();
    glm::vec3 size = m_Model->GetSize();
    float radius = m_Model->GetBoundingSphereRadius();

    METAGFX_INFO << "Model bounds - Center: (" << center.x << ", " << center.y << ", " << center.z << ")";
    METAGFX_INFO << "Model bounds - Size: (" << size.x << ", " << size.y << ", " << size.z << ")";
    METAGFX_INFO << "Model bounds - Bounding sphere radius: " << radius;

    // Frame the camera to view the model with 30% margin
    m_Camera->FrameBoundingBox(center, size, 1.3f);

    // Update ground plane position based on model bounds
    UpdateGroundPlanePosition();

    METAGFX_INFO << "Camera framed at position: ("
                 << m_Camera->GetPosition().x << ", "
                 << m_Camera->GetPosition().y << ", "
                 << m_Camera->GetPosition().z << ")";
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
    // Modified to cast more obvious shadows - light comes from above-left-front
    // This is the shadow-casting light, direction controlled by m_LightDirection
    auto keyLight = std::make_unique<DirectionalLight>(
        m_LightDirection,                  // Direction: controlled via UI
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

void Application::CreateGroundPlane() {
    // Ground plane will be created/updated dynamically when a model is loaded
    // See UpdateGroundPlanePosition()
}

void Application::UpdateGroundPlanePosition() {
    using namespace rhi;

    if (!m_Model || !m_Model->IsValid()) {
        return;
    }

    // Get model bounding box
    glm::vec3 minBounds, maxBounds;
    if (!m_Model->GetBoundingBox(minBounds, maxBounds)) {
        return;
    }

    // Position ground plane below the model's lowest point
    // Add offset to ensure it's clearly below and avoid shadow acne
    float modelHeight = maxBounds.y - minBounds.y;
    float offset = glm::max(modelHeight * 0.3f, 0.5f);  // 30% of model height OR minimum 0.5 units
    float groundY = minBounds.y - offset;

    METAGFX_INFO << "Model Y bounds: min=" << minBounds.y << ", max=" << maxBounds.y
                 << ", ground plane Y=" << groundY;

    // Make ground plane large enough to show shadows
    float planeSize = glm::max(maxBounds.x - minBounds.x, maxBounds.z - minBounds.z) * 2.0f;
    planeSize = glm::max(planeSize, 15.0f);  // Minimum 15 units

    // Create a simple quad (two triangles) for the ground plane
    std::vector<Vertex> vertices = {
        // Position                                    Normal              TexCoord
        { { -planeSize, groundY, -planeSize }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
        { {  planeSize, groundY, -planeSize }, { 0.0f, 1.0f, 0.0f }, { 10.0f, 0.0f } },
        { {  planeSize, groundY,  planeSize }, { 0.0f, 1.0f, 0.0f }, { 10.0f, 10.0f } },
        { { -planeSize, groundY,  planeSize }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 10.0f } }
    };

    std::vector<uint32> indices = {
        0, 2, 1,  // First triangle (clockwise from above = front face visible from below/side)
        2, 0, 3   // Second triangle (clockwise from above = front face visible from below/side)
    };

    // Log vertex coordinates for debugging
    METAGFX_INFO << "Ground plane vertices:";
    METAGFX_INFO << "  Vertex 0: (" << vertices[0].position.x << ", " << vertices[0].position.y << ", " << vertices[0].position.z << ")";
    METAGFX_INFO << "  Vertex 1: (" << vertices[1].position.x << ", " << vertices[1].position.y << ", " << vertices[1].position.z << ")";
    METAGFX_INFO << "  Vertex 2: (" << vertices[2].position.x << ", " << vertices[2].position.y << ", " << vertices[2].position.z << ")";
    METAGFX_INFO << "  Vertex 3: (" << vertices[3].position.x << ", " << vertices[3].position.y << ", " << vertices[3].position.z << ")";
    METAGFX_INFO << "  Indices: [" << indices[0] << "," << indices[1] << "," << indices[2] << "], [" << indices[3] << "," << indices[4] << "," << indices[5] << "]";

    // Recreate ground plane
    if (m_GroundPlane) {
        m_GroundPlane->Cleanup();
    }
    m_GroundPlane = std::make_unique<Model>();
    auto mesh = std::make_unique<Mesh>();
    if (mesh->Initialize(m_Device.get(), vertices, indices)) {
        m_GroundPlane->AddMesh(std::move(mesh));
        METAGFX_INFO << "Ground plane positioned at Y=" << groundY
                     << ", size=" << (planeSize * 2.0f) << "x" << (planeSize * 2.0f);
    } else {
        METAGFX_ERROR << "Failed to initialize ground plane mesh";
    }
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
    
    rhi::ShaderDesc fragShaderDesc{};
    fragShaderDesc.stage = rhi::ShaderStage::Fragment;
    fragShaderDesc.code = fragShaderCode;
    fragShaderDesc.entryPoint = "main";

    auto fragShader = m_Device->CreateShader(fragShaderDesc);

    // Create pipeline
    rhi::PipelineDesc pipelineDesc{};
    pipelineDesc.vertexShader = vertShader;
    pipelineDesc.fragmentShader = fragShader;

    // Vertex input: position and color
    pipelineDesc.vertexInput.stride = sizeof(float) * 6;
    pipelineDesc.vertexInput.attributes = {
        { 0, rhi::Format::R32G32B32_SFLOAT, 0 },                // position at location 0
        { 1, rhi::Format::R32G32B32_SFLOAT, sizeof(float) * 3 } // color at location 1
    };

    pipelineDesc.topology = rhi::PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = rhi::CullMode::None;
    
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

    rhi::ShaderDesc fragShaderDesc{};
    fragShaderDesc.stage = rhi::ShaderStage::Fragment;
    fragShaderDesc.code = fragShaderCode;
    fragShaderDesc.entryPoint = "main";

    auto fragShader = m_Device->CreateShader(fragShaderDesc);

    // Create pipeline for models
    rhi::PipelineDesc pipelineDesc{};
    pipelineDesc.vertexShader = vertShader;
    pipelineDesc.fragmentShader = fragShader;

    // Vertex input: position (vec3), normal (vec3), texcoord (vec2)
    pipelineDesc.vertexInput.stride = sizeof(Vertex);
    pipelineDesc.vertexInput.attributes = {
        { 0, rhi::Format::R32G32B32_SFLOAT, 0 },                          // position at location 0
        { 1, rhi::Format::R32G32B32_SFLOAT, sizeof(float) * 3 },          // normal at location 1
        { 2, rhi::Format::R32G32_SFLOAT, sizeof(float) * 6 }              // texcoord at location 2
    };

    pipelineDesc.topology = rhi::PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = rhi::CullMode::Back;
    pipelineDesc.rasterization.frontFace = rhi::FrontFace::CounterClockwise;  // glTF uses CCW winding order

    // Enable depth testing for proper 3D rendering
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = true;

    m_ModelPipeline = m_Device->CreateGraphicsPipeline(pipelineDesc);

    METAGFX_INFO << "Model pipeline created";
}

void Application::CreateSkyboxPipeline() {
    using namespace rhi;

    // Load skybox shaders (SPIR-V bytecode)
    std::vector<uint8> vertShaderCode = {
        #include "skybox.vert.spv.inl"
    };

    ShaderDesc vertShaderDesc{};
    vertShaderDesc.stage = ShaderStage::Vertex;
    vertShaderDesc.code = vertShaderCode;
    vertShaderDesc.entryPoint = "main";

    auto vertShader = m_Device->CreateShader(vertShaderDesc);

    std::vector<uint8> fragShaderCode = {
        #include "skybox.frag.spv.inl"
    };

    ShaderDesc fragShaderDesc{};
    fragShaderDesc.stage = ShaderStage::Fragment;
    fragShaderDesc.code = fragShaderCode;
    fragShaderDesc.entryPoint = "main";

    auto fragShader = m_Device->CreateShader(fragShaderDesc);

    // Create pipeline for skybox
    PipelineDesc pipelineDesc{};
    pipelineDesc.vertexShader = vertShader;
    pipelineDesc.fragmentShader = fragShader;

    // Vertex input: position (vec3) only - used as cubemap direction
    pipelineDesc.vertexInput.stride = sizeof(Vertex);
    pipelineDesc.vertexInput.attributes = {
        { 0, Format::R32G32B32_SFLOAT, 0 }  // position at location 0
    };

    pipelineDesc.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = CullMode::None;  // No culling for debugging
    pipelineDesc.rasterization.frontFace = FrontFace::CounterClockwise;

    // Depth testing: LessOrEqual without writing depth (render where depth == 1.0 cleared value)
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = false;  // Don't write depth
    pipelineDesc.depthStencil.depthCompareOp = CompareOp::LessOrEqual;

    m_SkyboxPipeline = m_Device->CreateGraphicsPipeline(pipelineDesc);

    METAGFX_INFO << "Skybox pipeline created";
}

void Application::CreateShadowPipeline() {
    using namespace rhi;

    // Load shadow map shaders (SPIR-V bytecode)
    std::vector<uint8> vertShaderCode = {
        #include "shadowmap.vert.spv.inl"
    };

    ShaderDesc vertShaderDesc{};
    vertShaderDesc.stage = ShaderStage::Vertex;
    vertShaderDesc.code = vertShaderCode;
    vertShaderDesc.entryPoint = "main";

    auto vertShader = m_Device->CreateShader(vertShaderDesc);

    std::vector<uint8> fragShaderCode = {
        #include "shadowmap.frag.spv.inl"
    };

    ShaderDesc fragShaderDesc{};
    fragShaderDesc.stage = ShaderStage::Fragment;
    fragShaderDesc.code = fragShaderCode;
    fragShaderDesc.entryPoint = "main";

    auto fragShader = m_Device->CreateShader(fragShaderDesc);

    // Create pipeline for shadow map rendering (depth-only)
    PipelineDesc pipelineDesc{};
    pipelineDesc.vertexShader = vertShader;
    pipelineDesc.fragmentShader = fragShader;

    // Vertex input: position (vec3) only
    pipelineDesc.vertexInput.stride = sizeof(Vertex);
    pipelineDesc.vertexInput.attributes = {
        { 0, Format::R32G32B32_SFLOAT, 0 }  // position at location 0
    };

    pipelineDesc.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = CullMode::Back;
    pipelineDesc.rasterization.frontFace = FrontFace::CounterClockwise;
    pipelineDesc.rasterization.depthBiasEnable = true;  // Enable depth bias to reduce shadow acne
    pipelineDesc.rasterization.depthBiasConstantFactor = 1.25f;
    pipelineDesc.rasterization.depthBiasSlopeFactor = 1.75f;

    // Enable depth testing and writing for shadow map
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = true;
    pipelineDesc.depthStencil.depthCompareOp = CompareOp::Less;  // Standard: closer fragments win

    m_ShadowPipeline = m_Device->CreateGraphicsPipeline(pipelineDesc);

    METAGFX_INFO << "Shadow pipeline created";
}

void Application::CreateSkyboxCube() {
    using namespace rhi;

    // Skybox cube vertices (positions only)
    // We use a unit cube centered at origin
    Vertex vertices[] = {
        // Back face
        {{-1.0f, -1.0f, -1.0f}, {}, {}},
        {{ 1.0f, -1.0f, -1.0f}, {}, {}},
        {{ 1.0f,  1.0f, -1.0f}, {}, {}},
        {{-1.0f,  1.0f, -1.0f}, {}, {}},
        // Front face
        {{-1.0f, -1.0f,  1.0f}, {}, {}},
        {{ 1.0f, -1.0f,  1.0f}, {}, {}},
        {{ 1.0f,  1.0f,  1.0f}, {}, {}},
        {{-1.0f,  1.0f,  1.0f}, {}, {}},
    };

    // Cube indices (36 indices for 12 triangles, 6 faces)
    uint32_t indices[] = {
        // Back face
        0, 1, 2, 2, 3, 0,
        // Front face
        4, 6, 5, 6, 4, 7,
        // Left face
        4, 0, 3, 3, 7, 4,
        // Right face
        1, 5, 6, 6, 2, 1,
        // Bottom face
        4, 5, 1, 1, 0, 4,
        // Top face
        3, 2, 6, 6, 7, 3
    };

    // Create vertex buffer
    BufferDesc vertexBufferDesc{};
    vertexBufferDesc.size = sizeof(vertices);
    vertexBufferDesc.usage = BufferUsage::Vertex;
    vertexBufferDesc.memoryUsage = MemoryUsage::CPUToGPU;

    m_SkyboxVertexBuffer = m_Device->CreateBuffer(vertexBufferDesc);
    m_SkyboxVertexBuffer->CopyData(vertices, sizeof(vertices));

    // Create index buffer
    BufferDesc indexBufferDesc{};
    indexBufferDesc.size = sizeof(indices);
    indexBufferDesc.usage = BufferUsage::Index;
    indexBufferDesc.memoryUsage = MemoryUsage::CPUToGPU;

    m_SkyboxIndexBuffer = m_Device->CreateBuffer(indexBufferDesc);
    m_SkyboxIndexBuffer->CopyData(indices, sizeof(indices));

    METAGFX_INFO << "Skybox cube created (8 vertices, 36 indices)";
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
        // Let ImGui handle events first
        ImGui_ImplSDL3_ProcessEvent(&event);

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
                    // METAGFX_INFO << "Mouse button pressed - drag enabled";
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    m_MouseButtonPressed = false;
                    // METAGFX_INFO << "Mouse button released";
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
                    // Destroy old ImGui framebuffers before resizing swap chain
                    auto vkDevice = std::static_pointer_cast<rhi::VulkanDevice>(m_Device);
                    auto& context = vkDevice->GetContext();
                    for (auto framebuffer : m_ImGuiFramebuffers) {
                        if (framebuffer != VK_NULL_HANDLE) {
                            vkDestroyFramebuffer(context.device, framebuffer, nullptr);
                        }
                    }
                    m_ImGuiFramebuffers.clear();

                    m_Device->GetSwapChain()->Resize(event.window.data1, event.window.data2);
                    m_Camera->SetAspectRatio(
                        static_cast<float>(event.window.data1) /
                        static_cast<float>(event.window.data2)
                    );

                    // Recreate depth buffer with new dimensions
                    m_DepthBuffer.reset();
                    rhi::TextureDesc depthDesc{};
                    depthDesc.width = event.window.data1;
                    depthDesc.height = event.window.data2;
                    depthDesc.format = rhi::Format::D32_SFLOAT;
                    depthDesc.usage = rhi::TextureUsage::DepthStencilAttachment;
                    depthDesc.debugName = "DepthBuffer";
                    m_DepthBuffer = m_Device->CreateTexture(depthDesc);
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

    // Process pending model load (deferred from ImGui UI)
    if (m_HasPendingModel) {
        m_HasPendingModel = false;

        // Queue old model for deletion after 2 frames
        if (m_Model) {
            m_DeletionQueue.push_back({std::move(m_Model), 2});
        }

        // Load new model
        LoadModel(m_PendingModelPath);
    }

    // Process deletion queue
    for (auto it = m_DeletionQueue.begin(); it != m_DeletionQueue.end(); ) {
        it->frameCount--;
        if (it->frameCount == 0) {
            it = m_DeletionQueue.erase(it);
        } else {
            ++it;
        }
    }

    auto swapChain = m_Device->GetSwapChain();
    auto backBuffer = swapChain->GetCurrentBackBuffer();

    // Update uniform buffer
    UniformBufferObject ubo{};
    // Model matrix: identity (no transformation)
    glm::mat4 modelMatrix = glm::mat4(1.0f);
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

    // CRITICAL: Add buffer memory barrier to ensure light buffer writes are visible to GPU
    // This barrier ensures the CPU-side light buffer writes complete before fragment shader reads
    auto lightBuffer = m_Scene->GetLightBuffer();
    if (lightBuffer) {
        auto vkLightBuffer = std::static_pointer_cast<VulkanBuffer>(lightBuffer);
        vkCmd->BufferMemoryBarrier(
            vkLightBuffer->GetHandle(),
            0,  // offset
            vkLightBuffer->GetSize(),  // size
            VK_PIPELINE_STAGE_HOST_BIT,  // srcStage: CPU writes
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  // dstStage: Fragment shader reads
            VK_ACCESS_HOST_WRITE_BIT,  // srcAccess: CPU writes
            VK_ACCESS_UNIFORM_READ_BIT  // dstAccess: Shader uniform reads
        );
    }

    // =============================================================================
    // Shadow Pass: Render scene from light's perspective to shadow map
    // =============================================================================

    // Debug: Log shadow pass conditions
    static bool loggedConditions = false;
    if (!loggedConditions) {
        METAGFX_INFO << "Shadow pass conditions: EnableShadows=" << m_EnableShadows
                     << ", ShadowMap=" << (m_ShadowMap ? "valid" : "null")
                     << ", Model=" << (m_Model ? "valid" : "null")
                     << ", ModelIsValid=" << (m_Model && m_Model->IsValid() ? "true" : "false");
        loggedConditions = true;
    }

    if (m_EnableShadows && m_ShadowMap && m_Model && m_Model->IsValid()) {
        // Debug: Log shadow pass execution
        static bool loggedShadowPass = false;
        if (!loggedShadowPass) {
            METAGFX_INFO << "Executing shadow pass - rendering " << m_Model->GetMeshes().size() << " meshes";
            loggedShadowPass = true;
        }

        // Get the first directional light for shadow casting
        DirectionalLight* shadowLight = nullptr;
        for (const auto& light : m_Scene->GetLights()) {
            if (auto* dirLight = dynamic_cast<DirectionalLight*>(light.get())) {
                shadowLight = dirLight;
                break;
            }
        }

        if (shadowLight) {
            // Update the light direction from UI control
            shadowLight->SetDirection(m_LightDirection);

            // Update shadow map light matrix
            m_ShadowMap->UpdateLightMatrix(shadowLight->GetDirection(), *m_Camera);

            // Update shadow uniform buffer
            struct ShadowUBO {
                glm::mat4 lightSpaceMatrix;
                glm::mat4 model;  // Model matrix (identity for now)
                float shadowBias;
                float padding[3];
            };
            ShadowUBO shadowUBO{};
            shadowUBO.lightSpaceMatrix = m_ShadowMap->GetLightSpaceMatrix();
            shadowUBO.model = glm::mat4(1.0f);  // Identity matrix
            shadowUBO.shadowBias = m_ShadowBias;

            // Debug: Log light space matrix (ALL rows)
            static bool loggedMatrix = false;
            if (!loggedMatrix) {
                const auto& m = shadowUBO.lightSpaceMatrix;
                METAGFX_INFO << "LightSpaceMatrix row 0: (" << m[0][0] << ", " << m[1][0] << ", " << m[2][0] << ", " << m[3][0] << ")";
                METAGFX_INFO << "LightSpaceMatrix row 1: (" << m[0][1] << ", " << m[1][1] << ", " << m[2][1] << ", " << m[3][1] << ")";
                METAGFX_INFO << "LightSpaceMatrix row 2: (" << m[0][2] << ", " << m[1][2] << ", " << m[2][2] << ", " << m[3][2] << ")";
                METAGFX_INFO << "LightSpaceMatrix row 3: (" << m[0][3] << ", " << m[1][3] << ", " << m[2][3] << ", " << m[3][3] << ")";
                loggedMatrix = true;
            }

            m_ShadowUniformBuffer->CopyData(&shadowUBO, sizeof(shadowUBO));

            // Begin shadow rendering pass (depth-only)
            // Note: BeginRendering will handle layout transitions via the render pass
            ClearValue shadowDepthClear{};
            shadowDepthClear.depthStencil.depth = 1.0f;  // Standard: far plane
            shadowDepthClear.depthStencil.stencil = 0;

            cmd->BeginRendering({}, m_ShadowMap->GetDepthTexture(), { shadowDepthClear });

            // Set viewport and scissor for shadow map
            Viewport shadowViewport{};
            shadowViewport.width = static_cast<float>(m_ShadowMap->GetWidth());
            shadowViewport.height = static_cast<float>(m_ShadowMap->GetHeight());
            shadowViewport.minDepth = 0.0f;
            shadowViewport.maxDepth = 1.0f;
            cmd->SetViewport(shadowViewport);

            Rect2D shadowScissor{};
            shadowScissor.width = m_ShadowMap->GetWidth();
            shadowScissor.height = m_ShadowMap->GetHeight();
            cmd->SetScissor(shadowScissor);

            // Bind shadow pipeline
            cmd->BindPipeline(m_ShadowPipeline);

            // Bind shadow descriptor set
            auto vkShadowPipeline = std::static_pointer_cast<VulkanPipeline>(m_ShadowPipeline);
            vkCmd->BindDescriptorSet(vkShadowPipeline->GetLayout(),
                                     m_ShadowDescriptorSet->GetSet(0));

            // Render all meshes from light's perspective
            uint32 meshesRendered = 0;
            for (const auto& mesh : m_Model->GetMeshes()) {
                if (mesh && mesh->IsValid()) {
                    // Draw mesh (model matrix is in the uniform buffer)
                    cmd->BindVertexBuffer(mesh->GetVertexBuffer());
                    cmd->BindIndexBuffer(mesh->GetIndexBuffer());
                    cmd->DrawIndexed(mesh->GetIndexCount());
                    meshesRendered++;

                    // Debug: Log draw call details
                    static bool loggedDrawCall = false;
                    if (!loggedDrawCall) {
                        METAGFX_INFO << "Shadow pass draw call: " << mesh->GetIndexCount()
                                     << " indices, vertex buffer valid: " << (mesh->GetVertexBuffer() ? "yes" : "no")
                                     << ", index buffer valid: " << (mesh->GetIndexBuffer() ? "yes" : "no");
                        loggedDrawCall = true;
                    }
                }
            }

            // NOTE: Do NOT render ground plane in shadow pass!
            // The ground plane should RECEIVE shadows, not CAST them.
            // Rendering it here would write its depth to the shadow map and interfere
            // with shadow calculations.

            // Debug: Log model info once per model load (using frame counter to rate-limit)
            static int lastLoggedFrame = -100;
            static int frameCounter = 0;
            frameCounter++;

            if (frameCounter - lastLoggedFrame > 60) {  // Log every 60 frames (once per second at 60fps)
                METAGFX_INFO << "Shadow pass rendered " << meshesRendered << " meshes";

                // Log model bounding box to verify it's within shadow frustum
                glm::vec3 minBounds, maxBounds;
                if (m_Model->GetBoundingBox(minBounds, maxBounds)) {
                    METAGFX_INFO << "Model bounds: min(" << minBounds.x << ", " << minBounds.y
                                 << ", " << minBounds.z << "), max(" << maxBounds.x << ", "
                                 << maxBounds.y << ", " << maxBounds.z << ")";
                    glm::vec3 center = (minBounds + maxBounds) * 0.5f;
                    glm::vec3 size = maxBounds - minBounds;
                    METAGFX_INFO << "Model center: (" << center.x << ", " << center.y << ", "
                                 << center.z << "), size: (" << size.x << ", " << size.y
                                 << ", " << size.z << ")";
                }

                lastLoggedFrame = frameCounter;
            }

            cmd->EndRendering();

            // Add pipeline barrier to ensure shadow map writes complete before sampling
            auto vkShadowTexture = std::static_pointer_cast<VulkanTexture>(m_ShadowMap->GetDepthTexture());
            VkImageMemoryBarrier shadowBarrier{};
            shadowBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            shadowBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            shadowBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            shadowBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            shadowBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            shadowBarrier.image = vkShadowTexture->GetImage();
            shadowBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            shadowBarrier.subresourceRange.baseMipLevel = 0;
            shadowBarrier.subresourceRange.levelCount = 1;
            shadowBarrier.subresourceRange.baseArrayLayer = 0;
            shadowBarrier.subresourceRange.layerCount = 1;
            shadowBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            shadowBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(vkCmd->GetHandle(),
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0, 0, nullptr, 0, nullptr, 1, &shadowBarrier);
        }
    }

    // =============================================================================
    // Main Pass: Render scene with shadow sampling
    // =============================================================================

    // Begin rendering
    ClearValue colorClear{};
    colorClear.color[0] = 0.1f;
    colorClear.color[1] = 0.1f;
    colorClear.color[2] = 0.15f;
    colorClear.color[3] = 1.0f;

    ClearValue depthClear{};
    depthClear.depthStencil.depth = 1.0f;
    depthClear.depthStencil.stencil = 0;

    cmd->BeginRendering({ backBuffer }, m_DepthBuffer, { colorClear, depthClear });
    
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

    // Draw the model FIRST
    if (m_Model && m_Model->IsValid()) {
        // Bind model pipeline
        cmd->BindPipeline(m_ModelPipeline);

        // Bind descriptor set (use current frame index for double buffering)
        auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(m_ModelPipeline);
        vkCmd->BindDescriptorSet(vkPipeline->GetLayout(),
                                 m_DescriptorSet->GetSet(m_CurrentFrame));

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

                // Binding 11: Emissive map
                Ref<rhi::Texture> emissiveMap = material->GetEmissiveMap();
                if (emissiveMap) {
                    m_DescriptorSet->UpdateTexture(11, emissiveMap, m_LinearRepeatSampler);
                } else {
                    m_DescriptorSet->UpdateTexture(11, m_DefaultBlackTexture, m_LinearRepeatSampler);
                }

                // Re-bind descriptor set after texture updates
                vkCmd->BindDescriptorSet(vkPipeline->GetLayout(),
                                        m_DescriptorSet->GetSet(m_CurrentFrame));

                // Push material flags and exposure (offset 16 bytes after cameraPosition vec4)
                uint32_t flags = material->GetTextureFlags();

                // Debug: Log flags on first frame
                static bool loggedOnce = false;
                if (!loggedOnce) {
                    METAGFX_INFO << "Material texture flags: 0x" << std::hex << flags << std::dec
                                 << " (HasAlbedo=" << ((flags & 0x1) != 0)
                                 << ", HasNormal=" << ((flags & 0x2) != 0)
                                 << ", HasMetallic=" << ((flags & 0x4) != 0)
                                 << ", HasRoughness=" << ((flags & 0x8) != 0)
                                 << ", HasMetallicRoughness=" << ((flags & 0x10) != 0)
                                 << ", HasAO=" << ((flags & 0x20) != 0)
                                 << ", HasEmissive=" << ((flags & 0x40) != 0) << ")";
                    loggedOnce = true;
                }

                // Push flags (offset 16, size 4)
                vkCmd->PushConstants(vkPipeline->GetLayout(),
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    16, sizeof(uint32_t), &flags);

                // Push exposure (offset 20, size 4)
                vkCmd->PushConstants(vkPipeline->GetLayout(),
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    20, sizeof(float), &m_Exposure);

                // Push IBL enable flag (offset 24, size 4)
                uint32_t enableIBL = m_EnableIBL ? 1u : 0u;
                vkCmd->PushConstants(vkPipeline->GetLayout(),
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    24, sizeof(uint32_t), &enableIBL);

                // Push IBL intensity (offset 28, size 4)
                vkCmd->PushConstants(vkPipeline->GetLayout(),
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    28, sizeof(float), &m_IBLIntensity);

                // Push shadow debug mode (offset 32, size 4)
                uint32_t shadowDebugMode = static_cast<uint32_t>(m_ShadowDebugMode);

                // Debug: Log shadow debug mode on first frame
                static bool loggedDebugMode = false;
                if (!loggedDebugMode) {
                    METAGFX_INFO << "Shadow debug mode being pushed to shader: " << shadowDebugMode;
                    loggedDebugMode = true;
                }

                vkCmd->PushConstants(vkPipeline->GetLayout(),
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    32, sizeof(uint32_t), &shadowDebugMode);

                // Push shadow enable flag (offset 36, size 4)
                uint32_t enableShadows = m_EnableShadows ? 1u : 0u;

                // Debug: Log shadow enable state on first frame
                static bool loggedShadowState = false;
                if (!loggedShadowState) {
                    METAGFX_INFO << "Shadow enable flag being pushed to shader: " << enableShadows;
                    loggedShadowState = true;
                }

                vkCmd->PushConstants(vkPipeline->GetLayout(),
                                    VK_SHADER_STAGE_FRAGMENT_BIT,
                                    36, sizeof(uint32_t), &enableShadows);

                // Bind and draw
                cmd->BindVertexBuffer(mesh->GetVertexBuffer());
                cmd->BindIndexBuffer(mesh->GetIndexBuffer());
                cmd->DrawIndexed(mesh->GetIndexCount());
            }
        }
    }

    // Render ground plane with simple grey material (if enabled)
    if (m_ShowGroundPlane && m_GroundPlane && m_GroundPlane->IsValid()) {
        // Create a simple grey material for the ground
        // Use darker grey to make shadows more visible
        MaterialProperties groundMat{};
        groundMat.albedo = glm::vec3(0.35f, 0.35f, 0.35f);  // Dark grey (reduced from 0.5)
        groundMat.roughness = 0.9f;  // Very rough (increased from 0.8)
        groundMat.metallic = 0.0f;   // Not metallic
        groundMat.emissiveFactor = glm::vec3(0.0f);
        m_GroundPlaneMaterialBuffer->CopyData(&groundMat, sizeof(groundMat));

        // DO NOT call UpdateTexture here! The ground plane descriptor set was initialized
        // with default textures during setup, and calling UpdateTexture during rendering
        // causes issues. Just bind the pre-configured descriptor set.

        // Bind ground plane's dedicated descriptor set (use current frame for double buffering)
        auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
        auto vkPipeline = std::static_pointer_cast<VulkanPipeline>(m_ModelPipeline);
        vkCmd->BindDescriptorSet(vkPipeline->GetLayout(), m_GroundPlaneDescriptorSet->GetSet(m_CurrentFrame));

        // Push material flags (no textures)
        uint32_t flags = 0;
        vkCmd->PushConstants(vkPipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 16, sizeof(uint32_t), &flags);

        // Draw ground plane
        for (const auto& mesh : m_GroundPlane->GetMeshes()) {
            if (mesh && mesh->IsValid()) {
                cmd->BindVertexBuffer(mesh->GetVertexBuffer());
                cmd->BindIndexBuffer(mesh->GetIndexBuffer());
                cmd->DrawIndexed(mesh->GetIndexCount());
            }
        }
    }

    // Render skybox LAST (only where depth >= model depth)
    if (m_ShowSkybox && m_EnvironmentMap && m_SkyboxPipeline && m_SkyboxVertexBuffer && m_SkyboxIndexBuffer && m_SkyboxDescriptorSet) {
        cmd->BindPipeline(m_SkyboxPipeline);

        // Get Vulkan-specific objects
        auto vkSkyboxPipeline = std::static_pointer_cast<VulkanPipeline>(m_SkyboxPipeline);

        // Update skybox descriptor set with uniform buffer (always use buffer[0])
        // FIXME: Matches main renderer which always updates buffer[0]
        m_SkyboxDescriptorSet->UpdateBuffer(0, m_UniformBuffers[0]);

        // Bind skybox descriptor set (binding 0: MVP, binding 1: environment cubemap)
        vkCmd->BindDescriptorSet(vkSkyboxPipeline->GetLayout(),
                                 m_SkyboxDescriptorSet->GetSet(m_CurrentFrame));

        // Push constants: exposure and LOD
        struct SkyboxPushConstants {
            float exposure;
            float lod;
        } skyboxPushConstants;

        skyboxPushConstants.exposure = m_Exposure;
        skyboxPushConstants.lod = m_SkyboxLOD;

        vkCmd->PushConstants(vkSkyboxPipeline->GetLayout(),
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(SkyboxPushConstants), &skyboxPushConstants);

        // Draw the skybox cube
        cmd->BindVertexBuffer(m_SkyboxVertexBuffer);
        cmd->BindIndexBuffer(m_SkyboxIndexBuffer);
        cmd->DrawIndexed(36);  // 36 indices for the cube
    }

    cmd->EndRendering();

    // Render ImGui overlay into the same command buffer
    RenderImGui(cmd, backBuffer);

    cmd->End();

    // Submit command buffer (contains both main rendering and ImGui)
    m_Device->SubmitCommandBuffer(cmd);

    // Present
    swapChain->Present();
    
    // Advance frame
    m_CurrentFrame = (m_CurrentFrame + 1) % 2;
}

void Application::Shutdown() {
    if (m_Device) {
        m_Device->WaitIdle();
    }

    // Shutdown ImGui
    ShutdownImGui();

    // Clean up scene and model
    m_Scene.reset();
    if (m_Model) {
        m_Model->Cleanup();
        m_Model.reset();
    }
    if (m_GroundPlane) {
        m_GroundPlane->Cleanup();
        m_GroundPlane.reset();
    }

    // Clean up pipelines
    m_ModelPipeline.reset();
    m_SkyboxPipeline.reset();
    m_ShadowPipeline.reset();
    m_Pipeline.reset();

    // Clean up buffers
    m_VertexBuffer.reset();
    m_SkyboxVertexBuffer.reset();
    m_SkyboxIndexBuffer.reset();
    m_UniformBuffers[0].reset();
    m_UniformBuffers[1].reset();
    m_MaterialBuffers[0].reset();
    m_MaterialBuffers[1].reset();
    m_GroundPlaneMaterialBuffer.reset();
    m_ShadowUniformBuffer.reset();

    // Clean up descriptor sets
    m_DescriptorSet.reset();
    m_SkyboxDescriptorSet.reset();
    m_ShadowDescriptorSet.reset();
    m_GroundPlaneDescriptorSet.reset();

    // Clean up textures (must be before device destruction)
    m_DefaultTexture.reset();
    m_DefaultNormalMap.reset();
    m_DefaultWhiteTexture.reset();
    m_DefaultBlackTexture.reset();
    m_DepthBuffer.reset();

    // Clean up IBL textures
    m_IrradianceMap.reset();
    m_PrefilteredMap.reset();
    m_BRDF_LUT.reset();
    m_EnvironmentMap.reset();

    // Clean up samplers
    m_LinearRepeatSampler.reset();
    m_CubemapSampler.reset();

    // Clean up shadow map
    m_ShadowMap.reset();

    // Finally destroy the device
    m_Device.reset();
    
    if (m_Window) {
        METAGFX_INFO << "Destroying window...";
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }

    METAGFX_INFO << "Shutting down SDL...";
    SDL_Quit();
}

void Application::InitImGui() {
    auto vkDevice = std::static_pointer_cast<rhi::VulkanDevice>(m_Device);
    auto& context = vkDevice->GetContext();
    auto swapChain = m_Device->GetSwapChain();

    // Create descriptor pool for ImGui
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = 11;
    poolInfo.pPoolSizes = poolSizes;

    vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &m_ImGuiDescriptorPool);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style
    ImGui::StyleColorsDark();

    // Create ImGui render pass
    VkAttachmentDescription attachment{};
    attachment.format = VK_FORMAT_B8G8R8A8_SRGB;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // Load existing content
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    vkCreateRenderPass(context.device, &renderPassInfo, nullptr, &m_ImGuiRenderPass);

    // Initialize ImGui backends
    ImGui_ImplSDL3_InitForVulkan(m_Window);

    // Get the actual swap chain image count by querying Vulkan
    auto vkSwapChain = std::static_pointer_cast<rhi::VulkanSwapChain>(swapChain);
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(context.device, vkSwapChain->GetHandle(), &imageCount, nullptr);
    METAGFX_INFO << "InitImGui: Swap chain has " << imageCount << " images";

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = context.instance;
    initInfo.PhysicalDevice = context.physicalDevice;
    initInfo.Device = context.device;
    initInfo.QueueFamily = context.graphicsQueueFamily;
    initInfo.Queue = context.graphicsQueue;
    initInfo.DescriptorPool = m_ImGuiDescriptorPool;
    initInfo.RenderPass = m_ImGuiRenderPass;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    // Upload fonts
    ImGui_ImplVulkan_CreateFontsTexture();

    // Wait for font upload to complete
    vkDeviceWaitIdle(context.device);

    // Initialize framebuffers vector (created lazily during rendering)
    // Need to match the swap chain image count (typically 2 or 3)
    m_ImGuiFramebuffers.resize(imageCount, VK_NULL_HANDLE);
}

void Application::ShutdownImGui() {
    if (!m_Device) return;

    auto vkDevice = std::static_pointer_cast<rhi::VulkanDevice>(m_Device);
    auto& context = vkDevice->GetContext();

    // Destroy ImGui framebuffers
    for (auto framebuffer : m_ImGuiFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context.device, framebuffer, nullptr);
        }
    }
    m_ImGuiFramebuffers.clear();

    // Shutdown ImGui backends
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    // Destroy ImGui resources
    if (m_ImGuiRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context.device, m_ImGuiRenderPass, nullptr);
        m_ImGuiRenderPass = VK_NULL_HANDLE;
    }

    if (m_ImGuiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.device, m_ImGuiDescriptorPool, nullptr);
        m_ImGuiDescriptorPool = VK_NULL_HANDLE;
    }
}

void Application::RenderImGui(Ref<rhi::CommandBuffer> cmd, Ref<rhi::Texture> backBuffer) {
    // METAGFX_INFO << "RenderImGui: ENTRY";
    if (!m_Device || !cmd || !backBuffer) {
        // METAGFX_INFO << "RenderImGui: Early exit - null pointer";
        return;
    }

    auto vkDevice = std::static_pointer_cast<rhi::VulkanDevice>(m_Device);
    if (!vkDevice) {
        // METAGFX_INFO << "RenderImGui: Early exit - vkDevice null";
        return;
    }

    auto& context = vkDevice->GetContext();
    auto swapChain = m_Device->GetSwapChain();
    if (!swapChain) {
        // METAGFX_INFO << "RenderImGui: Early exit - swapChain null";
        return;
    }

    // Start ImGui frame
    // METAGFX_INFO << "RenderImGui: Calling ImGui_ImplVulkan_NewFrame";
    ImGui_ImplVulkan_NewFrame();
    // METAGFX_INFO << "RenderImGui: Calling ImGui_ImplSDL3_NewFrame";
    ImGui_ImplSDL3_NewFrame();
    // METAGFX_INFO << "RenderImGui: Calling ImGui::NewFrame";
    ImGui::NewFrame();
    // METAGFX_INFO << "RenderImGui: ImGui frame started";

    // Define UI
    // METAGFX_INFO << "RenderImGui: Creating UI";
    ImGui::SetNextWindowSize(ImVec2(350, 550), ImGuiCond_FirstUseEver);
    ImGui::Begin("MetaGFX Controls");

    ImGui::Text("Rendering Controls");
    ImGui::Separator();

    // Model selection
    const char* modelNames[] = {
        "Antique Camera",
        "Bunny",
        "Damaged Helmet",
        "Metal Rough Spheres"
    };
    int currentModel = m_CurrentModelIndex;
    if (ImGui::Combo("Model", &currentModel, modelNames, IM_ARRAYSIZE(modelNames))) {
        if (currentModel != m_CurrentModelIndex) {
            m_CurrentModelIndex = currentModel;
            m_PendingModelPath = m_AvailableModels[m_CurrentModelIndex];
            m_HasPendingModel = true;
        }
    }

    ImGui::Spacing();

    // Exposure slider
    ImGui::SliderFloat("Exposure", &m_Exposure, 0.1f, 5.0f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Image-Based Lighting");
    ImGui::Separator();

    // IBL toggle
    ImGui::Checkbox("Enable IBL", &m_EnableIBL);

    // IBL intensity slider (only show when IBL is enabled)
    if (m_EnableIBL) {
        ImGui::SliderFloat("IBL Intensity", &m_IBLIntensity, 0.0f, 2.0f);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Active");
        ImGui::Text("Environment lighting enabled");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Status: Disabled");
        ImGui::Text("Using simple ambient (3%%)");
    }

    ImGui::Spacing();
    ImGui::Text("Tip: Toggle to see the difference!");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Skybox");
    ImGui::Separator();

    // Skybox toggle
    ImGui::Checkbox("Show Skybox", &m_ShowSkybox);

    // Skybox LOD slider (only show when skybox is enabled)
    if (m_ShowSkybox) {
        ImGui::SliderFloat("Skybox Blur", &m_SkyboxLOD, 0.0f, 5.0f);
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Visible");
        ImGui::Text("Environment map displayed");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Status: Hidden");
        ImGui::Text("Using solid color background");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Shadow Mapping");
    ImGui::Separator();

    // Shadow toggle
    if (ImGui::Checkbox("Enable Shadows", &m_EnableShadows)) {
        METAGFX_INFO << "Shadow enabled state changed to: " << (m_EnableShadows ? "ENABLED" : "DISABLED");
    }

    // Ground plane toggle
    ImGui::Checkbox("Show Ground Plane", &m_ShowGroundPlane);

    // Shadow controls (only show when shadows are enabled)
    if (m_EnableShadows) {
        // Shadow bias slider
        if (ImGui::SliderFloat("Shadow Bias", &m_ShadowBias, 0.0f, 0.01f, "%.5f")) {
            // Shadow bias will be updated in the next frame's render pass
        }

        // Light direction controls
        ImGui::Text("Light Direction:");
        ImGui::SliderFloat("Light X", &m_LightDirection.x, -2.0f, 2.0f);
        ImGui::SliderFloat("Light Y", &m_LightDirection.y, -2.0f, 2.0f);
        ImGui::SliderFloat("Light Z", &m_LightDirection.z, -2.0f, 2.0f);
        if (ImGui::Button("Reset Light Direction")) {
            m_LightDirection = glm::vec3(0.5f, -1.0f, -0.3f);
        }

        // Shadow debug mode selector
        const char* debugModes[] = {
            "Off",
            "Shadow Factor",
            "Normals",
            "Depth & Factor",
            "Depth Color",
            "Shadow Grayscale",
            "Depth vs Sample"
        };
        if (ImGui::Combo("Debug Mode", &m_ShadowDebugMode, debugModes, 7)) {
            m_VisualizeShadowMap = (m_ShadowDebugMode > 0);
        }

        if (m_ShadowDebugMode == 1) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  White = lit, Black = shadowed");
        } else if (m_ShadowDebugMode == 2) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  RGB = vertex normals");
        } else if (m_ShadowDebugMode == 3) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  Split: world pos | light space pos");
        } else if (m_ShadowDebugMode == 4) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  RGB = proj XY + depth");
        } else if (m_ShadowDebugMode == 5) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  Grayscale shadow factor");
        } else if (m_ShadowDebugMode == 6) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "  R = depth, G = comparison result");
        }

        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Active");
        if (m_ShadowMap) {
            ImGui::Text("Shadow Map: %ux%u", m_ShadowMap->GetWidth(), m_ShadowMap->GetHeight());
            ImGui::Text("Using PCF (3x3 kernel)");
        }

        ImGui::Spacing();
        ImGui::Text("Tip: Adjust bias to reduce acne");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Status: Disabled");
        ImGui::Text("No shadow rendering");
    }

    // Demo window toggle
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Checkbox("Show Demo Window", &m_ShowDemoWindow);

    ImGui::End();

    // Show demo window if enabled
    if (m_ShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }

    // Render ImGui
    // METAGFX_INFO << "RenderImGui: Calling ImGui::Render";
    ImGui::Render();

    // Check if there's anything to draw
    ImDrawData* drawData = ImGui::GetDrawData();
    // METAGFX_INFO << "RenderImGui: DrawData vertices=" << (drawData ? drawData->TotalVtxCount : -1);
    if (!drawData || drawData->TotalVtxCount == 0) {
        // METAGFX_INFO << "RenderImGui: No draw data, returning";
        return;  // Nothing to draw
    }

    // Get image index from swap chain
    // METAGFX_INFO << "RenderImGui: Getting image index";
    auto vkSwapChain = std::static_pointer_cast<rhi::VulkanSwapChain>(swapChain);
    if (!vkSwapChain) {
        // METAGFX_INFO << "RenderImGui: vkSwapChain is null";
        return;
    }
    uint32_t imageIndex = vkSwapChain->GetCurrentImageIndex();
    // METAGFX_INFO << "RenderImGui: Image index=" << imageIndex;

    auto vkTexture = std::static_pointer_cast<rhi::VulkanTexture>(backBuffer);
    if (!vkTexture) {
        // METAGFX_INFO << "RenderImGui: vkTexture is null";
        return;
    }

    // Create framebuffer if needed (lazy creation)
    // METAGFX_INFO << "RenderImGui: Checking framebuffer[" << imageIndex << "]";
    if (m_ImGuiFramebuffers[imageIndex] == VK_NULL_HANDLE) {
        // METAGFX_INFO << "RenderImGui: Creating framebuffer";
        VkImageView attachments[] = { vkTexture->GetImageView() };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_ImGuiRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChain->GetWidth();
        framebufferInfo.height = swapChain->GetHeight();
        framebufferInfo.layers = 1;

        vkCreateFramebuffer(context.device, &framebufferInfo, nullptr, &m_ImGuiFramebuffers[imageIndex]);
        // METAGFX_INFO << "RenderImGui: Framebuffer created";
    }

    // Use the provided command buffer (already recording)
    // METAGFX_INFO << "RenderImGui: Getting command buffer handle";
    auto vkCmd = std::static_pointer_cast<rhi::VulkanCommandBuffer>(cmd);
    VkCommandBuffer commandBuffer = vkCmd->GetHandle();

    // Transition image from PRESENT_SRC_KHR to COLOR_ATTACHMENT_OPTIMAL for ImGui render pass
    // METAGFX_INFO << "RenderImGui: Setting up image barrier";
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkTexture->GetImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // METAGFX_INFO << "RenderImGui: Recording pipeline barrier";
    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // METAGFX_INFO << "RenderImGui: Setting up render pass";
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_ImGuiRenderPass;
    renderPassInfo.framebuffer = m_ImGuiFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {swapChain->GetWidth(), swapChain->GetHeight()};

    // METAGFX_INFO << "RenderImGui: Beginning render pass";
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Record ImGui draw data
    // METAGFX_INFO << "RenderImGui: Recording ImGui draw data";
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    // METAGFX_INFO << "RenderImGui: Ending render pass";
    vkCmdEndRenderPass(commandBuffer);

    // METAGFX_INFO << "RenderImGui: EXIT";
    // Note: Command buffer will be ended and submitted by Render() after this function returns
}

} // namespace metagfx

