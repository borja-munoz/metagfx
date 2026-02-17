// ============================================================================
// src/app/Application.cpp
// ============================================================================
#include "Application.h"
#include "BindingLayout.h"
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/CommandBuffer.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Pipeline.h"
#include "metagfx/rhi/Sampler.h"
#include "metagfx/rhi/Shader.h"
#include "metagfx/rhi/SwapChain.h"
#include "metagfx/rhi/Texture.h"
#include "metagfx/rhi/Types.h"
#ifdef METAGFX_USE_VULKAN
#include "metagfx/rhi/vulkan/VulkanBuffer.h"
#include "metagfx/rhi/vulkan/VulkanCommandBuffer.h"
#include "metagfx/rhi/vulkan/VulkanDevice.h"
#include "metagfx/rhi/DescriptorSet.h"
#include "metagfx/rhi/vulkan/VulkanPipeline.h"
#include "metagfx/rhi/vulkan/VulkanSwapChain.h"
#include "metagfx/rhi/vulkan/VulkanTexture.h"
#endif
#ifdef METAGFX_USE_METAL
#include "metagfx/rhi/metal/MetalCommandBuffer.h"
#include "metagfx/rhi/metal/MetalDevice.h"
#include "metagfx/rhi/metal/MetalTexture.h"
#endif
#include "metagfx/math/Frustum.h"
#include "metagfx/scene/Camera.h"
#include "metagfx/scene/Material.h"
#include "metagfx/scene/Mesh.h"
#include "metagfx/scene/ShadowMap.h"
#include "metagfx/scene/pbrt/PbrtLoader.h"
#include "metagfx/utils/TextureUtils.h"
#include <SDL3/SDL.h>
#include <cctype>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#ifdef METAGFX_USE_VULKAN
#include <imgui_impl_vulkan.h>
#endif
#ifdef METAGFX_USE_METAL
#include <imgui_impl_metal.h>
#endif
#ifdef METAGFX_USE_WEBGPU
#include "metagfx/rhi/webgpu/WebGPUDevice.h"
#include "metagfx/rhi/webgpu/WebGPUCommandBuffer.h"
#include <imgui_impl_wgpu.h>
#include <webgpu/webgpu.h>
#endif

namespace metagfx {

Application::Application(const ApplicationConfig& config)
    : m_Config(config) {
    Init();
}

Application::~Application() {
    Shutdown();
}

void Application::InitWindow(rhi::GraphicsAPI api) {
    // Create window with appropriate flags for selected graphics API
    uint32_t windowFlags = SDL_WINDOW_RESIZABLE;

    const char* apiName = "Unknown";
    switch (api) {
        case rhi::GraphicsAPI::Vulkan: apiName = "Vulkan"; break;
        case rhi::GraphicsAPI::Direct3D12: apiName = "D3D12"; break;
        case rhi::GraphicsAPI::Metal: apiName = "Metal"; break;
        case rhi::GraphicsAPI::WebGPU: apiName = "WebGPU"; break;
    }
    METAGFX_INFO << "Creating window for backend: " << apiName;

#ifdef METAGFX_USE_VULKAN
    if (api == rhi::GraphicsAPI::Vulkan) {
        windowFlags |= SDL_WINDOW_VULKAN;
    }
#endif
#ifdef METAGFX_USE_METAL
    if (api == rhi::GraphicsAPI::Metal) {
        windowFlags |= SDL_WINDOW_METAL;
    }
#endif

    m_Window = SDL_CreateWindow(
        m_Config.title.c_str(),
        m_Config.width,
        m_Config.height,
        windowFlags
    );

    if (!m_Window) {
        METAGFX_CRITICAL << "Failed to create window: " << SDL_GetError();
        return;
    }

    METAGFX_INFO << "Window created: " << m_Config.width << "x" << m_Config.height;
}

void Application::Init() {
    METAGFX_INFO << "Initializing application...";

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        METAGFX_CRITICAL << "Failed to initialize SDL: " << SDL_GetError();
        return;
    }
    METAGFX_INFO << "SDL initialized successfully";

    InitWindow(m_Config.graphicsAPI);
    if (!m_Window) return;

    // Initialize model list (static data — done once here, not in InitGPUResources)
    m_AvailableModels = {
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/AntiqueCamera.glb",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/bunny_tex_coords.obj",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/DamagedHelmet.glb",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/models/MetalRoughSpheres.glb",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/scenes/cornell-box.pbrt",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/scenes/cornell-box/scene-v4.pbrt",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/scenes/contemporary-bathroom/contemporary-bathroom.pbrt",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/scenes/milestone52-test.pbrt",
        "/Users/Borja/dev/borja-munoz/metagfx/assets/scenes/classroom/scene-v4.pbrt"
    };
    m_CurrentModelIndex = 4;  // Default: Cornell Box

    InitGPUResources();
    m_Running = true;
}

