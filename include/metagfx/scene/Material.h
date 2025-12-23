// ============================================================================
// include/metagfx/scene/Material.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include <glm/glm.hpp>

namespace metagfx {

// Forward declarations
namespace rhi {
    class Texture;
}

// Material texture flags
enum class MaterialTextureFlags : uint32 {
    None = 0,
    HasAlbedoMap = 1 << 0
};

// GPU-side structure (std140 layout compatible)
// Total size: 32 bytes for optimal alignment
struct MaterialProperties {
    glm::vec3 albedo;      // 12 bytes (offset 0)  - Base color
    float roughness;       // 4 bytes  (offset 12) - Surface roughness [0,1]
    float metallic;        // 4 bytes  (offset 16) - Metallic property [0,1]
    float padding[3];      // 12 bytes (offset 20) - Padding to 32 bytes
};

class Material {
public:
    // Constructor with default material properties
    Material(const glm::vec3& albedo = glm::vec3(0.8f),
             float roughness = 0.5f,
             float metallic = 0.0f);

    // Setters with validation
    void SetAlbedo(const glm::vec3& albedo);
    void SetRoughness(float roughness);
    void SetMetallic(float metallic);

    // Getters
    const MaterialProperties& GetProperties() const { return m_Properties; }
    const glm::vec3& GetAlbedo() const { return m_Properties.albedo; }
    float GetRoughness() const { return m_Properties.roughness; }
    float GetMetallic() const { return m_Properties.metallic; }

    // Texture management
    void SetAlbedoMap(Ref<rhi::Texture> texture);
    Ref<rhi::Texture> GetAlbedoMap() const { return m_AlbedoMap; }
    bool HasAlbedoMap() const { return (m_TextureFlags & static_cast<uint32>(MaterialTextureFlags::HasAlbedoMap)) != 0; }
    uint32 GetTextureFlags() const { return m_TextureFlags; }

private:
    MaterialProperties m_Properties;
    Ref<rhi::Texture> m_AlbedoMap;
    uint32 m_TextureFlags = 0;
};

} // namespace metagfx
