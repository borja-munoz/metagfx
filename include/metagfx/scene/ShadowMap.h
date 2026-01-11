// ============================================================================
// include/metagfx/scene/ShadowMap.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/GraphicsDevice.h"
#include "metagfx/rhi/Texture.h"
#include "metagfx/rhi/Framebuffer.h"
#include "metagfx/rhi/Sampler.h"
#include "metagfx/scene/Camera.h"
#include <glm/glm.hpp>

namespace metagfx {

// Shadow map for directional light shadow rendering
// Creates a depth-only framebuffer and manages light-space transformation matrices
class ShadowMap {
public:
    ShadowMap(Ref<rhi::GraphicsDevice> device, uint32 width, uint32 height);
    ~ShadowMap();

    // Update light-space transformation matrix for directional light
    void UpdateLightMatrix(const glm::vec3& lightDir, const Camera& camera);

    // Getters
    uint32 GetWidth() const { return m_Width; }
    uint32 GetHeight() const { return m_Height; }
    Ref<rhi::Texture> GetDepthTexture() const { return m_DepthTexture; }
    Ref<rhi::Framebuffer> GetFramebuffer() const { return m_Framebuffer; }
    Ref<rhi::Sampler> GetSampler() const { return m_Sampler; }
    const glm::mat4& GetLightSpaceMatrix() const { return m_LightSpaceMatrix; }

private:
    Ref<rhi::GraphicsDevice> m_Device;

    // Shadow map resources
    Ref<rhi::Texture> m_DepthTexture;
    Ref<rhi::Framebuffer> m_Framebuffer;
    Ref<rhi::Sampler> m_Sampler;  // Comparison sampler for PCF

    // Dimensions
    uint32 m_Width;
    uint32 m_Height;

    // Light-space transformation matrix
    glm::mat4 m_LightSpaceMatrix;
};

} // namespace metagfx