void Application::InitGPUResources() {
    const char* apiName = "Unknown";
    switch (m_Config.graphicsAPI) {
        case rhi::GraphicsAPI::Vulkan: apiName = "Vulkan"; break;
        case rhi::GraphicsAPI::Direct3D12: apiName = "D3D12"; break;
        case rhi::GraphicsAPI::Metal: apiName = "Metal"; break;
        case rhi::GraphicsAPI::WebGPU: apiName = "WebGPU"; break;
    }
    METAGFX_INFO << "Initializing GPU resources for backend: " << apiName;

    // Create graphics device with configured API
    m_Device = rhi::CreateGraphicsDevice(m_Config.graphicsAPI, m_Window);
    if (!m_Device) {
        METAGFX_ERROR << "Failed to create graphics device for " << apiName;
        return;
    }

    METAGFX_INFO << "Graphics device created: " << m_Device->GetDeviceInfo().deviceName;

    // Create camera with appropriate Y-axis flip based on graphics API
    // Vulkan and Metal use Y-down NDC (flip required)
    // WebGPU uses Y-up NDC like OpenGL (no flip)
    bool flipY = (m_Config.graphicsAPI == rhi::GraphicsAPI::Vulkan ||
                  m_Config.graphicsAPI == rhi::GraphicsAPI::Metal);

    m_Camera = std::make_unique<Camera>(
        45.0f,
        static_cast<float>(m_Config.width) / static_cast<float>(m_Config.height),
        0.1f,
        100.0f,
        flipY
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

    // Create push constant buffers (for WebGPU compatibility)
    // Model push constants: cameraPosition (vec4) + materialFlags (uint) + exposure (float) + enableIBL (uint) + iblIntensity (float) + shadowDebugMode (uint) + enableShadows (uint)
    // Total: 16 + 4 + 4 + 4 + 4 + 4 + 4 = 40 bytes
    BufferDesc modelPushConstDesc{};
    modelPushConstDesc.size = 64;  // Aligned to 64 bytes for safety
    modelPushConstDesc.usage = BufferUsage::Uniform;
    modelPushConstDesc.memoryUsage = MemoryUsage::CPUToGPU;
    // Create double-buffered push constant buffers (one per frame in flight)
    // This prevents race conditions where CPU writes frame N while GPU reads frame N-1
    m_ModelPushConstantBuffer[0] = m_Device->CreateBuffer(modelPushConstDesc);
    m_ModelPushConstantBuffer[1] = m_Device->CreateBuffer(modelPushConstDesc);

    // Initialize model push constants with default values
    // NOTE: Must match shader layout exactly (40 bytes, no padding)
    struct ModelPushConstants {
        glm::vec4 cameraPosition;    // 16 bytes
        uint32_t materialFlags;      // 4 bytes
        float exposure;              // 4 bytes
        uint32_t enableIBL;          // 4 bytes
        float iblIntensity;          // 4 bytes
        uint32_t shadowDebugMode;    // 4 bytes
        uint32_t enableShadows;      // 4 bytes
    };  // Total: 40 bytes
    ModelPushConstants initialModelPushConst{};
    initialModelPushConst.cameraPosition = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    initialModelPushConst.materialFlags = 0;
    initialModelPushConst.exposure = 1.0f;
    initialModelPushConst.enableIBL = 0;
    initialModelPushConst.iblIntensity = 1.0f;
    initialModelPushConst.shadowDebugMode = 0;  // No debug mode
    initialModelPushConst.enableShadows = 0;
    m_ModelPushConstantBuffer[0]->CopyData(&initialModelPushConst, sizeof(ModelPushConstants));
    m_ModelPushConstantBuffer[1]->CopyData(&initialModelPushConst, sizeof(ModelPushConstants));

    // Skybox push constants: exposure (float) + lod (float)
    // Total: 4 + 4 = 8 bytes
    BufferDesc skyboxPushConstDesc{};
    skyboxPushConstDesc.size = 16;  // Aligned to 16 bytes
    skyboxPushConstDesc.usage = BufferUsage::Uniform;
    skyboxPushConstDesc.memoryUsage = MemoryUsage::CPUToGPU;
    m_SkyboxPushConstantBuffer = m_Device->CreateBuffer(skyboxPushConstDesc);

    // Initialize skybox push constants with default values
    struct SkyboxPushConstants {
        float exposure;
        float lod;
    };
    SkyboxPushConstants initialSkyboxPushConst{};
    initialSkyboxPushConst.exposure = 1.0f;
    initialSkyboxPushConst.lod = 0.0f;
    m_SkyboxPushConstantBuffer->CopyData(&initialSkyboxPushConst, sizeof(SkyboxPushConstants));

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
    m_Scene->InitializeLightBuffer(m_Device.get(), m_Config.graphicsAPI);

    // Create test lights
    CreateTestLights();

    // Upload light data to GPU before creating descriptor sets
    m_Scene->UpdateLightBuffer();

    // Create shadow map (2048x2048 default resolution)
    m_ShadowMap = std::make_unique<ShadowMap>(m_Device, 2048, 2048);

    // Create descriptor set with 15 bindings for model rendering
    // WebGPU uses sparse layout with gaps for auto-inserted samplers
    // Vulkan/Metal use dense layout with combined image samplers
    using rhi::DescriptorType;
    using rhi::ShaderStage;
    using rhi::DescriptorBindingDesc;

    // Use uniform buffer for lights on all backends - the shader declares it as
    // uniform (std140) and the size (1040 bytes) fits within WebGPU's 65536-byte limit
    DescriptorType lightBufferType = DescriptorType::UniformBuffer;

    // Create descriptor sets for each frame (double buffering for Vulkan)
    for (uint32 frameIndex = 0; frameIndex < 2; frameIndex++) {
        std::vector<DescriptorBindingDesc> bindings = {
            { BINDING(m_Config.graphicsAPI, ModelBindings::MVP), DescriptorType::UniformBuffer, ShaderStage::Vertex, m_UniformBuffers[frameIndex], nullptr, nullptr },  // MVP matrices (per-frame buffer)
            { BINDING(m_Config.graphicsAPI, ModelBindings::MATERIAL), DescriptorType::UniformBuffer, ShaderStage::Fragment, m_MaterialBuffers[frameIndex], nullptr, nullptr },  // Material (per-frame buffer)
            { BINDING(m_Config.graphicsAPI, ModelBindings::ALBEDO), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultTexture, m_LinearRepeatSampler },  // Albedo
            { BINDING(m_Config.graphicsAPI, ModelBindings::LIGHTS), lightBufferType, ShaderStage::Fragment, m_Scene->GetLightBuffer(), nullptr, nullptr },  // Lights
            { BINDING(m_Config.graphicsAPI, ModelBindings::NORMAL), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultNormalMap, m_LinearRepeatSampler },  // Normal
            { BINDING(m_Config.graphicsAPI, ModelBindings::METALLIC), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultWhiteTexture, m_LinearRepeatSampler },  // Metallic
            { BINDING(m_Config.graphicsAPI, ModelBindings::ROUGHNESS), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultWhiteTexture, m_LinearRepeatSampler },  // Roughness
            { BINDING(m_Config.graphicsAPI, ModelBindings::AO), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultWhiteTexture, m_LinearRepeatSampler },  // AO
            { BINDING(m_Config.graphicsAPI, ModelBindings::IRRADIANCE), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_IrradianceMap, m_CubemapSampler },  // Irradiance
            { BINDING(m_Config.graphicsAPI, ModelBindings::PREFILTERED), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_PrefilteredMap, m_CubemapSampler },  // Prefiltered
            { BINDING(m_Config.graphicsAPI, ModelBindings::BRDF_LUT), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_BRDF_LUT, m_LinearRepeatSampler },  // BRDF LUT
            { BINDING(m_Config.graphicsAPI, ModelBindings::EMISSIVE), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultBlackTexture, m_LinearRepeatSampler },  // Emissive
            { BINDING(m_Config.graphicsAPI, ModelBindings::SHADOWMAP), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_ShadowMap->GetDepthTexture(), m_ShadowMap->GetSampler() },  // Shadow map
            { BINDING(m_Config.graphicsAPI, ModelBindings::SHADOW_UBO), DescriptorType::UniformBuffer, ShaderStage::Fragment, m_ShadowUniformBuffer, nullptr, nullptr },  // Shadow UBO
            { BINDING(m_Config.graphicsAPI, ModelBindings::PUSH_CONSTANTS), DescriptorType::UniformBuffer, ShaderStage::Fragment, m_ModelPushConstantBuffer[frameIndex], nullptr, nullptr }  // Push constants (double-buffered per frame)
        };

        rhi::DescriptorSetDesc descriptorSetDesc;
        descriptorSetDesc.bindings = bindings;
        descriptorSetDesc.debugName = "MainDescriptorSet";
        m_DescriptorSet[frameIndex] = m_Device->CreateDescriptorSet(descriptorSetDesc);
    }

    // Create ground plane descriptor sets (double buffered, same layout as model but separate instance)
    // We MUST use the same layout because they share the same pipeline
    for (uint32 frameIndex = 0; frameIndex < 2; frameIndex++) {
        std::vector<DescriptorBindingDesc> groundPlaneBindings = {
            { BINDING(m_Config.graphicsAPI, ModelBindings::MVP), DescriptorType::UniformBuffer, ShaderStage::Vertex, m_UniformBuffers[frameIndex], nullptr, nullptr },  // MVP (per-frame)
            { BINDING(m_Config.graphicsAPI, ModelBindings::MATERIAL), DescriptorType::UniformBuffer, ShaderStage::Fragment, m_GroundPlaneMaterialBuffer, nullptr, nullptr },  // Ground plane material
            { BINDING(m_Config.graphicsAPI, ModelBindings::ALBEDO), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultWhiteTexture, m_LinearRepeatSampler },  // Albedo
            { BINDING(m_Config.graphicsAPI, ModelBindings::LIGHTS), lightBufferType, ShaderStage::Fragment, m_Scene->GetLightBuffer(), nullptr, nullptr },  // Lights
            { BINDING(m_Config.graphicsAPI, ModelBindings::NORMAL), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultNormalMap, m_LinearRepeatSampler },  // Normal
            { BINDING(m_Config.graphicsAPI, ModelBindings::METALLIC), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultWhiteTexture, m_LinearRepeatSampler },  // Metallic
            { BINDING(m_Config.graphicsAPI, ModelBindings::ROUGHNESS), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultWhiteTexture, m_LinearRepeatSampler },  // Roughness
            { BINDING(m_Config.graphicsAPI, ModelBindings::AO), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultWhiteTexture, m_LinearRepeatSampler },  // AO
            { BINDING(m_Config.graphicsAPI, ModelBindings::IRRADIANCE), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_IrradianceMap, m_CubemapSampler },  // Irradiance
            { BINDING(m_Config.graphicsAPI, ModelBindings::PREFILTERED), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_PrefilteredMap, m_CubemapSampler },  // Prefiltered
            { BINDING(m_Config.graphicsAPI, ModelBindings::BRDF_LUT), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_BRDF_LUT, m_LinearRepeatSampler },  // BRDF LUT
            { BINDING(m_Config.graphicsAPI, ModelBindings::EMISSIVE), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_DefaultBlackTexture, m_LinearRepeatSampler },  // Emissive
            { BINDING(m_Config.graphicsAPI, ModelBindings::SHADOWMAP), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_ShadowMap->GetDepthTexture(), m_ShadowMap->GetSampler() },  // Shadow map
            { BINDING(m_Config.graphicsAPI, ModelBindings::SHADOW_UBO), DescriptorType::UniformBuffer, ShaderStage::Fragment, m_ShadowUniformBuffer, nullptr, nullptr },  // Shadow UBO
            { BINDING(m_Config.graphicsAPI, ModelBindings::PUSH_CONSTANTS), DescriptorType::UniformBuffer, ShaderStage::Fragment, m_ModelPushConstantBuffer[frameIndex], nullptr, nullptr }  // Push constants (double-buffered per frame)
        };

        rhi::DescriptorSetDesc groundPlaneDescriptorSetDesc;
        groundPlaneDescriptorSetDesc.bindings = groundPlaneBindings;
        groundPlaneDescriptorSetDesc.debugName = "GroundPlaneDescriptorSet";
        m_GroundPlaneDescriptorSet[frameIndex] = m_Device->CreateDescriptorSet(groundPlaneDescriptorSetDesc);
    }

    // Create shadow descriptor set (for shadow pass rendering)
    std::vector<DescriptorBindingDesc> shadowBindings = {
        { ShadowBindings::SHADOW_UBO, DescriptorType::UniformBuffer, ShaderStage::Vertex, m_ShadowUniformBuffer, nullptr, nullptr }  // Shadow UBO
    };

    rhi::DescriptorSetDesc shadowDescriptorSetDesc;
    shadowDescriptorSetDesc.bindings = shadowBindings;
    shadowDescriptorSetDesc.debugName = "ShadowDescriptorSet";
    m_ShadowDescriptorSet = m_Device->CreateDescriptorSet(shadowDescriptorSetDesc);

    // Create skybox descriptor sets (double buffered)
    // Use BINDING() macro to get correct binding numbers per backend
    // WebGPU uses sparse layout: 0(MVP), 1(env texture), 2(env sampler by Tint), 3(push constants)
    // Vulkan/Metal use dense layout: 0(MVP), 1(env combined), 2(push constants)
    for (uint32 frameIndex = 0; frameIndex < 2; frameIndex++) {
        std::vector<DescriptorBindingDesc> skyboxBindings = {
            { BINDING(m_Config.graphicsAPI, SkyboxBindings::MVP), DescriptorType::UniformBuffer, ShaderStage::Vertex, m_UniformBuffers[frameIndex], nullptr, nullptr },  // MVP matrices (per-frame buffer)
            { BINDING(m_Config.graphicsAPI, SkyboxBindings::ENVIRONMENT), DescriptorType::SampledTexture, ShaderStage::Fragment, nullptr, m_EnvironmentMap, m_CubemapSampler },  // Environment cubemap
            { BINDING(m_Config.graphicsAPI, SkyboxBindings::PUSH_CONSTANTS), DescriptorType::UniformBuffer, ShaderStage::Fragment, m_SkyboxPushConstantBuffer, nullptr, nullptr }  // Push constants (WebGPU needs this as UBO)
        };

        rhi::DescriptorSetDesc skyboxDescriptorSetDesc;
        skyboxDescriptorSetDesc.bindings = skyboxBindings;
        skyboxDescriptorSetDesc.debugName = "SkyboxDescriptorSet";
        m_SkyboxDescriptorSet[frameIndex] = m_Device->CreateDescriptorSet(skyboxDescriptorSetDesc);
    }

    // Set descriptor set layout on device before creating pipeline (use frame 0)
    m_Device->SetActiveDescriptorSetLayout(m_DescriptorSet[0]);

    // Create triangle resources
    CreateTriangle();

    // Create model pipeline
    CreateModelPipeline();

    // Create skybox pipeline with skybox descriptor set layout (use frame 0)
    m_Device->SetActiveDescriptorSetLayout(m_SkyboxDescriptorSet[0]);
    CreateSkyboxPipeline();

    // Create shadow pipeline with shadow descriptor set layout
    m_Device->SetActiveDescriptorSetLayout(m_ShadowDescriptorSet);
    CreateShadowPipeline();

    // Restore main descriptor set layout (use frame 0)
    m_Device->SetActiveDescriptorSetLayout(m_DescriptorSet[0]);

    // Create skybox cube geometry
    CreateSkyboxCube();

    // Load model (m_AvailableModels and m_CurrentModelIndex set by caller)
    if (!m_AvailableModels.empty()) {
        LoadModel(m_AvailableModels[m_CurrentModelIndex]);
    }

    // Create ground plane for shadow visualization
    CreateGroundPlane();

    // Create initial instance buffer (single identity transform for non-instanced rendering)
    CreateInstanceBuffer();

    // Create a permanent 1-element identity instance buffer.
    // Vulkan requires ALL pipeline vertex bindings to be bound before a draw call.
    // Non-instanced draws (ground plane) bind this to slot 1 instead of m_InstanceBuffer.
    {
        glm::mat4 identity(1.0f);
        rhi::BufferDesc idDesc{};
        idDesc.size        = sizeof(glm::mat4);
        idDesc.usage       = rhi::BufferUsage::Vertex | rhi::BufferUsage::TransferDst;
        idDesc.memoryUsage = rhi::MemoryUsage::CPUToGPU;
        m_SingleInstanceBuffer = m_Device->CreateBuffer(idDesc);
        if (m_SingleInstanceBuffer)
            m_SingleInstanceBuffer->CopyData(&identity, sizeof(identity));
    }

    // Start frame timing
    m_FrameStart = std::chrono::steady_clock::now();

    METAGFX_INFO << "Controls:";
    METAGFX_INFO << "  WASD/QE - Camera movement";
    METAGFX_INFO << "  Mouse drag - Rotate camera";
    METAGFX_INFO << "  1-4 - Load specific model";
    METAGFX_INFO << "  N - Next model";
    METAGFX_INFO << "  P - Previous model";
    METAGFX_INFO << "  ESC - Exit";

    // Initialize ImGui
    InitImGui();
}

void Application::LoadModel(const std::string& path) {
    METAGFX_INFO << "Loading model: " << path;

    // ── PBRT v4 scene files ───────────────────────────────────────────────────
    if (path.size() > 5 && path.rfind(".pbrt") == path.size() - 5) {
        auto result = PbrtLoader::Load(m_Device.get(), path, m_Scene.get());

        if (!result.model || !result.model->IsValid()) {
            METAGFX_WARN << "PBRT load failed: " << path << " — using fallback cube";
            m_Model = std::make_unique<Model>();
            m_Model->CreateCube(m_Device.get(), 1.0f);
        } else {
            m_Model = std::move(result.model);
        }

        // Apply PBRT camera if defined; otherwise frame the bounding box normally
        if (result.camera.defined) {
            float aspect  = static_cast<float>(m_Config.width) / static_cast<float>(m_Config.height);
            float eyeDist = glm::length(result.camera.eye - result.camera.look);
            float farPlane = eyeDist * 100.0f;
            m_Camera->SetPerspective(result.camera.fov, aspect, 0.1f, farPlane);
            // Orbit around the model center so the user can orbit freely around the scene
            glm::vec3 orbitCenter = m_Model->GetCenter();
            m_Camera->SetPosition(result.camera.eye);
            m_Camera->LookAt(orbitCenter, result.camera.up);
            m_Camera->SetOrbitTarget(orbitCenter);
        } else {
            glm::vec3 center = m_Model->GetCenter();
            glm::vec3 size   = m_Model->GetSize();
            m_Camera->FrameBoundingBox(center, size, 1.3f);
        }

        // PBRT scenes are self-contained: hide the ground plane and skybox so
        // they don't intrude on the scene's own geometry and environment.
        m_ShowGroundPlane = false;
        m_ShowSkybox      = false;

        // If the PBRT scene specifies an environment map via LightSource "infinite",
        // load it as a skybox cubemap (PFM equirectangular supported).
        if (!result.envMapPath.empty()) {
            METAGFX_INFO << "PBRT scene specifies environment map: " << result.envMapPath;
            bool envLoaded = false;

            // Try PFM format (equirectangular float image → GPU cubemap)
            namespace fs = std::filesystem;
            std::string ext = fs::path(result.envMapPath).extension().string();
            // lowercase the extension for comparison
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (ext == ".pfm") {
                auto hdr = utils::LoadPFMImage(result.envMapPath);
                if (hdr.pixels) {
                    auto cubemap    = utils::LoadCubemapFromEquirectangular(m_Device.get(), hdr, 256);
                    auto irradiance = utils::ComputeIrradianceCubemap(m_Device.get(), hdr, 32);
                    utils::FreeHDRImage(hdr);
                    if (cubemap) {
                        m_EnvironmentMap = std::move(cubemap);
                        RecreateSkyboxDescriptorSets();
                        m_ShowSkybox  = true;
                        m_SkyboxLOD   = 0.0f;
                        envLoaded = true;
                        METAGFX_INFO << "PFM environment map loaded as skybox cubemap";
                    }
                    if (irradiance) {
                        m_IrradianceMap = std::move(irradiance);
                        // IBL is left off by default — the scene's own lights provide direct illumination.
                        // The user can enable IBL and adjust intensity via the GUI if desired.
                        METAGFX_INFO << "Diffuse irradiance cubemap computed from PFM (IBL ready, off by default)";
                    }
                }
            }

            if (!envLoaded) {
                METAGFX_INFO << "Unsupported env map format or load failed for: " << result.envMapPath;
                METAGFX_INFO << "To use as IBL run: ./bin/tools/ibl_precompute \""
                             << result.envMapPath << "\" assets/envmaps/ && restart the app";
            }
        }

        UpdateGroundPlanePosition();

        m_Device->WaitIdle();
        if (m_Config.graphicsAPI != rhi::GraphicsAPI::WebGPU && m_DescriptorSet[0]) {
            const auto& meshes = m_Model->GetMeshes();
            if (!meshes.empty() && meshes[0] && meshes[0]->GetMaterial()) {
                UpdateModelDescriptorTextures(meshes[0]->GetMaterial());
            }
        }
        CreateMeshDescriptorSets();
        return;
    }

    // ── Regular model files (Assimp) ──────────────────────────────────────────
    m_Model = std::make_unique<Model>();
    if (!m_Model->LoadFromFile(m_Device.get(), path)) {
        METAGFX_WARN << "Failed to load " << path << ", creating fallback cube";
        if (!m_Model->CreateCube(m_Device.get(), 1.0f)) {
            METAGFX_ERROR << "Failed to create fallback cube model";
            m_Model = nullptr;
            return;
        }
    }

    // Restore default scene lights (may have been replaced by a PBRT scene)
    m_Scene->ClearLights();
    CreateTestLights();

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

    // Wait for in-flight GPU work before touching descriptor sets / material buffers
    m_Device->WaitIdle();

    // For Vulkan/Metal: update the shared descriptor set textures (used as fallback)
    if (m_Config.graphicsAPI != rhi::GraphicsAPI::WebGPU && m_DescriptorSet[0]) {
        const auto& meshes = m_Model->GetMeshes();
        if (!meshes.empty() && meshes[0] && meshes[0]->GetMaterial()) {
            UpdateModelDescriptorTextures(meshes[0]->GetMaterial());
        }
    }

    // Build per-mesh material buffers + descriptor sets so each mesh can carry
    // its own albedo/roughness/metallic to the GPU without being overwritten.
    CreateMeshDescriptorSets();
}

void Application::UpdateModelDescriptorTextures(Material* material) {
    if (!material) return;

    auto api = m_Config.graphicsAPI;

    // Get all texture references
    Ref<rhi::Texture> albedoMap = material->GetAlbedoMap();
    Ref<rhi::Texture> normalMap = material->GetNormalMap();
    Ref<rhi::Texture> metallicRoughnessMap = material->GetMetallicRoughnessMap();
    Ref<rhi::Texture> metallicMap = material->GetMetallicMap();
    Ref<rhi::Texture> roughnessMap = material->GetRoughnessMap();
    Ref<rhi::Texture> aoMap = material->GetAOMap();
    Ref<rhi::Texture> emissiveMap = material->GetEmissiveMap();

    // Update BOTH descriptor sets (for Vulkan double buffering)
    // WebGPU updates per-frame during rendering, so this only affects Vulkan/Metal
    for (uint32 frameIndex = 0; frameIndex < 2; frameIndex++) {
        if (!m_DescriptorSet[frameIndex]) continue;

        auto& descriptorSet = m_DescriptorSet[frameIndex];

        descriptorSet->UpdateTexture(BINDING(api, ModelBindings::ALBEDO),
            albedoMap ? albedoMap : m_DefaultTexture, m_LinearRepeatSampler);

        descriptorSet->UpdateTexture(BINDING(api, ModelBindings::NORMAL),
            normalMap ? normalMap : m_DefaultNormalMap, m_LinearRepeatSampler);

        if (metallicRoughnessMap) {
            descriptorSet->UpdateTexture(BINDING(api, ModelBindings::METALLIC), metallicRoughnessMap, m_LinearRepeatSampler);
            descriptorSet->UpdateTexture(BINDING(api, ModelBindings::ROUGHNESS), metallicRoughnessMap, m_LinearRepeatSampler);
        } else {
            descriptorSet->UpdateTexture(BINDING(api, ModelBindings::METALLIC),
                metallicMap ? metallicMap : m_DefaultWhiteTexture, m_LinearRepeatSampler);
            descriptorSet->UpdateTexture(BINDING(api, ModelBindings::ROUGHNESS),
                roughnessMap ? roughnessMap : m_DefaultWhiteTexture, m_LinearRepeatSampler);
        }

        descriptorSet->UpdateTexture(BINDING(api, ModelBindings::AO),
            aoMap ? aoMap : m_DefaultWhiteTexture, m_LinearRepeatSampler);

        descriptorSet->UpdateTexture(BINDING(api, ModelBindings::EMISSIVE),
            emissiveMap ? emissiveMap : m_DefaultBlackTexture, m_LinearRepeatSampler);

        descriptorSet->Update();
    }
}

void Application::CreateMeshDescriptorSets() {
    if (!m_Model || !m_Device) return;

    // Release any previously allocated per-mesh resources
    m_MeshMaterialBuffers.clear();
    m_MeshDescriptorSets.clear();

    const auto& meshes = m_Model->GetMeshes();
    if (meshes.empty()) return;

    auto api = m_Config.graphicsAPI;
    // Use uniform buffer for lights on all backends - must match the shared
    // descriptor set layout (used for pipeline layout) which also uses UniformBuffer.
    auto lightBufferType = rhi::DescriptorType::UniformBuffer;

    rhi::BufferDesc matDesc{};
    matDesc.size        = sizeof(MaterialProperties);
    matDesc.usage       = rhi::BufferUsage::Uniform;
    matDesc.memoryUsage = rhi::MemoryUsage::CPUToGPU;

    m_MeshMaterialBuffers.resize(meshes.size());
    m_MeshDescriptorSets.resize(meshes.size());

    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& mesh = meshes[i];
        Material* mat = (mesh && mesh->GetMaterial()) ? mesh->GetMaterial() : nullptr;

        // Allocate and fill the per-mesh material buffer once (mesh materials don't change at runtime)
        m_MeshMaterialBuffers[i] = m_Device->CreateBuffer(matDesc);
        if (mat) {
            MaterialProperties props = mat->GetProperties();
            m_MeshMaterialBuffers[i]->CopyData(&props, sizeof(props));
        } else {
            MaterialProperties def{};
            def.albedo    = glm::vec3(0.8f);
            def.roughness = 0.5f;
            def.metallic  = 0.0f;
            m_MeshMaterialBuffers[i]->CopyData(&def, sizeof(def));
        }

        // Resolve per-mesh texture bindings
        Ref<rhi::Texture> albedoMap      = mat ? mat->GetAlbedoMap()              : nullptr;
        Ref<rhi::Texture> normalMap      = mat ? mat->GetNormalMap()               : nullptr;
        Ref<rhi::Texture> mrMap          = mat ? mat->GetMetallicRoughnessMap()    : nullptr;
        Ref<rhi::Texture> metallicMap    = mat ? mat->GetMetallicMap()             : nullptr;
        Ref<rhi::Texture> roughnessMap   = mat ? mat->GetRoughnessMap()            : nullptr;
        Ref<rhi::Texture> aoMap          = mat ? mat->GetAOMap()                   : nullptr;
        Ref<rhi::Texture> emissiveMap    = mat ? mat->GetEmissiveMap()             : nullptr;
        Ref<rhi::Texture> metallicTex    = mrMap  ? mrMap  : (metallicMap  ? metallicMap  : m_DefaultWhiteTexture);
        Ref<rhi::Texture> roughnessTex   = mrMap  ? mrMap  : (roughnessMap ? roughnessMap : m_DefaultWhiteTexture);

        // Create one descriptor set per frame (MVP / push-constant buffer are per-frame)
        for (uint32 frame = 0; frame < 2; ++frame) {
            using rhi::DescriptorBindingDesc;
            std::vector<DescriptorBindingDesc> bindings = {
                { BINDING(api, ModelBindings::MVP),           rhi::DescriptorType::UniformBuffer,  rhi::ShaderStage::Vertex,   m_UniformBuffers[frame],                nullptr,                                      nullptr                },
                { BINDING(api, ModelBindings::MATERIAL),      rhi::DescriptorType::UniformBuffer,  rhi::ShaderStage::Fragment, m_MeshMaterialBuffers[i],               nullptr,                                      nullptr                },
                { BINDING(api, ModelBindings::ALBEDO),        rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                albedoMap ? albedoMap : m_DefaultTexture,     m_LinearRepeatSampler  },
                { BINDING(api, ModelBindings::LIGHTS),        lightBufferType,                     rhi::ShaderStage::Fragment, m_Scene->GetLightBuffer(),              nullptr,                                      nullptr                },
                { BINDING(api, ModelBindings::NORMAL),        rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                normalMap ? normalMap : m_DefaultNormalMap,   m_LinearRepeatSampler  },
                { BINDING(api, ModelBindings::METALLIC),      rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                metallicTex,                                  m_LinearRepeatSampler  },
                { BINDING(api, ModelBindings::ROUGHNESS),     rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                roughnessTex,                                 m_LinearRepeatSampler  },
                { BINDING(api, ModelBindings::AO),            rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                aoMap ? aoMap : m_DefaultWhiteTexture,        m_LinearRepeatSampler  },
                { BINDING(api, ModelBindings::IRRADIANCE),    rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                m_IrradianceMap,                              m_CubemapSampler       },
                { BINDING(api, ModelBindings::PREFILTERED),   rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                m_PrefilteredMap,                             m_CubemapSampler       },
                { BINDING(api, ModelBindings::BRDF_LUT),      rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                m_BRDF_LUT,                                   m_LinearRepeatSampler  },
                { BINDING(api, ModelBindings::EMISSIVE),      rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                emissiveMap ? emissiveMap : m_DefaultBlackTexture, m_LinearRepeatSampler },
                { BINDING(api, ModelBindings::SHADOWMAP),     rhi::DescriptorType::SampledTexture, rhi::ShaderStage::Fragment, nullptr,                                m_ShadowMap->GetDepthTexture(),               m_ShadowMap->GetSampler() },
                { BINDING(api, ModelBindings::SHADOW_UBO),    rhi::DescriptorType::UniformBuffer,  rhi::ShaderStage::Fragment, m_ShadowUniformBuffer,                  nullptr,                                      nullptr                },
                { BINDING(api, ModelBindings::PUSH_CONSTANTS),rhi::DescriptorType::UniformBuffer,  rhi::ShaderStage::Fragment, m_ModelPushConstantBuffer[frame],        nullptr,                                      nullptr                },
            };
            rhi::DescriptorSetDesc desc{};
            desc.bindings = bindings;
            m_MeshDescriptorSets[i][frame] = m_Device->CreateDescriptorSet(desc);
        }
    }

    METAGFX_INFO << "CreateMeshDescriptorSets: created " << meshes.size()
                 << " per-mesh descriptor set pair(s)";
}

