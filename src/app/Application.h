// ============================================================================
// src/app/Application.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/Pipeline.h"
#include "metagfx/scene/Camera.h"
#include "metagfx/scene/Model.h"
#include "metagfx/scene/Scene.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace metagfx {

// Forward declarations
namespace rhi {
    class GraphicsDevice;
    class Buffer;
    class Pipeline;
    class Texture;
    class Sampler;
    class VulkanDescriptorSet;
}

class ShadowMap;

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
    void CreateModelPipeline();
    void CreateSkyboxPipeline();
    void CreateShadowPipeline();
    void CreateSkyboxCube();
    void CreateTestLights();
    void CreateGroundPlane();
    void UpdateGroundPlanePosition();
    void LoadModel(const std::string& path);
    void LoadNextModel();
    void LoadPreviousModel();
    void ProcessEvents();
    void Update(float deltaTime);
    void Render();

    // ImGui
    void InitImGui();
    void ShutdownImGui();
    void RenderImGui(Ref<rhi::CommandBuffer> cmd, Ref<rhi::Texture> backBuffer);

    ApplicationConfig m_Config;
    SDL_Window* m_Window = nullptr;
    bool m_Running = false;
    
    // Graphics resources
    Ref<rhi::GraphicsDevice> m_Device;
    Ref<rhi::Buffer> m_VertexBuffer;
    Ref<rhi::Pipeline> m_Pipeline;
    Ref<rhi::Pipeline> m_ModelPipeline;
    Ref<rhi::Pipeline> m_SkyboxPipeline;  // Pipeline for skybox rendering
    Ref<rhi::Pipeline> m_ShadowPipeline;  // Pipeline for shadow map rendering
    Ref<rhi::Buffer> m_SkyboxVertexBuffer;  // Cube vertices for skybox
    Ref<rhi::Buffer> m_SkyboxIndexBuffer;   // Cube indices for skybox

    // Camera
    std::unique_ptr<Camera> m_Camera;
    bool m_FirstMouse = true;
    float m_LastX = 640.0f;
    float m_LastY = 360.0f;
    bool m_CameraEnabled = false;  // Disabled by default
    bool m_MouseButtonPressed = false;  // Track mouse button state for click-and-drag
    
    // Uniform buffers
    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };
    
    Ref<rhi::Buffer> m_UniformBuffers[2];  // Double buffering for MVP
    Ref<rhi::Buffer> m_MaterialBuffers[2];  // Double buffering for material
    Ref<rhi::Buffer> m_GroundPlaneMaterialBuffer;  // Dedicated material buffer for ground plane
    Ref<rhi::Buffer> m_ShadowUniformBuffer;  // Shadow UBO (light space matrix + bias)
    std::unique_ptr<rhi::VulkanDescriptorSet> m_DescriptorSet;
    std::unique_ptr<rhi::VulkanDescriptorSet> m_SkyboxDescriptorSet;  // Separate descriptor set for skybox
    std::unique_ptr<rhi::VulkanDescriptorSet> m_ShadowDescriptorSet;  // Descriptor set for shadow pass
    std::unique_ptr<rhi::VulkanDescriptorSet> m_GroundPlaneDescriptorSet;  // Separate descriptor set for ground plane
    uint32 m_CurrentFrame = 0;

    // Texture resources
    Ref<rhi::Sampler> m_LinearRepeatSampler;
    Ref<rhi::Texture> m_DefaultTexture;  // Checker pattern for albedo
    Ref<rhi::Texture> m_DefaultNormalMap;  // Flat normal map (128,128,255)
    Ref<rhi::Texture> m_DefaultWhiteTexture;  // White 1x1 for metallic/roughness/AO
    Ref<rhi::Texture> m_DefaultBlackTexture;  // Black 1x1 for emissive (no emission)
    Ref<rhi::Texture> m_DepthBuffer;  // Depth buffer for 3D rendering

    // IBL (Image-Based Lighting) resources
    Ref<rhi::Sampler> m_CubemapSampler;  // Linear filtering for cubemaps
    Ref<rhi::Texture> m_IrradianceMap;   // Diffuse irradiance cubemap
    Ref<rhi::Texture> m_PrefilteredMap;  // Specular prefiltered cubemap
    Ref<rhi::Texture> m_BRDF_LUT;        // BRDF integration lookup table
    Ref<rhi::Texture> m_EnvironmentMap;  // Full-resolution environment map for skybox

    // Scene and model
    std::unique_ptr<Scene> m_Scene;
    std::unique_ptr<Model> m_Model;
    std::unique_ptr<Model> m_GroundPlane;  // Ground plane to visualize shadows

    // Shadow mapping
    std::unique_ptr<ShadowMap> m_ShadowMap;
    bool m_EnableShadows = true;
    float m_ShadowBias = 0.005f;
    bool m_VisualizeShadowMap = false;  // Debug: Show shadow map directly
    int m_ShadowDebugMode = 0;  // 0=normal, 1=shadow factor, 2=depth coords
    bool m_ShowGroundPlane = true;  // Show/hide ground plane
    glm::vec3 m_LightDirection = glm::vec3(0.5f, -1.0f, -0.3f);  // Direction for main shadow-casting light

    // Model management
    std::vector<std::string> m_AvailableModels;
    int m_CurrentModelIndex = 0;
    std::string m_PendingModelPath;  // Model to load at start of next frame
    bool m_HasPendingModel = false;

    // Deferred deletion queue for old models
    struct PendingDeletion {
        std::unique_ptr<Model> model;
        uint32 frameCount;  // Frames to wait before deletion
    };
    std::vector<PendingDeletion> m_DeletionQueue;

    // ImGui state
    VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
    VkRenderPass m_ImGuiRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_ImGuiFramebuffers;  // One per swap chain image

    // GUI parameters
    float m_Exposure = 1.0f;
    bool m_EnableIBL = false;  // Disable IBL by default for shadow visualization
    float m_IBLIntensity = 0.05f;  // IBL contribution multiplier (default: very subtle)
    bool m_ShowSkybox = false;  // Hide skybox by default for shadow visualization
    float m_SkyboxLOD = 0.0f;  // Skybox mipmap LOD (0 = sharp, higher = blurred)
    bool m_ShowDemoWindow = false;
};

} // namespace metagfx