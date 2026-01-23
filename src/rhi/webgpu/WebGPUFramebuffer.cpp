// ============================================================================
// src/rhi/webgpu/WebGPUFramebuffer.cpp
// ============================================================================
#include "metagfx/rhi/webgpu/WebGPUFramebuffer.h"
#include "metagfx/core/Logger.h"

namespace metagfx {
namespace rhi {

WebGPUFramebuffer::WebGPUFramebuffer(WebGPUContext& context, const FramebufferDesc& desc)
    : m_Context(context)
    , m_ColorAttachments(desc.colorAttachments)
    , m_DepthAttachment(desc.depthAttachment)
    , m_Width(desc.width)
    , m_Height(desc.height) {

    // WebGPU doesn't have explicit framebuffer objects
    // Framebuffers are created implicitly during BeginRenderPass
    // This class simply stores the attachment references

    // Validate attachments
    if (m_ColorAttachments.empty() && !m_DepthAttachment) {
        WEBGPU_LOG_WARNING("Framebuffer created with no attachments");
    }

    // Validate dimensions
    for (const auto& colorAttachment : m_ColorAttachments) {
        if (colorAttachment) {
            if (colorAttachment->GetWidth() != m_Width || colorAttachment->GetHeight() != m_Height) {
                WEBGPU_LOG_WARNING("Color attachment dimensions mismatch: "
                                   << colorAttachment->GetWidth() << "x" << colorAttachment->GetHeight()
                                   << " vs framebuffer " << m_Width << "x" << m_Height);
            }
        }
    }

    if (m_DepthAttachment) {
        if (m_DepthAttachment->GetWidth() != m_Width || m_DepthAttachment->GetHeight() != m_Height) {
            WEBGPU_LOG_WARNING("Depth attachment dimensions mismatch: "
                               << m_DepthAttachment->GetWidth() << "x" << m_DepthAttachment->GetHeight()
                               << " vs framebuffer " << m_Width << "x" << m_Height);
        }
    }

    WEBGPU_LOG_INFO("WebGPU framebuffer created: " << m_Width << "x" << m_Height
                    << ", color attachments: " << m_ColorAttachments.size()
                    << ", depth: " << (m_DepthAttachment ? "yes" : "no"));
}

WebGPUFramebuffer::~WebGPUFramebuffer() {
    // No resources to clean up (textures are owned by Ref<>)
}

} // namespace rhi
} // namespace metagfx