void Application::RecreateSkyboxDescriptorSets() {
    using namespace rhi;
    if (!m_EnvironmentMap) return;

    m_Device->WaitIdle();

    for (uint32 frameIndex = 0; frameIndex < 2; ++frameIndex) {
        m_SkyboxDescriptorSet[frameIndex].reset();

        std::vector<DescriptorBindingDesc> skyboxBindings = {
            { BINDING(m_Config.graphicsAPI, SkyboxBindings::MVP),
              DescriptorType::UniformBuffer, ShaderStage::Vertex,
              m_UniformBuffers[frameIndex], nullptr, nullptr },
            { BINDING(m_Config.graphicsAPI, SkyboxBindings::ENVIRONMENT),
              DescriptorType::SampledTexture, ShaderStage::Fragment,
              nullptr, m_EnvironmentMap, m_CubemapSampler },
            { BINDING(m_Config.graphicsAPI, SkyboxBindings::PUSH_CONSTANTS),
              DescriptorType::UniformBuffer, ShaderStage::Fragment,
              m_SkyboxPushConstantBuffer, nullptr, nullptr }
        };

        rhi::DescriptorSetDesc desc;
        desc.bindings  = skyboxBindings;
        desc.debugName = "SkyboxDescriptorSet";
        m_SkyboxDescriptorSet[frameIndex] = m_Device->CreateDescriptorSet(desc);
    }
    METAGFX_INFO << "Skybox descriptor sets recreated with new environment map";
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
        { 0, rhi::Format::R32G32B32_SFLOAT, 0, 0 },                // position at location 0, binding 0
        { 1, rhi::Format::R32G32B32_SFLOAT, sizeof(float) * 3, 0 } // color at location 1, binding 0
    };

    pipelineDesc.topology = rhi::PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = rhi::CullMode::None;
    pipelineDesc.descriptorSetLayout = m_DescriptorSet[0];  // WebGPU requires explicit pipeline layout

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

    // Vertex input: position (vec3), normal (vec3), texcoord (vec2) + per-instance mat4
    pipelineDesc.vertexInput.stride = sizeof(Vertex);  // Legacy fallback for single-binding backends
    pipelineDesc.vertexInput.attributes = {
        { 0, rhi::Format::R32G32B32_SFLOAT, 0, 0 },
        { 1, rhi::Format::R32G32B32_SFLOAT, sizeof(float) * 3, 0 },
        { 2, rhi::Format::R32G32_SFLOAT,    sizeof(float) * 6, 0 }
    };
    // Multi-binding vertex input: binding 0 = per-vertex, binding 1 = per-instance mat4
    pipelineDesc.vertexInputState.bindings = {
        { 0, sizeof(Vertex),         rhi::VertexInputRate::Vertex   },
        { 1, sizeof(glm::mat4),      rhi::VertexInputRate::Instance },
    };
    pipelineDesc.vertexInputState.attributes = {
        // Per-vertex (binding 0)
        { 0, rhi::Format::R32G32B32_SFLOAT,    0,                  0 },
        { 1, rhi::Format::R32G32B32_SFLOAT,    sizeof(float) * 3,  0 },
        { 2, rhi::Format::R32G32_SFLOAT,       sizeof(float) * 6,  0 },
        // Per-instance mat4 rows (binding 1)
        { 3, rhi::Format::R32G32B32A32_SFLOAT, sizeof(float) * 0,  1 },
        { 4, rhi::Format::R32G32B32A32_SFLOAT, sizeof(float) * 4,  1 },
        { 5, rhi::Format::R32G32B32A32_SFLOAT, sizeof(float) * 8,  1 },
        { 6, rhi::Format::R32G32B32A32_SFLOAT, sizeof(float) * 12, 1 },
    };

    pipelineDesc.topology = rhi::PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = rhi::CullMode::None;  // Cornell box interior + other scenes
    pipelineDesc.rasterization.frontFace = rhi::FrontFace::CounterClockwise;

    // Enable depth testing for proper 3D rendering
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = true;
    pipelineDesc.depthStencil.depthCompareOp = CompareOp::Less;  // Standard depth test
    pipelineDesc.descriptorSetLayout = m_DescriptorSet[0];  // WebGPU requires explicit pipeline layout

    // WebGPU requires explicit color attachment (even if using default format)
    // Vulkan and Metal do NOT use this field - it causes pipeline creation to fail
    METAGFX_INFO << "Graphics API for model pipeline: " << static_cast<int>(m_Config.graphicsAPI)
                 << " (Vulkan=0, Metal=2, WebGPU=3)";
    if (m_Config.graphicsAPI == rhi::GraphicsAPI::WebGPU) {
        METAGFX_INFO << "Setting colorAttachments for WebGPU";
        pipelineDesc.colorAttachments = { rhi::ColorAttachmentState{} };  // One color attachment, no blending
    } else {
        METAGFX_INFO << "Skipping colorAttachments for Vulkan/Metal";
    }

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
        { 0, Format::R32G32B32_SFLOAT, 0, 0 }  // position at location 0, binding 0
    };

    pipelineDesc.topology = PrimitiveTopology::TriangleList;
    pipelineDesc.rasterization.cullMode = CullMode::None;  // No culling for debugging
    pipelineDesc.rasterization.frontFace = FrontFace::CounterClockwise;

    // Depth testing: LessOrEqual without writing depth (render where depth == 1.0 cleared value)
    pipelineDesc.depthStencil.depthTestEnable = true;
    pipelineDesc.depthStencil.depthWriteEnable = false;  // Don't write depth
    pipelineDesc.depthStencil.depthCompareOp = CompareOp::LessOrEqual;
    pipelineDesc.descriptorSetLayout = m_SkyboxDescriptorSet[0];  // WebGPU requires explicit pipeline layout

    // WebGPU requires explicit color attachment (even if using default format)
    // Vulkan and Metal do NOT use this field - it causes pipeline creation to fail
    if (m_Config.graphicsAPI == rhi::GraphicsAPI::WebGPU) {
        pipelineDesc.colorAttachments = { rhi::ColorAttachmentState{} };  // One color attachment, no blending
    }

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

    // Vertex input: position (vec3) only per-vertex + per-instance mat4
    pipelineDesc.vertexInput.stride = sizeof(Vertex);  // Legacy fallback
    pipelineDesc.vertexInput.attributes = {
        { 0, Format::R32G32B32_SFLOAT, 0, 0 }  // position at location 0, binding 0
    };
    // Multi-binding vertex input: binding 0 = per-vertex position, binding 1 = per-instance mat4
    pipelineDesc.vertexInputState.bindings = {
        { 0, sizeof(Vertex),     rhi::VertexInputRate::Vertex   },
        { 1, sizeof(glm::mat4),  rhi::VertexInputRate::Instance },
    };
    pipelineDesc.vertexInputState.attributes = {
        { 0, rhi::Format::R32G32B32_SFLOAT,    0,                  0 },  // position
        { 3, rhi::Format::R32G32B32A32_SFLOAT, sizeof(float) * 0,  1 },  // instanceRow0
        { 4, rhi::Format::R32G32B32A32_SFLOAT, sizeof(float) * 4,  1 },  // instanceRow1
        { 5, rhi::Format::R32G32B32A32_SFLOAT, sizeof(float) * 8,  1 },  // instanceRow2
        { 6, rhi::Format::R32G32B32A32_SFLOAT, sizeof(float) * 12, 1 },  // instanceRow3
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
    pipelineDesc.descriptorSetLayout = m_ShadowDescriptorSet;  // WebGPU requires explicit pipeline layout

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

void Application::CreateInstanceBuffer() {
    using namespace rhi;

    int N = m_EnableInstancing ? m_InstanceGridSize : 1;
    std::vector<glm::mat4> transforms;
    transforms.reserve(N * N);

    if (m_EnableInstancing && N > 1) {
        for (int x = 0; x < N; ++x) {
            for (int z = 0; z < N; ++z) {
                float px = (x - N / 2) * m_InstanceSpacing;
                float pz = (z - N / 2) * m_InstanceSpacing;
                transforms.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(px, 0.0f, pz)));
            }
        }
    } else {
        transforms.push_back(glm::mat4(1.0f));  // Single identity transform
    }

    // Store CPU-side so per-frame frustum culling can filter them
    m_InstanceTransforms = transforms;

    // Size the GPU buffer for the maximum number of instances so per-frame uploads
    // of a culled subset never need to reallocate
    BufferDesc desc{};
    desc.size        = static_cast<uint32>(transforms.size() * sizeof(glm::mat4));
    desc.usage       = BufferUsage::Vertex | BufferUsage::TransferDst;
    desc.memoryUsage = MemoryUsage::CPUToGPU;

    m_InstanceBuffer = m_Device->CreateBuffer(desc);
    if (m_InstanceBuffer) {
        m_InstanceBuffer->CopyData(transforms.data(), desc.size);
        METAGFX_INFO << "Instance buffer created: " << transforms.size() << " transforms";
    }
}

