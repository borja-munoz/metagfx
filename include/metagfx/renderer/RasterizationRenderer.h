// ============================================================================
// include/metagfx/renderer/RasterizationRenderer.h
// ============================================================================
#pragma once

#include "metagfx/renderer/Renderer.h"
#include "metagfx/rhi/Pipeline.h"
#include "metagfx/rhi/Buffer.h"
#include "metagfx/scene/Scene.h"
#include <memory>

namespace metagfx {

// Rasterization-based renderer with shadow mapping support
// This is the traditional forward rendering pipeline with PBR and IBL
class RasterizationRenderer : public Renderer {
public:
    explicit RasterizationRenderer(Ref<rhi::GraphicsDevice> device);
    ~RasterizationRenderer() override;

    // Renderer interface implementation
    void Initialize() override;
    void Shutdown() override;
    void Render(Scene& scene, Camera& camera) override;
    void OnResize(uint32 width, uint32 height) override;

    // Renderer identification
    const char* GetName() const override { return "Rasterization"; }
    RenderMode GetMode() const override { return RenderMode::Rasterization; }
    bool SupportsFeature(RenderFeature feature) const override;

    // Shadow settings
    void SetShadowsEnabled(bool enabled) { m_EnableShadows = enabled; }
    bool AreShadowsEnabled() const { return m_EnableShadows; }
    void SetShadowMapSize(uint32 size);
    uint32 GetShadowMapSize() const { return m_ShadowMapSize; }
    void SetShadowBias(float bias) { m_ShadowBias = bias; }
    float GetShadowBias() const { return m_ShadowBias; }

private:
    // Rendering passes
    void RenderShadowPass(Scene& scene, Camera& camera);
    void RenderMainPass(Scene& scene, Camera& camera);

    // Pipeline management
    void CreatePipelines();
    void DestroyPipelines();

    // Shadow mapping (will be added in Phase 2)
    // std::unique_ptr<ShadowMap> m_ShadowMap;  // TODO: Add in Phase 2
    Ref<rhi::Pipeline> m_ShadowPipeline;
    bool m_EnableShadows = true;
    uint32 m_ShadowMapSize = 2048;
    float m_ShadowBias = 0.005f;

    // Main rendering pipeline (will be moved from Application)
    Ref<rhi::Pipeline> m_MainPipeline;

    // Viewport dimensions
    uint32 m_Width = 0;
    uint32 m_Height = 0;
};

} // namespace metagfx
