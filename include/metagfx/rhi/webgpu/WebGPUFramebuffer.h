// ============================================================================
// include/metagfx/rhi/webgpu/WebGPUFramebuffer.h
// ============================================================================
#pragma once

#include "metagfx/rhi/Framebuffer.h"
#include "WebGPUTypes.h"
#include <vector>

namespace metagfx {
namespace rhi {

class WebGPUFramebuffer : public Framebuffer {
public:
    WebGPUFramebuffer(WebGPUContext& context, const FramebufferDesc& desc);
    ~WebGPUFramebuffer() override;

    uint32 GetWidth() const override { return m_Width; }
    uint32 GetHeight() const override { return m_Height; }
    Ref<Texture> GetDepthAttachment() const override { return m_DepthAttachment; }
    const std::vector<Ref<Texture>>& GetColorAttachments() const override { return m_ColorAttachments; }

private:
    WebGPUContext& m_Context;
    std::vector<Ref<Texture>> m_ColorAttachments;
    Ref<Texture> m_DepthAttachment;
    uint32 m_Width = 0;
    uint32 m_Height = 0;
};

} // namespace rhi
} // namespace metagfx
