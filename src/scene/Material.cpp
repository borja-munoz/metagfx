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

    // Initialize padding to zero
    m_Properties.padding[0] = 0.0f;
    m_Properties.padding[1] = 0.0f;
    m_Properties.padding[2] = 0.0f;
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

} // namespace metagfx
