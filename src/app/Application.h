// ============================================================================
// src/app/Application.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/rhi/Pipeline.h"
#include "metagfx/rhi/Types.h"
#include "metagfx/scene/Camera.h"
#include "metagfx/scene/Model.h"
#include "metagfx/scene/Scene.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#ifdef METAGFX_USE_VULKAN
#include <vulkan/vulkan.h>
#endif
#include <chrono>
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
    class DescriptorSet;
}
class Material;
class ShadowMap;

struct ApplicationConfig {
    std::string title = "MetaGFX";
    uint32 width = 1280;
    uint32 height = 720;
    bool vsync = true;
    rhi::GraphicsAPI graphicsAPI = rhi::GraphicsAPI::Vulkan;  // Default to Vulkan
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
    void UpdateModelDescriptorTextures(Material* material);
    void ProcessEvents();
    void Update(float deltaTime);
    void Render();

    // ImGui
    void InitImGui();
    void ShutdownImGui();
    void RenderImGui(Ref<rhi::CommandBuffer> cmd, Ref<rhi::Texture> backBuffer);

    // Backend switching helpers
    void InitWindow(rhi::GraphicsAPI api);
    void InitGPUResources();
    void ShutdownGPUResources();
    void SwitchBackend(rhi::GraphicsAPI newAPI);

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

    // Push constant buffers (WebGPU requires these as uniform buffers)
    // Double-buffered to prevent race conditions: CPU writes frame N while GPU reads frame N-1
    Ref<rhi::Buffer> m_ModelPushConstantBuffer[2];  // Push constants for model shader (per-frame)
    Ref<rhi::Buffer> m_SkyboxPushConstantBuffer;    // Push constants for skybox shader
    Ref<rhi::DescriptorSet> m_DescriptorSet[2];  // Double-buffered (frame 0/1 use different uniform buffers)
    Ref<rhi::DescriptorSet> m_SkyboxDescriptorSet[2];  // Double-buffered for skybox
    Ref<rhi::DescriptorSet> m_ShadowDescriptorSet;  // Shadow pass (no double buffering needed)
    Ref<rhi::DescriptorSet> m_GroundPlaneDescriptorSet[2];  // Double-buffered for ground plane
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

    // ImGui state (Vulkan-specific; Metal and WebGPU use different mechanisms)
#ifdef METAGFX_USE_VULKAN
    VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
    VkRenderPass m_ImGuiRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_ImGuiFramebuffers;  // One per swap chain image
#endif

    // Pending backend switch (deferred to start of next frame to avoid mid-render teardown)
    bool m_HasPendingBackendSwitch = false;
    rhi::GraphicsAPI m_PendingBackendAPI = rhi::GraphicsAPI::Vulkan;

    // ── Feature 1: Frustum culling ──────────────────────────────────────────
    bool m_EnableFrustumCulling = true;

    // ── Feature 2: LOD ──────────────────────────────────────────────────────
    bool  m_EnableLOD      = true;
    float m_LOD1Distance   = 5.0f;
    float m_LOD2Distance   = 20.0f;

    // ── Feature 3: Instanced rendering ─────────────────────────────────────
    bool              m_EnableInstancing = false;
    int               m_InstanceGridSize = 3;     // N×N grid
    float             m_InstanceSpacing  = 2.0f;
    Ref<rhi::Buffer>         m_InstanceBuffer;            // N×N mat4 transforms (sized for max instances)
    Ref<rhi::Buffer>         m_SingleInstanceBuffer;      // 1 identity mat4 — always bound to slot 1 for non-instanced draws
    bool                     m_InstanceBufferDirty = false;  // Recreate at start of next frame (not mid-frame)
    std::vector<glm::mat4>   m_InstanceTransforms;        // CPU-side full set of instance transforms

    void CreateInstanceBuffer();  // Create / recreate the instance buffer

    // ── Feature 4: Performance metrics ─────────────────────────────────────
    struct FrameMetrics {
        uint32 drawCalls     = 0;
        uint32 triangles     = 0;
        uint32 culledMeshes  = 0;
        float  frameTimeMs   = 0.0f;
    };
    FrameMetrics m_Metrics{};
    std::chrono::steady_clock::time_point m_FrameStart;

    // Smoothed display values — updated every 500 ms to avoid flickering
    float  m_DisplayFrameTimeMs = 0.0f;
    float  m_DisplayFps         = 0.0f;
    float  m_DisplayAccumMs     = 0.0f;
    int    m_DisplayFrameCount  = 0;

    // GUI parameters
    float m_Exposure = 1.0f;
    bool m_EnableIBL = false;  // Disable IBL by default for shadow visualization
    float m_IBLIntensity = 0.05f;  // IBL contribution multiplier (default: very subtle)
    bool m_ShowSkybox = false;  // Hide skybox by default for shadow visualization
    float m_SkyboxLOD = 0.0f;  // Skybox mipmap LOD (0 = sharp, higher = blurred)
    bool m_ShowDemoWindow = false;
};

} // namespace metagfx