void Application::Run() {
    METAGFX_INFO << "Starting main loop...";
    
    uint64_t lastTime = SDL_GetTicksNS();
    
    while (m_Running) {
        // Calculate delta time
        uint64_t currentTime = SDL_GetTicksNS();
        float deltaTime = (currentTime - lastTime) / 1000000000.0f;
        lastTime = currentTime;
        
        m_FrameStart = std::chrono::steady_clock::now();
        // Reset per-frame counters but preserve frameTimeMs from the previous frame:
        // RenderImGui() is called inside Render() — before the end-of-frame timestamp
        // is taken — so frameTimeMs would always read 0 if we zeroed it here.
        m_Metrics.drawCalls    = 0;
        m_Metrics.triangles    = 0;
        m_Metrics.culledMeshes = 0;

        ProcessEvents();
        Update(deltaTime);
        Render();

        auto frameEnd = std::chrono::steady_clock::now();
        m_Metrics.frameTimeMs = std::chrono::duration<float, std::milli>(frameEnd - m_FrameStart).count();

        // Refresh display values every 500 ms to avoid flickering
        m_DisplayAccumMs += m_Metrics.frameTimeMs;
        m_DisplayFrameCount++;
        if (m_DisplayAccumMs >= 500.0f) {
            m_DisplayFrameTimeMs = m_DisplayAccumMs / m_DisplayFrameCount;
            m_DisplayFps         = 1000.0f / m_DisplayFrameTimeMs;
            m_DisplayAccumMs     = 0.0f;
            m_DisplayFrameCount  = 0;
        }
    }

    METAGFX_INFO << "Main loop ended";
}

