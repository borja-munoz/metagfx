// ============================================================================
// src/scene/Material.cpp
// ============================================================================
#include "metagfx/scene/Material.h"
#include "metagfx/core/Logger.h"
#include <algorithm>

namespace metagfx {

Material::Material(const glm::vec3& albedo, float roughness, float metallic) {
    SetAlbedo(albedo);
    SetRoughness(roughness);
    SetMetallic(metallic);
    SetEmissiveFactor(glm::vec3(0.0f));  // Default: no emission

    // Initialize padding to zero
    m_Properties.padding1[0] = 0.0f;
    m_Properties.padding1[1] = 0.0f;
    m_Properties.padding2 = 0.0f;
}

void Material::SetAlbedo(const glm::vec3& albedo) {
    // Clamp albedo components to [0, 1] range
    m_Properties.albedo = glm::clamp(albedo, glm::vec3(0.0f), glm::vec3(1.0f));
}

void Material::SetRoughness(float roughness) {
    // Clamp roughness to [0, 1] range
    m_Properties.roughness = std::clamp(roughness, 0.0f, 1.0f);
}

void Material::SetMetallic(float metallic) {
    // Clamp metallic to [0, 1] range
    m_Properties.metallic = std::clamp(metallic, 0.0f, 1.0f);
}

void Material::SetEmissiveFactor(const glm::vec3& emissive) {
    // Emissive can be > 1.0 for HDR emissive materials
    // Only clamp to non-negative values
    m_Properties.emissiveFactor = glm::max(emissive, glm::vec3(0.0f));
}

void Material::SetAlbedoMap(Ref<rhi::Texture> texture) {
    m_AlbedoMap = texture;

    if (texture) {
        m_TextureFlags |= static_cast<uint32>(MaterialTextureFlags::HasAlbedoMap);
    } else {
        m_TextureFlags &= ~static_cast<uint32>(MaterialTextureFlags::HasAlbedoMap);
    }
}

void Material::SetNormalMap(Ref<rhi::Texture> texture) {
    m_NormalMap = texture;

    if (texture) {
        m_TextureFlags |= static_cast<uint32>(MaterialTextureFlags::HasNormalMap);
    } else {
        m_TextureFlags &= ~static_cast<uint32>(MaterialTextureFlags::HasNormalMap);
    }
}

void Material::SetMetallicMap(Ref<rhi::Texture> texture) {
    m_MetallicMap = texture;

    if (texture) {
        m_TextureFlags |= static_cast<uint32>(MaterialTextureFlags::HasMetallicMap);
    } else {
        m_TextureFlags &= ~static_cast<uint32>(MaterialTextureFlags::HasMetallicMap);
    }
}

void Material::SetRoughnessMap(Ref<rhi::Texture> texture) {
    m_RoughnessMap = texture;

    if (texture) {
        m_TextureFlags |= static_cast<uint32>(MaterialTextureFlags::HasRoughnessMap);
    } else {
        m_TextureFlags &= ~static_cast<uint32>(MaterialTextureFlags::HasRoughnessMap);
    }
}

void Material::SetMetallicRoughnessMap(Ref<rhi::Texture> texture) {
    m_MetallicRoughnessMap = texture;

    if (texture) {
        m_TextureFlags |= static_cast<uint32>(MaterialTextureFlags::HasMetallicRoughnessMap);
    } else {
        m_TextureFlags &= ~static_cast<uint32>(MaterialTextureFlags::HasMetallicRoughnessMap);
    }
}

void Material::SetAOMap(Ref<rhi::Texture> texture) {
    m_AOMap = texture;

    if (texture) {
        m_TextureFlags |= static_cast<uint32>(MaterialTextureFlags::HasAOMap);
    } else {
        m_TextureFlags &= ~static_cast<uint32>(MaterialTextureFlags::HasAOMap);
    }
}

void Material::SetEmissiveMap(Ref<rhi::Texture> texture) {
    m_EmissiveMap = texture;

    if (texture) {
        m_TextureFlags |= static_cast<uint32>(MaterialTextureFlags::HasEmissiveMap);
    } else {
        m_TextureFlags &= ~static_cast<uint32>(MaterialTextureFlags::HasEmissiveMap);
    }
}

} // namespace metagfx
