#pragma once

#include "metagfx/core/Types.h"
#include <glm/glm.hpp>

namespace metagfx {

// Light types matching GPU shader
enum class LightType : uint32 {
    Directional = 0,
    Point = 1,
    Spot = 2
};

// GPU-compatible light data (std140 layout)
// Must be exactly 64 bytes for array alignment
struct LightData {
    glm::vec4 positionAndType;   // xyz = position (world space), w = type
    glm::vec4 directionAndRange; // xyz = direction (normalized), w = range
    glm::vec4 colorAndIntensity; // rgb = color, w = intensity
    glm::vec4 spotAngles;        // x = inner cone (rad), y = outer cone (rad), z = att constant, w = att linear
};

// Complete light buffer structure for GPU
struct LightBuffer {
    uint32 lightCount;           // Number of active lights
    uint32 padding[3];           // Align to 16 bytes
    LightData lights[16];        // Maximum 16 lights
};

// Abstract base class for all light types
class Light {
public:
    Light(LightType type, const glm::vec3& color, float intensity);
    virtual ~Light() = default;

    // Non-copyable, movable
    Light(const Light&) = delete;
    Light& operator=(const Light&) = delete;
    Light(Light&&) = default;
    Light& operator=(Light&&) = default;

    // Common properties
    void SetColor(const glm::vec3& color);
    void SetIntensity(float intensity);

    const glm::vec3& GetColor() const { return m_Color; }
    float GetIntensity() const { return m_Intensity; }
    LightType GetType() const { return m_Type; }

    // Each subclass implements this to fill GPU data
    virtual LightData ToGPUData() const = 0;

protected:
    LightType m_Type;
    glm::vec3 m_Color;
    float m_Intensity;
};

// Directional Light (parallel rays, like sun/moon)
class DirectionalLight : public Light {
public:
    DirectionalLight(const glm::vec3& direction = glm::vec3(0.0f, -1.0f, 0.0f),
                     const glm::vec3& color = glm::vec3(1.0f),
                     float intensity = 1.0f);

    void SetDirection(const glm::vec3& direction);
    const glm::vec3& GetDirection() const { return m_Direction; }

    LightData ToGPUData() const override;

private:
    glm::vec3 m_Direction;
};

// Point Light (omnidirectional, like light bulb)
class PointLight : public Light {
public:
    PointLight(const glm::vec3& position = glm::vec3(0.0f),
               float range = 10.0f,
               const glm::vec3& color = glm::vec3(1.0f),
               float intensity = 1.0f);

    void SetPosition(const glm::vec3& position);
    void SetRange(float range);
    void SetAttenuation(float constant, float linear);

    const glm::vec3& GetPosition() const { return m_Position; }
    float GetRange() const { return m_Range; }

    LightData ToGPUData() const override;

private:
    glm::vec3 m_Position;
    float m_Range;
    float m_AttenuationConstant;
    float m_AttenuationLinear;
};

// Spot Light (cone-shaped, like flashlight)
class SpotLight : public Light {
public:
    SpotLight(const glm::vec3& position = glm::vec3(0.0f),
              const glm::vec3& direction = glm::vec3(0.0f, -1.0f, 0.0f),
              float innerConeAngle = 12.5f,  // degrees
              float outerConeAngle = 17.5f,  // degrees
              float range = 10.0f,
              const glm::vec3& color = glm::vec3(1.0f),
              float intensity = 1.0f);

    void SetPosition(const glm::vec3& position);
    void SetDirection(const glm::vec3& direction);
    void SetConeAngles(float innerDegrees, float outerDegrees);
    void SetRange(float range);
    void SetAttenuation(float constant, float linear);

    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::vec3& GetDirection() const { return m_Direction; }
    float GetInnerConeAngle() const { return glm::degrees(m_InnerConeAngle); }
    float GetOuterConeAngle() const { return glm::degrees(m_OuterConeAngle); }

    LightData ToGPUData() const override;

private:
    glm::vec3 m_Position;
    glm::vec3 m_Direction;
    float m_InnerConeAngle;  // Stored in radians
    float m_OuterConeAngle;  // Stored in radians
    float m_Range;
    float m_AttenuationConstant;
    float m_AttenuationLinear;
};

} // namespace metagfx