void Application::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Let ImGui handle events first
        ImGui_ImplSDL3_ProcessEvent(&event);

        // After ImGui has processed the event, check whether it wants
        // exclusive ownership of mouse / keyboard input.  If so, skip
        // passing the event to the 3D-view handlers below.
        ImGuiIO& io = ImGui::GetIO();

        switch (event.type) {
            case SDL_EVENT_QUIT:
                METAGFX_INFO << "Quit event received";
                m_Running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (io.WantCaptureKeyboard) break;
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
                if (io.WantCaptureMouse) break;
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
                if (io.WantCaptureMouse) break;
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
                if (io.WantCaptureMouse) break;
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

    // Process pending backend switch (deferred from ImGui to avoid mid-render teardown)
    if (m_HasPendingBackendSwitch) {
        m_HasPendingBackendSwitch = false;
        SwitchBackend(m_PendingBackendAPI);
        return;  // Resources recreated; skip the rest of this frame
    }

    // Recreate instance buffer if dirty (deferred from ImGui to avoid destroying a
    // buffer that the previous frame's command buffer is still referencing)
    if (m_InstanceBufferDirty) {
        m_InstanceBufferDirty = false;
        CreateInstanceBuffer();
    }

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

    // Metal uses OpenGL clip space convention (Y-up), while Vulkan requires Y-flip.
    // The Camera flips Y for Vulkan, so we undo it for Metal.
    if (m_Device->GetDeviceInfo().api == rhi::GraphicsAPI::Metal) {
        ubo.projection[1][1] *= -1.0f;
    }

    // Use per-frame uniform buffer for proper double buffering
    m_UniformBuffers[m_CurrentFrame]->CopyData(&ubo, sizeof(ubo));

    // Update light buffer before rendering
    m_Scene->UpdateLightBuffer();

    // Create command buffer
    auto cmd = m_Device->CreateCommandBuffer();

    // Update push constant buffers BEFORE beginning command encoding
    // (WebGPU queue.WriteBuffer must complete before commands execute)
    #ifdef METAGFX_USE_WEBGPU
    {
        struct ModelPushConstants {
            glm::vec4 cameraPosition;
            uint32_t materialFlags;
            float exposure;
            uint32_t enableIBL;
            float iblIntensity;
            uint32_t shadowDebugMode;
            uint32_t enableShadows;
        };

        ModelPushConstants modelPushConst{};
        modelPushConst.cameraPosition = glm::vec4(m_Camera->GetPosition(), 1.0f);
        modelPushConst.materialFlags = 0;
        modelPushConst.exposure = m_Exposure;
        modelPushConst.enableIBL = m_EnableIBL ? 1u : 0u;
        modelPushConst.iblIntensity = m_IBLIntensity;
        modelPushConst.shadowDebugMode = m_ShadowDebugMode;
        modelPushConst.enableShadows = m_EnableShadows ? 1u : 0u;

        // Write the actual data (40 bytes) to the current frame's buffer
        m_ModelPushConstantBuffer[m_CurrentFrame]->CopyData(&modelPushConst, sizeof(ModelPushConstants));
    }
    #endif

    cmd->Begin();

    // Advanced features now working on Metal - all features enabled
    bool debugDisableAdvancedFeatures = false;  // All features enabled: shadows, ground plane, skybox

    // DEBUG: For WebGPU, gradually enable features
    #ifdef METAGFX_USE_WEBGPU
    bool enableWebGPUDrawing = true;  // Clear color works! Now test rendering
    #else
    bool enableWebGPUDrawing = true;
    #endif

    // Add buffer memory barrier to ensure light buffer writes are visible to GPU
    auto lightBuffer = m_Scene->GetLightBuffer();
    if (lightBuffer && !debugDisableAdvancedFeatures) {
        cmd->BufferMemoryBarrier(lightBuffer);
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
        if (debugDisableAdvancedFeatures) {
            METAGFX_INFO << "DEBUG: Advanced features DISABLED for Metal debugging";
        }
        loggedConditions = true;
    }

    if (enableWebGPUDrawing && !debugDisableAdvancedFeatures && m_EnableShadows && m_ShadowMap && m_Model && m_Model->IsValid()) {
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
            // METAGFX_INFO << "Binding SHADOW pipeline (ptr=" << m_ShadowPipeline.get() << ")";
            cmd->BindPipeline(m_ShadowPipeline);

            // Bind shadow descriptor set
            cmd->BindDescriptorSet(m_ShadowPipeline, m_ShadowDescriptorSet, 0);

            // Render all meshes from light's perspective (with instancing)
            uint32 meshesRendered = 0;
            const int instanceCount = m_EnableInstancing
                ? (m_InstanceGridSize * m_InstanceGridSize)
                : 1;

            // Build light-space frustum for culling
            Frustum lightFrustum = Frustum::FromViewProjection(shadowUBO.lightSpaceMatrix);

            // Cull model against light frustum
            bool modelVisible = true;
            if (m_EnableFrustumCulling) {
                modelVisible = lightFrustum.IntersectsSphere(
                    m_Model->GetCenter(), m_Model->GetBoundingSphereRadius() * 1.2f);
            }

            if (modelVisible) {
                for (const auto& mesh : m_Model->GetMeshes()) {
                    if (mesh && mesh->IsValid()) {
                        // Shadow pass always uses LOD0 (full geometry)
                        cmd->BindVertexBuffer(mesh->GetVertexBuffer(), 0);
                        cmd->BindVertexBuffer(m_InstanceBuffer, 1);
                        cmd->BindIndexBuffer(mesh->GetIndexBuffer(0));
                        cmd->DrawIndexed(mesh->GetIndexCount(0), instanceCount);
                        m_Metrics.drawCalls++;
                        meshesRendered++;
                    }
                }
            }

            // NOTE: Do NOT render ground plane in shadow pass!
            // The ground plane should RECEIVE shadows, not CAST them.
            // Rendering it here would write its depth to the shadow map and interfere
            // with shadow calculations.

            cmd->EndRendering();

            // Add pipeline barrier to ensure shadow map writes complete before sampling
#ifdef METAGFX_USE_VULKAN
            // Only execute Vulkan barrier if Vulkan is the active backend
            if (m_Device->GetDeviceInfo().api == GraphicsAPI::Vulkan) {
                auto vkCmd = std::static_pointer_cast<VulkanCommandBuffer>(cmd);
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
#endif
            // Note: Metal handles image layout transitions automatically
        }
    }

    // =============================================================================
    // Main Pass: Render scene with shadow sampling
    // =============================================================================

    // Begin rendering
    ClearValue colorClear{};
    colorClear.color[0] = 0.0f;
    colorClear.color[1] = 0.0f;
    colorClear.color[2] = 0.0f;
    colorClear.color[3] = 1.0f;

    ClearValue depthClear{};
    depthClear.depthStencil.depth = 1.0f;
    depthClear.depthStencil.stencil = 0;

    // Re-enabled depth buffer for Metal testing (Step 2)
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

    // DEBUG: Model rendering test (Step 3)
    static bool loggedModelTest = false;
    if (debugDisableAdvancedFeatures && !loggedModelTest) {
        METAGFX_INFO << "DEBUG: Testing model rendering (shadows/skybox/ground still disabled)";
        loggedModelTest = true;
    }

    // Draw the model FIRST - now enabled for Metal testing
    if (enableWebGPUDrawing && m_Model && m_Model->IsValid()) {
        // Bind model pipeline
        // METAGFX_INFO << "Binding MODEL pipeline (ptr=" << m_ModelPipeline.get() << ")";
        cmd->BindPipeline(m_ModelPipeline);
        // METAGFX_INFO << "Pipeline bound, now binding descriptor set with frameIndex=" << m_CurrentFrame;

        // Per-mesh descriptor sets are bound inside the mesh loop below.
        // (Binding the shared set here would be overridden anyway.)

        // Prepare push constant data structure for all backends
        // The shader uses layout(binding = 14) uniform, so we need this uniform buffer
        // NOTE: Must match shader layout exactly (40 bytes, no padding)
        struct ModelPushConstants {
            glm::vec4 cameraPosition;    // 16 bytes
            uint32_t materialFlags;      // 4 bytes
            float exposure;              // 4 bytes
            uint32_t enableIBL;          // 4 bytes
            float iblIntensity;          // 4 bytes
            uint32_t shadowDebugMode;    // 4 bytes
            uint32_t enableShadows;      // 4 bytes
        };  // Total: 40 bytes
        ModelPushConstants modelPushConst{};
        modelPushConst.cameraPosition = glm::vec4(m_Camera->GetPosition(), 1.0f);
        // Material-specific fields will be updated per-mesh below

        // Push camera position for specular lighting
        glm::vec4 cameraPos(m_Camera->GetPosition(), 1.0f);
        cmd->PushConstants(m_ModelPipeline, ShaderStage::Fragment,
                           0, sizeof(glm::vec4), &cameraPos);

        // Frustum culling + instance count resolution
        //
        // • Instancing ON  → per-instance sphere test; upload only visible transforms.
        //                    culledMeshes counts instances that were removed.
        // • Instancing OFF → single model-level sphere test.
        bool modelVisible = true;
        int  instanceCount = 1;

        if (m_EnableInstancing && !m_InstanceTransforms.empty()) {
            if (m_EnableFrustumCulling) {
                Frustum cameraFrustum = m_Camera->GetFrustum();
                float   sphereRadius  = m_Model->GetBoundingSphereRadius() * 1.2f;
                glm::vec3 modelCenter = m_Model->GetCenter();

                std::vector<glm::mat4> visibleTransforms;
                visibleTransforms.reserve(m_InstanceTransforms.size());
                for (const auto& t : m_InstanceTransforms) {
                    glm::vec3 iCenter = glm::vec3(t * glm::vec4(modelCenter, 1.0f));
                    if (cameraFrustum.IntersectsSphere(iCenter, sphereRadius))
                        visibleTransforms.push_back(t);
                }

                m_Metrics.culledMeshes += static_cast<uint32>(
                    m_InstanceTransforms.size() - visibleTransforms.size());
                instanceCount = static_cast<int>(visibleTransforms.size());

                if (instanceCount > 0)
                    m_InstanceBuffer->CopyData(visibleTransforms.data(),
                                               instanceCount * sizeof(glm::mat4));
                else
                    modelVisible = false;
            } else {
                instanceCount = static_cast<int>(m_InstanceTransforms.size());
            }
        } else {
            // Non-instanced: model-level sphere test
            if (m_EnableFrustumCulling) {
                Frustum cameraFrustum = m_Camera->GetFrustum();
                modelVisible = cameraFrustum.IntersectsSphere(
                    m_Model->GetCenter(), m_Model->GetBoundingSphereRadius() * 1.2f);
                if (!modelVisible)
                    m_Metrics.culledMeshes += static_cast<uint32>(m_Model->GetMeshCount());
            }
        }

        // Draw all meshes in the model
        const auto& allMeshes = m_Model->GetMeshes();
        for (size_t meshIdx = 0; meshIdx < allMeshes.size(); ++meshIdx) {
            const auto& mesh = allMeshes[meshIdx];
            if (!modelVisible) break;
            if (mesh && mesh->IsValid() && mesh->GetMaterial()) {
                Material* material = mesh->GetMaterial();

                // Bind the per-mesh descriptor set so each mesh carries its own material
                // (albedo, roughness, metallic) and texture bindings to the GPU.
                // This avoids the "last material wins" race where a single shared buffer
                // is overwritten once per mesh during recording but read only after all
                // writes are done at GPU execution time.
                if (meshIdx < m_MeshDescriptorSets.size() && m_MeshDescriptorSets[meshIdx][m_CurrentFrame]) {
                    cmd->BindDescriptorSet(m_ModelPipeline, m_MeshDescriptorSets[meshIdx][m_CurrentFrame], 0);
                } else {
                    // Fallback: shared descriptor set (single-material models / WebGPU rebuild)
                    if (m_Config.graphicsAPI == rhi::GraphicsAPI::WebGPU) {
                        UpdateModelDescriptorTextures(material);
                    }
                    // Also update the shared material buffer for the fallback path
                    MaterialProperties matProps = material->GetProperties();
                    m_MaterialBuffers[m_CurrentFrame]->CopyData(&matProps, sizeof(matProps));
                    cmd->BindDescriptorSet(m_ModelPipeline, m_DescriptorSet[m_CurrentFrame], 0);
                }

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
                cmd->PushConstants(m_ModelPipeline, ShaderStage::Fragment,
                                   16, sizeof(uint32_t), &flags);

                // Push exposure (offset 20, size 4)
                cmd->PushConstants(m_ModelPipeline, ShaderStage::Fragment,
                                   20, sizeof(float), &m_Exposure);

                // Push IBL enable flag (offset 24, size 4)
                uint32_t enableIBL = m_EnableIBL ? 1u : 0u;
                cmd->PushConstants(m_ModelPipeline, ShaderStage::Fragment,
                                   24, sizeof(uint32_t), &enableIBL);

                // Push IBL intensity (offset 28, size 4)
                cmd->PushConstants(m_ModelPipeline, ShaderStage::Fragment,
                                   28, sizeof(float), &m_IBLIntensity);

                // Push shadow debug mode (offset 32, size 4)
                uint32_t shadowDebugMode = static_cast<uint32_t>(m_ShadowDebugMode);

                // Debug: Log shadow debug mode on first frame
                static bool loggedDebugMode = false;
                if (!loggedDebugMode) {
                    METAGFX_INFO << "Shadow debug mode being pushed to shader: " << shadowDebugMode;
                    loggedDebugMode = true;
                }

                cmd->PushConstants(m_ModelPipeline, ShaderStage::Fragment,
                                   32, sizeof(uint32_t), &shadowDebugMode);

                // Push shadow enable flag (offset 36, size 4)
                uint32_t enableShadows = m_EnableShadows ? 1u : 0u;

                // Debug: Log shadow enable state on first frame
                static bool loggedShadowState = false;
                if (!loggedShadowState) {
                    METAGFX_INFO << "Shadow enable flag being pushed to shader: " << enableShadows;
                    loggedShadowState = true;
                }

                cmd->PushConstants(m_ModelPipeline, ShaderStage::Fragment,
                                   36, sizeof(uint32_t), &enableShadows);

                // Update push constant uniform buffer for all backends
                // The shader now uses layout(binding = 14) uniform instead of layout(push_constant)
                // so we need to update the uniform buffer, not just call PushConstants()
                modelPushConst.materialFlags = flags;
                modelPushConst.exposure = m_Exposure;
                modelPushConst.enableIBL = enableIBL;
                modelPushConst.iblIntensity = m_IBLIntensity;
                modelPushConst.shadowDebugMode = shadowDebugMode;
                modelPushConst.enableShadows = enableShadows;

                m_ModelPushConstantBuffer[m_CurrentFrame]->CopyData(&modelPushConst, sizeof(ModelPushConstants));

                #ifdef METAGFX_USE_WEBGPU
                // Re-bind after updating the push constant buffer (WebGPU requires this)
                if (meshIdx < m_MeshDescriptorSets.size() && m_MeshDescriptorSets[meshIdx][m_CurrentFrame]) {
                    cmd->BindDescriptorSet(m_ModelPipeline, m_MeshDescriptorSets[meshIdx][m_CurrentFrame], 0);
                } else {
                    cmd->BindDescriptorSet(m_ModelPipeline, m_DescriptorSet[m_CurrentFrame], 0);
                }
                #endif

                // LOD selection based on camera distance
                int lod = 0;
                if (m_EnableLOD) {
                    float dist = glm::length(m_Camera->GetPosition() - m_Model->GetCenter());
                    if (dist > m_LOD2Distance) lod = 2;
                    else if (dist > m_LOD1Distance) lod = 1;
                }

                cmd->BindVertexBuffer(mesh->GetVertexBuffer(), 0);
                cmd->BindVertexBuffer(m_InstanceBuffer, 1);
                cmd->BindIndexBuffer(mesh->GetIndexBuffer(lod));
                cmd->DrawIndexed(mesh->GetIndexCount(lod), instanceCount);

                m_Metrics.drawCalls++;
                m_Metrics.triangles += mesh->GetIndexCount(lod) / 3 * instanceCount;
            }
        }
    }

    // Render ground plane with simple grey material (if enabled)
    if (!debugDisableAdvancedFeatures && m_ShowGroundPlane && m_GroundPlane && m_GroundPlane->IsValid()) {
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
        cmd->BindDescriptorSet(m_ModelPipeline, m_GroundPlaneDescriptorSet[m_CurrentFrame], 0);

        // Push material flags (no textures)
        uint32_t flags = 0;
        cmd->PushConstants(m_ModelPipeline, ShaderStage::Fragment, 16, sizeof(uint32_t), &flags);

        // Draw ground plane (non-instanced: bind identity matrix to slot 1 so Vulkan is satisfied)
        if (enableWebGPUDrawing) {
            for (const auto& mesh : m_GroundPlane->GetMeshes()) {
                if (mesh && mesh->IsValid()) {
                    cmd->BindVertexBuffer(mesh->GetVertexBuffer(), 0);
                    cmd->BindVertexBuffer(m_SingleInstanceBuffer, 1);
                    cmd->BindIndexBuffer(mesh->GetIndexBuffer());
                    cmd->DrawIndexed(mesh->GetIndexCount(), 1);
                    m_Metrics.drawCalls++;
                }
            }
        }
    }

    // Render skybox LAST (only where depth >= model depth)
    if (enableWebGPUDrawing && !debugDisableAdvancedFeatures && m_ShowSkybox && m_EnvironmentMap && m_SkyboxPipeline && m_SkyboxVertexBuffer && m_SkyboxIndexBuffer && m_SkyboxDescriptorSet[m_CurrentFrame]) {
        cmd->BindPipeline(m_SkyboxPipeline);

        // Bind skybox descriptor set (binding 0: MVP, binding 1: environment cubemap)
        // Each frame's descriptor set already points to the correct per-frame uniform buffer
        cmd->BindDescriptorSet(m_SkyboxPipeline, m_SkyboxDescriptorSet[m_CurrentFrame], 0);

        // Push constants: exposure and LOD
        struct SkyboxPushConstants {
            float exposure;
            float lod;
        } skyboxPushConstants;

        skyboxPushConstants.exposure = m_Exposure;
        skyboxPushConstants.lod = m_SkyboxLOD;

        cmd->PushConstants(m_SkyboxPipeline, ShaderStage::Fragment,
                           0, sizeof(SkyboxPushConstants), &skyboxPushConstants);

        // For WebGPU, update the push constant buffer (binding 3)
        #ifdef METAGFX_USE_WEBGPU
        m_SkyboxPushConstantBuffer->CopyData(&skyboxPushConstants, sizeof(SkyboxPushConstants));
        // Re-bind descriptor set to ensure push constant buffer is bound
        cmd->BindDescriptorSet(m_SkyboxPipeline, m_SkyboxDescriptorSet[m_CurrentFrame], 0);
        #endif

        // Draw the skybox cube
        cmd->BindVertexBuffer(m_SkyboxVertexBuffer);
        cmd->BindIndexBuffer(m_SkyboxIndexBuffer);
        cmd->DrawIndexed(36);  // 36 indices for the cube
        m_Metrics.drawCalls++;
    }

    // ImGui rendering order depends on backend:
    // - Metal/WebGPU: ImGui must be rendered within the main render pass (BEFORE EndRendering)
    // - Vulkan: ImGui uses its own separate render pass (AFTER EndRendering)
    auto api = m_Device->GetDeviceInfo().api;

    if (api == GraphicsAPI::Metal || api == GraphicsAPI::WebGPU) {
        // Render ImGui within the active render pass for Metal/WebGPU
        RenderImGui(cmd, backBuffer);
        cmd->EndRendering();
    } else {
        // For Vulkan: end main render pass first, then render ImGui in separate pass
        cmd->EndRendering();
        RenderImGui(cmd, backBuffer);
    }

    cmd->End();

    // Submit command buffer (contains both main rendering and ImGui)
    m_Device->SubmitCommandBuffer(cmd);

    // Present (advances VulkanSwapChain's internal frame counter)
    swapChain->Present();

    // Synchronize Application's frame counter with swap chain's frame counter
    // This ensures buffer/descriptor set selection matches the sync objects
    if (api == GraphicsAPI::Vulkan) {
        auto vkSwapChain = std::static_pointer_cast<rhi::VulkanSwapChain>(swapChain);
        m_CurrentFrame = vkSwapChain->GetCurrentFrame();
    } else {
        // For Metal and WebGPU, advance normally
        m_CurrentFrame = (m_CurrentFrame + 1) % 2;
    }
}

