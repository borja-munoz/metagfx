// ============================================================================
// include/metagfx/rhi/metal/MetalFramebuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Framebuffer.h"
#include "MetalTypes.h"

namespace metagfx {
namespace rhi {

class MetalFramebuffer : public Framebuffer {
public:
    MetalFramebuffer(MetalContext& context, const FramebufferDesc& desc);
    ~MetalFramebuffer() override;

    // Framebuffer interface
    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Ref<Texture> GetDepthAttachment() const override { return m_DepthAttachment; }
    const std::vector<Ref<Texture>>& GetColorAttachments() const override { return m_ColorAttachments; }

private:
    MetalContext& m_Context;
    std::vector<Ref<Texture>> m_ColorAttachments;
    Ref<Texture> m_DepthAttachment;
    uint32 m_Width = 0;
    uint32 m_Height = 0;
};

} // namespace rhi
} // namespace metagfx
