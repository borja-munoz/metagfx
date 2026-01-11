// ============================================================================
// include/metagfx/renderer/Renderer.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/rhi/GraphicsDevice.h"

namespace metagfx {

// Forward declarations
class Scene;
class Camera;

// Rendering mode enumeration
enum class RenderMode {
    Rasterization,  // Traditional rasterization with shadow maps
    Hybrid,         // Rasterization + ray traced effects
    PathTracing     // Full path tracing
};

// Render feature flags
enum class RenderFeature {
    Shadows,              // Shadow mapping (rasterization)
    RayTracedShadows,     // Ray traced shadows
    Reflections,          // Screen-space reflections
    RayTracedReflections, // Ray traced reflections
    GlobalIllumination,   // Path traced GI
    AmbientOcclusion,     // SSAO
    RayTracedAO           // Ray traced AO
};

// Abstract renderer base class
// Provides interface for different rendering modes (Rasterization, Hybrid, Path Tracing)
class Renderer {
public:
    virtual ~Renderer() = default;

    // Core rendering interface
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Render(Scene& scene, Camera& camera) = 0;
    virtual void OnResize(uint32 width, uint32 height) = 0;

    // Renderer identification
    virtual const char* GetName() const = 0;
    virtual RenderMode GetMode() const = 0;
    virtual bool SupportsFeature(RenderFeature feature) const = 0;

protected:
    explicit Renderer(Ref<rhi::GraphicsDevice> device)
        : m_Device(device) {}

    Ref<rhi::GraphicsDevice> m_Device;
};

} // namespace metagfx
