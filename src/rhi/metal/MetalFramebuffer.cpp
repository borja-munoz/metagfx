// ============================================================================
// src/rhi/metal/MetalFramebuffer.cpp
// ============================================================================
#include "metagfx/core/Logger.h"
#include "metagfx/rhi/metal/MetalFramebuffer.h"
#include "metagfx/rhi/metal/MetalTexture.h"

namespace metagfx {
namespace rhi {

MetalFramebuffer::MetalFramebuffer(MetalContext& context, const FramebufferDesc& desc)
    : m_Context(context) {

    // Store color attachments
    m_ColorAttachments = desc.colorAttachments;

    // Store depth attachment
    m_DepthAttachment = desc.depthAttachment;

    // Calculate dimensions from first available attachment
    if (m_DepthAttachment) {
        m_Width = m_DepthAttachment->GetWidth();
        m_Height = m_DepthAttachment->GetHeight();
    } else if (!m_ColorAttachments.empty() && m_ColorAttachments[0]) {
        m_Width = m_ColorAttachments[0]->GetWidth();
        m_Height = m_ColorAttachments[0]->GetHeight();
    }
}

MetalFramebuffer::~MetalFramebuffer() {
    m_ColorAttachments.clear();
    m_DepthAttachment.reset();
}

} // namespace rhi
} // namespace metagfx