void Application::ShutdownGPUResources() {
    if (m_Device) {
        m_Device->WaitIdle();
    }

    // Force drain deferred deletion queue before tearing down the device
    for (auto& pending : m_DeletionQueue) {
        if (pending.model) {
            pending.model->Cleanup();
        }
    }
    m_DeletionQueue.clear();

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
    m_ModelPushConstantBuffer[0].reset();
    m_ModelPushConstantBuffer[1].reset();
    m_SkyboxPushConstantBuffer.reset();
    m_InstanceBuffer.reset();
    m_SingleInstanceBuffer.reset();

    // Clean up per-mesh resources
    m_MeshMaterialBuffers.clear();
    m_MeshDescriptorSets.clear();

    // Clean up descriptor sets
    m_DescriptorSet[0].reset();
    m_DescriptorSet[1].reset();
    m_SkyboxDescriptorSet[0].reset();
    m_SkyboxDescriptorSet[1].reset();
    m_ShadowDescriptorSet.reset();
    m_GroundPlaneDescriptorSet[0].reset();
    m_GroundPlaneDescriptorSet[1].reset();

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

    // Destroy device last (after all resources that depend on it)
    m_Device.reset();
}

void Application::Shutdown() {
    ShutdownGPUResources();

    if (m_Window) {
        METAGFX_INFO << "Destroying window...";
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }

    METAGFX_INFO << "Shutting down SDL...";
    SDL_Quit();
}

