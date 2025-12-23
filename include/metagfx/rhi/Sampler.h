// ============================================================================
// include/metagfx/rhi/Sampler.h
// ============================================================================
#pragma once

namespace metagfx {
namespace rhi {

/**
 * @brief Abstract sampler interface for texture sampling configuration
 *
 * Samplers define how textures are sampled in shaders (filtering, wrapping, etc.).
 * Samplers are typically shared across multiple materials for efficiency.
 */
class Sampler {
public:
    virtual ~Sampler() = default;

protected:
    Sampler() = default;
};

} // namespace rhi
} // namespace metagfx
