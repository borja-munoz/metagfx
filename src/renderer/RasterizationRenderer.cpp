// ============================================================================
// src/renderer/RasterizationRenderer.cpp
// ============================================================================
#include "metagfx/renderer/RasterizationRenderer.h"
#include "metagfx/scene/Scene.h"
#include "metagfx/scene/Camera.h"
#include "metagfx/core/Logger.h"

namespace metagfx {

RasterizationRenderer::RasterizationRenderer(Ref<rhi::GraphicsDevice> device)
    : Renderer(device) {
}

RasterizationRenderer::~RasterizationRenderer() {
    Shutdown();
}

void RasterizationRenderer::Initialize() {
    METAGFX_INFO << "Initializing Rasterization Renderer";

    // TODO: Create pipelines in Phase 0
    // TODO: Initialize shadow map in Phase 2
}

void RasterizationRenderer::Shutdown() {
    METAGFX_INFO << "Shutting down Rasterization Renderer";

    DestroyPipelines();
    // m_ShadowMap.reset();  // TODO: Uncomment when ShadowMap is implemented in Phase 2
}

void RasterizationRenderer::Render(Scene& scene, Camera& camera) {
    // TODO: This will be implemented in phases:
    // Phase 2: Shadow pass
    // Phase 4: Main pass with shadow sampling

    // if (m_EnableShadows && m_ShadowMap) {
    //     RenderShadowPass(scene, camera);
    // }

    RenderMainPass(scene, camera);
}

void RasterizationRenderer::OnResize(uint32 width, uint32 height) {
    m_Width = width;
    m_Height = height;

    // Recreate pipelines if necessary
    // Shadow map size is independent of viewport size
}

bool RasterizationRenderer::SupportsFeature(RenderFeature feature) const {
    switch (feature) {
        case RenderFeature::Shadows:
            return true;  // Shadow mapping is supported
        case RenderFeature::AmbientOcclusion:
            return false;  // SSAO not implemented yet
        default:
            return false;  // No ray tracing features in rasterization mode
    }
}

void RasterizationRenderer::SetShadowMapSize(uint32 size) {
    if (size < 512 || size > 8192) {
        METAGFX_WARN << "Shadow map size must be between 512 and 8192. Clamping.";
        size = std::clamp(size, 512u, 8192u);
    }

    m_ShadowMapSize = size;

    // Recreate shadow map if it already exists
    // if (m_ShadowMap) {
    //     // TODO: Recreate shadow map in Phase 2
    // }
}

void RasterizationRenderer::RenderShadowPass(Scene& scene, Camera& camera) {
    // TODO: Implement in Phase 2
    // 1. Set up light-space transformation matrices
    // 2. Bind shadow pipeline
    // 3. Render scene from light's perspective to shadow map
}

void RasterizationRenderer::RenderMainPass(Scene& scene, Camera& camera) {
    // TODO: Implement in Phase 4
    // This will eventually replace the rendering logic currently in Application::Render()
    // 1. Bind main pipeline
    // 2. Bind shadow map texture (if shadows enabled)
    // 3. Render scene with PBR + IBL + shadows
}

void RasterizationRenderer::CreatePipelines() {
    // TODO: Implement in Phase 0/1
    // - Create main PBR pipeline (moved from Application)
    // - Create shadow pipeline (Phase 2)
}

void RasterizationRenderer::DestroyPipelines() {
    m_MainPipeline.reset();
    m_ShadowPipeline.reset();
}

} // namespace metagfx
