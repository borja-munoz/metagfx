// ============================================================================
// include/metagfx/scene/Camera.h
// ============================================================================
#pragma once

#include "metagfx/core/Types.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace metagfx {

enum class CameraProjection {
    Perspective,
    Orthographic
};

class Camera {
public:
    Camera(float fov = 45.0f, float aspectRatio = 16.0f / 9.0f, 
           float nearPlane = 0.1f, float farPlane = 100.0f);
    ~Camera() = default;

    // Projection
    void SetPerspective(float fov, float aspectRatio, float nearPlane, float farPlane);
    void SetOrthographic(float left, float right, float bottom, float top, 
                        float nearPlane, float farPlane);
    void SetAspectRatio(float aspectRatio);
    
    // Transform
    void SetPosition(const glm::vec3& position);
    void SetRotation(float pitch, float yaw, float roll = 0.0f);
    void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0, 1, 0));
    
    // Movement
    void Move(const glm::vec3& delta);
    void Rotate(float deltaPitch, float deltaYaw);
    
    // Camera controls
    void ProcessKeyboard(int key, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);

    // Orbital camera controls
    void SetOrbitTarget(const glm::vec3& target);
    void OrbitAroundTarget(float deltaYaw, float deltaPitch);
    void ZoomToTarget(float delta);
    const glm::vec3& GetOrbitTarget() const { return m_OrbitTarget; }
    float GetOrbitDistance() const { return m_OrbitDistance; }

    /**
     * @brief Frame the camera to view a bounding box with proper margins
     * @param center Center of the bounding box
     * @param size Size (extent) of the bounding box
     * @param marginFactor Margin multiplier (1.0 = tight fit, 1.5 = 50% margin, default: 1.3)
     */
    void FrameBoundingBox(const glm::vec3& center, const glm::vec3& size, float marginFactor = 1.3f);

    // Getters
    const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
    const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
    glm::mat4 GetViewProjectionMatrix() const { return m_ProjectionMatrix * m_ViewMatrix; }
    
    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::vec3& GetFront() const { return m_Front; }
    const glm::vec3& GetUp() const { return m_Up; }
    const glm::vec3& GetRight() const { return m_Right; }
    
    float GetFOV() const { return m_FOV; }
    float GetNearPlane() const { return m_NearPlane; }
    float GetFarPlane() const { return m_FarPlane; }

private:
    void UpdateViewMatrix();
    void UpdateVectors();

    // Projection parameters
    CameraProjection m_ProjectionType = CameraProjection::Perspective;
    float m_FOV = 45.0f;
    float m_AspectRatio = 16.0f / 9.0f;
    float m_NearPlane = 0.1f;
    float m_FarPlane = 100.0f;
    
    // Orthographic parameters
    float m_OrthoLeft = -10.0f;
    float m_OrthoRight = 10.0f;
    float m_OrthoBottom = -10.0f;
    float m_OrthoTop = 10.0f;
    
    // Transform
    glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_Right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 m_WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // Euler angles
    float m_Pitch = 0.0f;
    float m_Yaw = -90.0f;
    float m_Roll = 0.0f;
    
    // Camera options
    float m_MovementSpeed = 2.5f;
    float m_MouseSensitivity = 0.5f;  // Increased from 0.1 for better click-and-drag responsiveness
    float m_ZoomSensitivity = 1.0f;

    // Orbital camera state
    glm::vec3 m_OrbitTarget = glm::vec3(0.0f, 0.0f, 0.0f);  // Point to orbit around
    float m_OrbitDistance = 5.0f;  // Distance from target
    float m_OrbitYaw = 0.0f;       // Horizontal angle around target
    float m_OrbitPitch = 0.0f;     // Vertical angle around target

    // Matrices
    glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
    glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
};

} // namespace metagfx

