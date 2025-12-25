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

// Material texture flags (bit flags for shader)
enum class MaterialTextureFlags : uint32 {
    None = 0,
    HasAlbedoMap = 1 << 0,              // Bit 0: Albedo/base color texture
    HasNormalMap = 1 << 1,              // Bit 1: Normal map (tangent space)
    HasMetallicMap = 1 << 2,            // Bit 2: Metallic texture (grayscale)
    HasRoughnessMap = 1 << 3,           // Bit 3: Roughness texture (grayscale)
    HasMetallicRoughnessMap = 1 << 4,   // Bit 4: Combined metallic-roughness (glTF: G=roughness, B=metallic)
    HasAOMap = 1 << 5                   // Bit 5: Ambient occlusion map (grayscale)
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

    void SetNormalMap(Ref<rhi::Texture> texture);
    Ref<rhi::Texture> GetNormalMap() const { return m_NormalMap; }
    bool HasNormalMap() const { return (m_TextureFlags & static_cast<uint32>(MaterialTextureFlags::HasNormalMap)) != 0; }

    void SetMetallicMap(Ref<rhi::Texture> texture);
    Ref<rhi::Texture> GetMetallicMap() const { return m_MetallicMap; }
    bool HasMetallicMap() const { return (m_TextureFlags & static_cast<uint32>(MaterialTextureFlags::HasMetallicMap)) != 0; }

    void SetRoughnessMap(Ref<rhi::Texture> texture);
    Ref<rhi::Texture> GetRoughnessMap() const { return m_RoughnessMap; }
    bool HasRoughnessMap() const { return (m_TextureFlags & static_cast<uint32>(MaterialTextureFlags::HasRoughnessMap)) != 0; }

    void SetMetallicRoughnessMap(Ref<rhi::Texture> texture);
    Ref<rhi::Texture> GetMetallicRoughnessMap() const { return m_MetallicRoughnessMap; }
    bool HasMetallicRoughnessMap() const { return (m_TextureFlags & static_cast<uint32>(MaterialTextureFlags::HasMetallicRoughnessMap)) != 0; }

    void SetAOMap(Ref<rhi::Texture> texture);
    Ref<rhi::Texture> GetAOMap() const { return m_AOMap; }
    bool HasAOMap() const { return (m_TextureFlags & static_cast<uint32>(MaterialTextureFlags::HasAOMap)) != 0; }

    uint32 GetTextureFlags() const { return m_TextureFlags; }

private:
    MaterialProperties m_Properties;

    // PBR texture maps
    Ref<rhi::Texture> m_AlbedoMap;
    Ref<rhi::Texture> m_NormalMap;
    Ref<rhi::Texture> m_MetallicMap;
    Ref<rhi::Texture> m_RoughnessMap;
    Ref<rhi::Texture> m_MetallicRoughnessMap;  // Combined texture (glTF standard)
    Ref<rhi::Texture> m_AOMap;

    uint32 m_TextureFlags = 0;
};

} // namespace metagfx