void Application::SwitchBackend(rhi::GraphicsAPI newAPI) {
    if (newAPI == m_Config.graphicsAPI) return;

    const char* newApiName = "Unknown";
    switch (newAPI) {
        case rhi::GraphicsAPI::Vulkan: newApiName = "Vulkan"; break;
        case rhi::GraphicsAPI::Direct3D12: newApiName = "D3D12"; break;
        case rhi::GraphicsAPI::Metal: newApiName = "Metal"; break;
        case rhi::GraphicsAPI::WebGPU: newApiName = "WebGPU"; break;
    }
    METAGFX_INFO << "Switching backend to " << newApiName;

    // m_CurrentModelIndex and m_AvailableModels are CPU-only — preserved through the switch

    // Tear down all GPU resources (WaitIdle + all resources + device)
    ShutdownGPUResources();

    // Destroy old window (required — Vulkan needs SDL_WINDOW_VULKAN, Metal needs SDL_WINDOW_METAL)
    if (m_Window) {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }

    // Update config and create new window for the new backend
    m_Config.graphicsAPI = newAPI;
    InitWindow(newAPI);
    if (!m_Window) {
        METAGFX_CRITICAL << "Failed to create window for new backend — stopping";
        m_Running = false;
        return;
    }

    // Rebuild all GPU resources (device, buffers, textures, pipelines, ImGui, loads model)
    // m_CurrentModelIndex is still set to the model that was active before the switch
    InitGPUResources();

    METAGFX_INFO << "Backend switch to " << newApiName << " complete";
}

void Application::InitImGui() {
    auto api = m_Device->GetDeviceInfo().api;

    // Setup ImGui context (common for all backends)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

#ifdef METAGFX_USE_METAL
    if (api == rhi::GraphicsAPI::Metal) {
        // Initialize ImGui for Metal backend
        auto metalDevice = std::static_pointer_cast<rhi::MetalDevice>(m_Device);
        auto& context = metalDevice->GetContext();

        ImGui_ImplSDL3_InitForMetal(m_Window);
        ImGui_ImplMetal_Init(context.device);

        METAGFX_INFO << "ImGui initialized for Metal backend";
        return;
    }
#endif

#ifdef METAGFX_USE_WEBGPU
    if (api == rhi::GraphicsAPI::WebGPU) {
        // Initialize ImGui for WebGPU backend
        auto webgpuDevice = std::static_pointer_cast<rhi::WebGPUDevice>(m_Device);
        auto& context = webgpuDevice->GetContext();

        ImGui_ImplSDL3_InitForOther(m_Window);

        ImGui_ImplWGPU_InitInfo initInfo{};
        initInfo.Device = context.device.Get();
        initInfo.NumFramesInFlight = 2;
        initInfo.RenderTargetFormat = WGPUTextureFormat_BGRA8Unorm;
        initInfo.DepthStencilFormat = WGPUTextureFormat_Depth32Float;

        ImGui_ImplWGPU_Init(&initInfo);

        METAGFX_INFO << "ImGui initialized for WebGPU backend";
        return;
    }
#endif

    if (api != rhi::GraphicsAPI::Vulkan) {
        METAGFX_INFO << "ImGui not supported for this backend yet";
        ImGui::DestroyContext();
        return;
    }

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
    // Wait for previous render pass color writes to complete before ImGui's load (read) and writes
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

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
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = context.instance;
    initInfo.PhysicalDevice = context.physicalDevice;
    initInfo.Device = context.device;
    initInfo.QueueFamily = context.graphicsQueueFamily;
    initInfo.Queue = context.graphicsQueue;
    initInfo.DescriptorPool = m_ImGuiDescriptorPool;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.PipelineCache = VK_NULL_HANDLE;

    // New in ImGui v1.92.x: Pipeline info moved to separate structure
    initInfo.PipelineInfoMain.RenderPass = m_ImGuiRenderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    initInfo.UseDynamicRendering = false;  // We're using render pass, not dynamic rendering

    ImGui_ImplVulkan_Init(&initInfo);

    // Wait for font upload to complete
    vkDeviceWaitIdle(context.device);

    // Initialize framebuffers vector (created lazily during rendering)
    // Need to match the swap chain image count (typically 2 or 3)
    m_ImGuiFramebuffers.resize(imageCount, VK_NULL_HANDLE);
}

