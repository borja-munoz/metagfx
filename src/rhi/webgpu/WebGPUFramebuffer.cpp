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
    , m_Width(0)
    , m_Height(0) {

    // WebGPU doesn't have explicit framebuffer objects
    // Framebuffers are created implicitly during BeginRenderPass
    // This class simply stores the attachment references

    // Infer dimensions from attachments
    if (!m_ColorAttachments.empty() && m_ColorAttachments[0]) {
        m_Width = m_ColorAttachments[0]->GetWidth();
        m_Height = m_ColorAttachments[0]->GetHeight();
    } else if (m_DepthAttachment) {
        m_Width = m_DepthAttachment->GetWidth();
        m_Height = m_DepthAttachment->GetHeight();
    }

    // Validate attachments
    if (m_ColorAttachments.empty() && !m_DepthAttachment) {
        METAGFX_WARN << "Framebuffer created with no attachments";
    }

    // Validate dimensions
    for (const auto& colorAttachment : m_ColorAttachments) {
        if (colorAttachment) {
            if (colorAttachment->GetWidth() != m_Width || colorAttachment->GetHeight() != m_Height) {
                METAGFX_WARN << "Color attachment dimensions mismatch: "
                                   << colorAttachment->GetWidth() << "x" << colorAttachment->GetHeight()
                                   << " vs framebuffer " << m_Width << "x" << m_Height;
            }
        }
    }

    if (m_DepthAttachment) {
        if (m_DepthAttachment->GetWidth() != m_Width || m_DepthAttachment->GetHeight() != m_Height) {
            METAGFX_WARN << "Depth attachment dimensions mismatch: "
                               << m_DepthAttachment->GetWidth() << "x" << m_DepthAttachment->GetHeight()
                               << " vs framebuffer " << m_Width << "x" << m_Height;
        }
    }

    METAGFX_INFO << "WebGPU framebuffer created: " << m_Width << "x" << m_Height
                    << ", color attachments: " << m_ColorAttachments.size()
                    << ", depth: " << (m_DepthAttachment ? "yes" : "no");
}

WebGPUFramebuffer::~WebGPUFramebuffer() {
    // No resources to clean up (textures are owned by Ref<>)
}

} // namespace rhi
} // namespace metagfx
