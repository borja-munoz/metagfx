#pragma once

#include "metagfx/core/Types.h"
#include "metagfx/scene/Light.h"
#include "metagfx/rhi/Buffer.h"
#include <vector>
#include <memory>

namespace metagfx {

// Forward declarations
namespace rhi {
    class GraphicsDevice;
    class Buffer;
}

class Scene {
public:
    Scene();
    ~Scene();

    // Light management
    Light* AddLight(std::unique_ptr<Light> light);
    void RemoveLight(Light* light);
    void ClearLights();

    const std::vector<std::unique_ptr<Light>>& GetLights() const { return m_Lights; }
    size_t GetLightCount() const { return m_Lights.size(); }

    // GPU buffer management
    void InitializeLightBuffer(rhi::GraphicsDevice* device);
    void UpdateLightBuffer();  // Call per frame before rendering
    Ref<rhi::Buffer> GetLightBuffer() const { return m_LightBuffer; }

    bool HasLights() const { return !m_Lights.empty(); }

    static constexpr uint32 MAX_LIGHTS = 16;

private:
    std::vector<std::unique_ptr<Light>> m_Lights;
    Ref<rhi::Buffer> m_LightBuffer;
    rhi::GraphicsDevice* m_Device = nullptr;
};

} // namespace metagfx
