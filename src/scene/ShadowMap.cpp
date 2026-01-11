// ============================================================================
// src/scene/ShadowMap.cpp
// ============================================================================
#include "metagfx/scene/ShadowMap.h"
#include "metagfx/core/Logger.h"
#include <glm/gtc/matrix_transform.hpp>

namespace metagfx {

ShadowMap::ShadowMap(Ref<rhi::GraphicsDevice> device, uint32 width, uint32 height)
    : m_Device(device)
    , m_Width(width)
    , m_Height(height)
    , m_LightSpaceMatrix(1.0f) {

    METAGFX_INFO << "Creating shadow map: " << width << "x" << height;

    // Create depth texture for shadow map
    rhi::TextureDesc depthDesc{};
    depthDesc.type = rhi::TextureType::Texture2D;
    depthDesc.width = width;
    depthDesc.height = height;
    depthDesc.format = rhi::Format::D32_SFLOAT;
    depthDesc.usage = rhi::TextureUsage::DepthStencilAttachment | rhi::TextureUsage::Sampled;
    depthDesc.mipLevels = 1;
    depthDesc.arrayLayers = 1;
    depthDesc.debugName = "ShadowMap_Depth";
    m_DepthTexture = device->CreateTexture(depthDesc);

    // Create framebuffer for shadow rendering
    rhi::FramebufferDesc fbDesc{};
    fbDesc.depthAttachment = m_DepthTexture;
    fbDesc.debugName = "ShadowMap_Framebuffer";
    m_Framebuffer = device->CreateFramebuffer(fbDesc);

    // Create comparison sampler for shadow map sampling (PCF)
    rhi::SamplerDesc samplerDesc{};
    samplerDesc.minFilter = rhi::Filter::Linear;
    samplerDesc.magFilter = rhi::Filter::Linear;
    samplerDesc.mipmapMode = rhi::Filter::Linear;
    samplerDesc.addressModeU = rhi::SamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeV = rhi::SamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeW = rhi::SamplerAddressMode::ClampToEdge;
    samplerDesc.enableCompare = true;
    samplerDesc.compareOp = rhi::CompareOp::LessOrEqual;  // Standard depth convention
    m_Sampler = device->CreateSampler(samplerDesc);

    METAGFX_INFO << "Shadow sampler created with LessOrEqual comparison";

    METAGFX_INFO << "Shadow map created successfully";
}

ShadowMap::~ShadowMap() {
    METAGFX_INFO << "Destroying shadow map";
}

void ShadowMap::UpdateLightMatrix(const glm::vec3& lightDir, const Camera& camera) {
    // For directional lights, we use an orthographic projection
    // The light "position" is along the light direction from the scene center

    // TODO: In a production system, this should compute a tight bounding box
    // around the visible scene geometry to minimize shadow map resolution waste

    // CRITICAL: Use a larger orthographic frustum to ensure all models are captured
    // The helmet model is tall and needs a bigger frustum than the bunny
    float orthoSize = 30.0f;  // Increased from 20.0f to handle larger models

    // IMPORTANT: Use Vulkan-style orthographic projection with [0, 1] depth range
    // glm::ortho uses OpenGL convention [-1, 1], which is wrong for Vulkan
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    // Manually construct orthographic matrix for Vulkan
    // Vulkan NDC: X right, Y down, Z forward [0, 1]
    // GLM's lookAt produces right-handed view space where -Z is forward (objects have negative Z)
    // We need to map view-space Z range [-far, -near] to NDC [0, 1]
    // Formula: NDC_z = (view_z + far) / (far - near) maps [-far, -near] to [0, 1]
    //   But view_z is negative, so: NDC_z = (-view_z - near) / (far - near)
    //   This is: NDC_z = -view_z / (far - near) - near / (far - near)
    glm::mat4 lightProjection = glm::mat4(1.0f);
    lightProjection[0][0] = 1.0f / orthoSize;  // Scale X (column 0, row 0)
    lightProjection[1][1] = 1.0f / orthoSize;  // Scale Y (column 1, row 1)
    lightProjection[2][2] = -1.0f / (farPlane - nearPlane);  // Scale Z (column 2, row 2)
    // NOTE: GLM is column-major, so [col][row]
    // Translation is in column 3: lightProjection[3] = vec4(tx, ty, tz, 1)
    // Formula: NDC = -view_z / (far - near) - near / (far - near)
    lightProjection[3][2] = -nearPlane / (farPlane - nearPlane);  // Translate Z (column 3, row 2)

    // Light "position" along the negative light direction (farther back)
    glm::vec3 lightPos = -glm::normalize(lightDir) * 60.0f;  // Increased from 40.0f for larger frustum

    // Light looks at the origin (scene center)
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // Avoid degenerate case where light direction is parallel to up vector
    if (std::abs(glm::dot(glm::normalize(lightDir), up)) > 0.999f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::mat4 lightView = glm::lookAt(lightPos, target, up);

    // Combined light-space matrix
    m_LightSpaceMatrix = lightProjection * lightView;

    // Debug logging (enabled for shadow debugging)
    static bool logged = false;
    if (!logged) {
        METAGFX_INFO << "Shadow frustum - orthoSize: " << orthoSize
                     << " (covers -" << orthoSize << " to +" << orthoSize << " in X and Z)"
                     << ", near: " << nearPlane << ", far: " << farPlane
                     << ", lightPos: (" << lightPos.x << ", " << lightPos.y << ", " << lightPos.z << ")"
                     << ", lightDir: (" << lightDir.x << ", " << lightDir.y << ", " << lightDir.z << ")";
        METAGFX_INFO << "Using Vulkan-style orthographic projection (depth [0,1])";
        METAGFX_INFO << "Projection matrix Z row: [" << lightProjection[2][0] << ", "
                     << lightProjection[2][1] << ", " << lightProjection[2][2] << ", " << lightProjection[2][3] << "]";
        METAGFX_INFO << "Projection matrix W row: [" << lightProjection[3][0] << ", "
                     << lightProjection[3][1] << ", " << lightProjection[3][2] << ", " << lightProjection[3][3] << "]";

        // Test transformation: origin point (0, 0, 0) should be in middle of frustum
        glm::vec4 testOrigin = m_LightSpaceMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec3 testNDC = glm::vec3(testOrigin) / testOrigin.w;
        METAGFX_INFO << "Test: Origin (0,0,0) -> Light NDC (raw): ("
                     << testNDC.x << ", " << testNDC.y << ", " << testNDC.z << ")";
        // For texture sampling, X/Y need * 0.5 + 0.5, but Z is already [0,1]
        glm::vec3 testTexCoord = testNDC;
        testTexCoord.x = testNDC.x * 0.5f + 0.5f;
        testTexCoord.y = testNDC.y * 0.5f + 0.5f;
        METAGFX_INFO << "  -> Texture coords: ("
                     << testTexCoord.x << ", " << testTexCoord.y << ", " << testTexCoord.z << ")";

        // Test a point 2 units above origin (where models might be)
        glm::vec4 testAbove = m_LightSpaceMatrix * glm::vec4(0.0f, 2.0f, 0.0f, 1.0f);
        glm::vec3 testAboveNDC = glm::vec3(testAbove) / testAbove.w;
        METAGFX_INFO << "Test: Point (0,2,0) -> Light NDC (raw): ("
                     << testAboveNDC.x << ", " << testAboveNDC.y << ", " << testAboveNDC.z << ")";
        glm::vec3 testAboveTexCoord = testAboveNDC;
        testAboveTexCoord.x = testAboveNDC.x * 0.5f + 0.5f;
        testAboveTexCoord.y = testAboveNDC.y * 0.5f + 0.5f;
        METAGFX_INFO << "  -> Texture coords: ("
                     << testAboveTexCoord.x << ", " << testAboveTexCoord.y << ", " << testAboveTexCoord.z << ")";

        logged = true;
    }
}

} // namespace metagfx
