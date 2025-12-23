#include "metagfx/scene/Light.h"
#include "metagfx/core/Logger.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace metagfx {

// Verify std140 alignment at compile time
static_assert(sizeof(LightData) == 64, "LightData must be 64 bytes for std140 alignment");
static_assert(sizeof(LightBuffer) == 1040, "LightBuffer must be 1040 bytes");
static_assert(offsetof(LightBuffer, lights) == 16, "lights array must start at offset 16");

// ============================================================================
// Light Base Class
// ============================================================================

Light::Light(LightType type, const glm::vec3& color, float intensity)
    : m_Type(type)
    , m_Color(color)
    , m_Intensity(intensity)
{
}

void Light::SetColor(const glm::vec3& color) {
    m_Color = color;
}

void Light::SetIntensity(float intensity) {
    m_Intensity = std::max(0.0f, intensity);  // Clamp to non-negative
}

// ============================================================================
// DirectionalLight
// ============================================================================

DirectionalLight::DirectionalLight(const glm::vec3& direction,
                                   const glm::vec3& color,
                                   float intensity)
    : Light(LightType::Directional, color, intensity)
    , m_Direction(glm::normalize(direction))
{
}

void DirectionalLight::SetDirection(const glm::vec3& direction) {
    m_Direction = glm::normalize(direction);
}

LightData DirectionalLight::ToGPUData() const {
    LightData data{};

    // Position is unused for directional lights (set w to type)
    data.positionAndType = glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(m_Type));

    // Direction is the primary data (range is unused for directional)
    data.directionAndRange = glm::vec4(m_Direction, 0.0f);

    // Color and intensity
    data.colorAndIntensity = glm::vec4(m_Color, m_Intensity);

    // Spot angles and attenuation are unused for directional lights
    data.spotAngles = glm::vec4(0.0f);

    return data;
}

// ============================================================================
// PointLight
// ============================================================================

PointLight::PointLight(const glm::vec3& position,
                       float range,
                       const glm::vec3& color,
                       float intensity)
    : Light(LightType::Point, color, intensity)
    , m_Position(position)
    , m_Range(std::max(0.01f, range))  // Prevent division by zero
    , m_AttenuationConstant(1.0f)
    , m_AttenuationLinear(0.09f)
{
}

void PointLight::SetPosition(const glm::vec3& position) {
    m_Position = position;
}

void PointLight::SetRange(float range) {
    m_Range = std::max(0.01f, range);  // Prevent division by zero
}

void PointLight::SetAttenuation(float constant, float linear) {
    m_AttenuationConstant = std::max(0.0f, constant);
    m_AttenuationLinear = std::max(0.0f, linear);
}

LightData PointLight::ToGPUData() const {
    LightData data{};

    // Position and type
    data.positionAndType = glm::vec4(m_Position, static_cast<float>(m_Type));

    // Direction is unused for point lights (range is used)
    data.directionAndRange = glm::vec4(0.0f, 0.0f, 0.0f, m_Range);

    // Color and intensity
    data.colorAndIntensity = glm::vec4(m_Color, m_Intensity);

    // Store attenuation constants (x, y unused for point lights)
    data.spotAngles = glm::vec4(0.0f, 0.0f, m_AttenuationConstant, m_AttenuationLinear);

    return data;
}

// ============================================================================
// SpotLight
// ============================================================================

SpotLight::SpotLight(const glm::vec3& position,
                     const glm::vec3& direction,
                     float innerConeAngle,
                     float outerConeAngle,
                     float range,
                     const glm::vec3& color,
                     float intensity)
    : Light(LightType::Spot, color, intensity)
    , m_Position(position)
    , m_Direction(glm::normalize(direction))
    , m_InnerConeAngle(glm::radians(innerConeAngle))
    , m_OuterConeAngle(glm::radians(outerConeAngle))
    , m_Range(std::max(0.01f, range))
    , m_AttenuationConstant(1.0f)
    , m_AttenuationLinear(0.09f)
{
    // Ensure outer cone is larger than inner cone
    if (m_OuterConeAngle < m_InnerConeAngle) {
        std::swap(m_InnerConeAngle, m_OuterConeAngle);
        METAGFX_WARN << "SpotLight: outer cone angle was smaller than inner, swapped values";
    }
}

void SpotLight::SetPosition(const glm::vec3& position) {
    m_Position = position;
}

void SpotLight::SetDirection(const glm::vec3& direction) {
    m_Direction = glm::normalize(direction);
}

void SpotLight::SetConeAngles(float innerDegrees, float outerDegrees) {
    m_InnerConeAngle = glm::radians(innerDegrees);
    m_OuterConeAngle = glm::radians(outerDegrees);

    // Ensure outer cone is larger than inner cone
    if (m_OuterConeAngle < m_InnerConeAngle) {
        std::swap(m_InnerConeAngle, m_OuterConeAngle);
        METAGFX_WARN << "SpotLight: outer cone angle was smaller than inner, swapped values";
    }
}

void SpotLight::SetRange(float range) {
    m_Range = std::max(0.01f, range);
}

void SpotLight::SetAttenuation(float constant, float linear) {
    m_AttenuationConstant = std::max(0.0f, constant);
    m_AttenuationLinear = std::max(0.0f, linear);
}

LightData SpotLight::ToGPUData() const {
    LightData data{};

    // Position and type
    data.positionAndType = glm::vec4(m_Position, static_cast<float>(m_Type));

    // Direction and range
    data.directionAndRange = glm::vec4(m_Direction, m_Range);

    // Color and intensity
    data.colorAndIntensity = glm::vec4(m_Color, m_Intensity);

    // Spot cone angles (in radians) and attenuation constants
    data.spotAngles = glm::vec4(m_InnerConeAngle, m_OuterConeAngle,
                                m_AttenuationConstant, m_AttenuationLinear);

    return data;
}

} // namespace metagfx