void Application::ShutdownImGui() {
    if (!m_Device) return;

    auto api = m_Device->GetDeviceInfo().api;

#ifdef METAGFX_USE_METAL
    if (api == rhi::GraphicsAPI::Metal) {
        ImGui_ImplMetal_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return;
    }
#endif

#ifdef METAGFX_USE_WEBGPU
    if (api == rhi::GraphicsAPI::WebGPU) {
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return;
    }
#endif

    if (api != rhi::GraphicsAPI::Vulkan) {
        return;
    }

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
    if (!m_Device || !cmd || !backBuffer) {
        return;
    }

    auto api = m_Device->GetDeviceInfo().api;

    // Start ImGui frame
    ImGui_ImplSDL3_NewFrame();

#ifdef METAGFX_USE_METAL
    if (api == rhi::GraphicsAPI::Metal) {
        // Metal backend needs NewFrame with render pass descriptor
        auto metalCmd = std::static_pointer_cast<rhi::MetalCommandBuffer>(cmd);
        auto encoder = metalCmd->GetRenderEncoder();
        if (!encoder) {
            return;  // No active render encoder
        }

        // Create render pass descriptor matching our current render target
        MTL::RenderPassDescriptor* renderPassDesc = MTL::RenderPassDescriptor::alloc()->init();

        // Set up color attachment
        auto colorAttachment = renderPassDesc->colorAttachments()->object(0);
        auto metalTexture = std::static_pointer_cast<rhi::MetalTexture>(backBuffer);
        colorAttachment->setTexture(metalTexture->GetHandle());
        colorAttachment->setLoadAction(MTL::LoadActionLoad);  // Keep existing content
        colorAttachment->setStoreAction(MTL::StoreActionStore);

        ImGui_ImplMetal_NewFrame(renderPassDesc);
        renderPassDesc->release();

        static bool logged = false;
        if (!logged) {
            METAGFX_INFO << "ImGui Metal NewFrame called";
            logged = true;
        }
    }
#endif

#ifdef METAGFX_USE_WEBGPU
    if (api == rhi::GraphicsAPI::WebGPU) {
        ImGui_ImplWGPU_NewFrame();
    }
#endif

    ImGui::NewFrame();

    // Define UI
    // METAGFX_INFO << "RenderImGui: Creating UI";
    ImGui::SetNextWindowSize(ImVec2(360, 350), ImGuiCond_FirstUseEver);
    ImGui::Begin("MetaGFX Controls");

    // ── Graphics Backend ─────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Graphics Backend", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Display current backend info
        const auto& deviceInfo = m_Device->GetDeviceInfo();
        const char* currentApiName = "Unknown";
        switch (deviceInfo.api) {
            case rhi::GraphicsAPI::Vulkan: currentApiName = "Vulkan"; break;
            case rhi::GraphicsAPI::Direct3D12: currentApiName = "D3D12"; break;
            case rhi::GraphicsAPI::Metal: currentApiName = "Metal"; break;
            case rhi::GraphicsAPI::WebGPU: currentApiName = "WebGPU"; break;
        }
        ImGui::Text("API: %s", currentApiName);
        ImGui::Text("Device: %s", deviceInfo.deviceName.c_str());

        // Backend selection combo — switches live without restart
        // Keep selectedBackend in sync with current API (handles external changes)
        int selectedBackend = static_cast<int>(m_Config.graphicsAPI);
        const char* backendNames[] = { "Vulkan", "D3D12", "Metal", "WebGPU" };

        // Determine which backends are available
        bool vulkanAvailable = false;
        bool metalAvailable = false;
        bool webgpuAvailable = false;
#ifdef METAGFX_USE_VULKAN
        vulkanAvailable = true;
#endif
#ifdef METAGFX_USE_METAL
        metalAvailable = true;
#endif
#ifdef METAGFX_USE_WEBGPU
        webgpuAvailable = true;
#endif

        ImGui::Spacing();
        ImGui::Text("Change Backend:");

        // Show combo with only available backends
        int previousBackend = selectedBackend;
        if (ImGui::BeginCombo("##backend", backendNames[selectedBackend])) {
            if (vulkanAvailable) {
                bool isSelected = (selectedBackend == 0);
                if (ImGui::Selectable("Vulkan", isSelected)) {
                    selectedBackend = 0;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            // D3D12 - index 1 (Windows only, not implemented yet)
            if (metalAvailable) {
                bool isSelected = (selectedBackend == 2);
                if (ImGui::Selectable("Metal", isSelected)) {
                    selectedBackend = 2;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            if (webgpuAvailable) {
                bool isSelected = (selectedBackend == 3);
                if (ImGui::Selectable("WebGPU", isSelected)) {
                    selectedBackend = 3;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Trigger backend switch if selection changed
        if (selectedBackend != previousBackend) {
            rhi::GraphicsAPI targetAPI = rhi::GraphicsAPI::Vulkan;
            switch (selectedBackend) {
                case 0: targetAPI = rhi::GraphicsAPI::Vulkan; break;
                case 1: targetAPI = rhi::GraphicsAPI::Direct3D12; break;
                case 2: targetAPI = rhi::GraphicsAPI::Metal; break;
                case 3: targetAPI = rhi::GraphicsAPI::WebGPU; break;
            }
            m_PendingBackendAPI = targetAPI;
            m_HasPendingBackendSwitch = true;
        }
    }
    ImGui::Spacing();

    // ── Rendering Controls ────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Rendering Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Model selection
        const char* modelNames[] = {
            "Antique Camera",
            "Bunny",
            "Damaged Helmet",
            "Metal Rough Spheres",
            "Cornell Box (PBRT)",
            "Cornell Box Bitterli",
            "Contemporary Bathroom",
            "Milestone 5.2 Test",
            "Classroom"
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
    }
    ImGui::Spacing();

    // ── Performance Metrics ──────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Performance")) {
        ImGui::Text("Frame Time:   %.2f ms  (%.0f FPS)", m_DisplayFrameTimeMs, m_DisplayFps);
        ImGui::Text("Draw Calls:   %u", m_Metrics.drawCalls);
        ImGui::Text("Triangles:    %s", [&]() -> std::string {
            char buf[32];
            uint32 t = m_Metrics.triangles;
            if (t >= 1000000) snprintf(buf, sizeof(buf), "%.2f M", t / 1000000.0f);
            else if (t >= 1000) snprintf(buf, sizeof(buf), "%.1f K", t / 1000.0f);
            else snprintf(buf, sizeof(buf), "%u", t);
            return buf;
        }().c_str());
        ImGui::Text("Culled:       %u meshes", m_Metrics.culledMeshes);
    }
    ImGui::Spacing();

    // ── Rendering Optimizations ──────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Optimizations")) {
        // Frustum culling
        ImGui::Checkbox("Frustum Culling", &m_EnableFrustumCulling);

        ImGui::Spacing();

        // LOD
        ImGui::Checkbox("Enable LOD", &m_EnableLOD);
        if (m_EnableLOD) {
            ImGui::SliderFloat("LOD1 Distance", &m_LOD1Distance, 1.0f, 50.0f, "%.1f m");
            ImGui::SliderFloat("LOD2 Distance", &m_LOD2Distance, m_LOD1Distance + 1.0f, 200.0f, "%.1f m");
        }

        ImGui::Spacing();

        // Instancing
        bool instancingChanged = false;
        instancingChanged |= ImGui::Checkbox("Instancing Grid", &m_EnableInstancing);
        if (m_EnableInstancing) {
            instancingChanged |= ImGui::SliderInt("Grid Size", &m_InstanceGridSize, 1, 7);
            instancingChanged |= ImGui::SliderFloat("Spacing", &m_InstanceSpacing, 1.0f, 10.0f, "%.1f m");
        }
        if (instancingChanged) {
            // Do NOT call CreateInstanceBuffer() here — the current frame's commands are
            // already recorded with the old buffer. Destroying it mid-frame causes
            // VK_ERROR_DEVICE_LOST. Defer to the start of the next frame instead.
            m_InstanceBufferDirty = true;
        }
    }
    ImGui::Spacing();

    // ── Image-Based Lighting ──────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Image-Based Lighting")) {
        ImGui::Checkbox("Enable IBL", &m_EnableIBL);

        if (m_EnableIBL) {
            ImGui::SliderFloat("IBL Intensity", &m_IBLIntensity, 0.0f, 2.0f);
        }
    }
    ImGui::Spacing();

    // ── Skybox ────────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Skybox")) {
        ImGui::Checkbox("Show Skybox", &m_ShowSkybox);

        if (m_ShowSkybox) {
            ImGui::SliderFloat("Skybox Blur", &m_SkyboxLOD, 0.0f, 5.0f);
        }
    }
    ImGui::Spacing();

    // ── Shadow Mapping ────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Shadow Mapping")) {
        if (ImGui::Checkbox("Enable Shadows", &m_EnableShadows)) {
            METAGFX_INFO << "Shadow enabled state changed to: " << (m_EnableShadows ? "ENABLED" : "DISABLED");
        }

        ImGui::Checkbox("Show Ground Plane", &m_ShowGroundPlane);

        if (m_EnableShadows) {
            ImGui::Spacing();

            if (ImGui::SliderFloat("Shadow Bias", &m_ShadowBias, 0.0f, 0.01f, "%.5f")) {
                // Shadow bias will be updated in the next frame's render pass
            }

            ImGui::Text("Light Direction:");
            ImGui::SliderFloat("Light X", &m_LightDirection.x, -2.0f, 2.0f);
            ImGui::SliderFloat("Light Y", &m_LightDirection.y, -2.0f, 2.0f);
            ImGui::SliderFloat("Light Z", &m_LightDirection.z, -2.0f, 2.0f);
            if (ImGui::Button("Reset Light Direction")) {
                m_LightDirection = glm::vec3(0.5f, -1.0f, -0.3f);
            }

            ImGui::Spacing();

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

        }
    }

    // Demo window toggle
    ImGui::Spacing();
    ImGui::Checkbox("Show Demo Window", &m_ShowDemoWindow);

    ImGui::End();

    // Show demo window if enabled
    if (m_ShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }

    // Render ImGui
    ImGui::Render();

    // Check if there's anything to draw
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->TotalVtxCount == 0) {
        return;  // Nothing to draw
    }

    // Backend-specific rendering
#ifdef METAGFX_USE_METAL
    if (api == rhi::GraphicsAPI::Metal) {
        // Metal: Render directly to the current render encoder
        auto metalCmd = std::static_pointer_cast<rhi::MetalCommandBuffer>(cmd);

        static bool logged = false;
        if (!logged) {
            METAGFX_INFO << "ImGui Metal RenderDrawData: vertices=" << drawData->TotalVtxCount;
            logged = true;
        }

        ImGui_ImplMetal_RenderDrawData(drawData,
                                       metalCmd->GetHandle(),
                                       metalCmd->GetRenderEncoder());
        return;
    }
#endif

#ifdef METAGFX_USE_WEBGPU
    if (api == rhi::GraphicsAPI::WebGPU) {
        // WebGPU: Render to the active render pass encoder
        // ImGui must render within the existing render pass (WebGPU doesn't support nested passes)
        auto webgpuCmd = std::static_pointer_cast<rhi::WebGPUCommandBuffer>(cmd);
        WGPURenderPassEncoder passEncoder = webgpuCmd->GetRenderPassEncoder().Get();

        if (!passEncoder) {
            METAGFX_ERROR << "RenderImGui: No active render pass encoder";
            return;
        }

        ImGui_ImplWGPU_RenderDrawData(drawData, passEncoder);
        return;
    }
#endif

    if (api != rhi::GraphicsAPI::Vulkan) {
        return;
    }

    // Vulkan rendering
    auto vkDevice = std::static_pointer_cast<rhi::VulkanDevice>(m_Device);
    auto& context = vkDevice->GetContext();
    auto swapChain = m_Device->GetSwapChain();
    auto vkSwapChain = std::static_pointer_cast<rhi::VulkanSwapChain>(swapChain);
    if (!vkSwapChain) {
        return;
    }
    uint32_t imageIndex = vkSwapChain->GetCurrentImageIndex();

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

    // The main render pass finalLayout = PRESENT_SRC_KHR, so the image is in that
    // layout when we reach here. The ImGui render pass has initialLayout = PRESENT_SRC_KHR,
    // so it handles the transition to COLOR_ATTACHMENT_OPTIMAL internally. No manual barrier needed.

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

