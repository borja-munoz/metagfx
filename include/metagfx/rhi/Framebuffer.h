// ============================================================================
// include/metagfx/rhi/Framebuffer.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/Texture.h"
#include "metagfx/rhi/Types.h"  // For FramebufferDesc

namespace metagfx {
namespace rhi {

// Abstract framebuffer interface
// Represents a collection of attachments (color, depth, stencil) for rendering
class Framebuffer {
public:
    virtual ~Framebuffer() = default;

    // Getters
    virtual uint32 GetWidth() const = 0;
    virtual uint32 GetHeight() const = 0;
    virtual Ref<Texture> GetDepthAttachment() const = 0;
    virtual const std::vector<Ref<Texture>>& GetColorAttachments() const = 0;
};

} // namespace rhi
} // namespace metagfx
