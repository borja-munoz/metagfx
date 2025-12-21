// ============================================================================
// src/scene/Camera.cpp
// ============================================================================
#include "metagfx/scene/Camera.h"
#include <SDL3/SDL.h>

namespace metagfx {

Camera::Camera(float fov, float aspectRatio, float nearPlane, float farPlane)
    : m_FOV(fov), m_AspectRatio(aspectRatio), m_NearPlane(nearPlane), m_FarPlane(farPlane) {
    
    SetPerspective(fov, aspectRatio, nearPlane, farPlane);
    UpdateVectors();
    UpdateViewMatrix();
}

void Camera::SetPerspective(float fov, float aspectRatio, float nearPlane, float farPlane) {
    m_ProjectionType = CameraProjection::Perspective;
    m_FOV = fov;
    m_AspectRatio = aspectRatio;
    m_NearPlane = nearPlane;
    m_FarPlane = farPlane;
    
    m_ProjectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
    
    // GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
    // For Vulkan, we need to flip the Y axis
    m_ProjectionMatrix[1][1] *= -1;
}

void Camera::SetOrthographic(float left, float right, float bottom, float top, 
                            float nearPlane, float farPlane) {
    m_ProjectionType = CameraProjection::Orthographic;
    m_OrthoLeft = left;
    m_OrthoRight = right;
    m_OrthoBottom = bottom;
    m_OrthoTop = top;
    m_NearPlane = nearPlane;
    m_FarPlane = farPlane;
    
    m_ProjectionMatrix = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
    m_ProjectionMatrix[1][1] *= -1; // Flip Y for Vulkan
}

void Camera::SetAspectRatio(float aspectRatio) {
    m_AspectRatio = aspectRatio;
    if (m_ProjectionType == CameraProjection::Perspective) {
        SetPerspective(m_FOV, m_AspectRatio, m_NearPlane, m_FarPlane);
    }
}

void Camera::SetPosition(const glm::vec3& position) {
    m_Position = position;
    UpdateViewMatrix();
}

void Camera::SetRotation(float pitch, float yaw, float roll) {
    m_Pitch = pitch;
    m_Yaw = yaw;
    m_Roll = roll;
    UpdateVectors();
    UpdateViewMatrix();
}

void Camera::LookAt(const glm::vec3& target, const glm::vec3& up) {
    m_ViewMatrix = glm::lookAt(m_Position, target, up);
    
    // Update front vector from view matrix
    m_Front = glm::normalize(target - m_Position);
    m_Right = glm::normalize(glm::cross(m_Front, up));
    m_Up = glm::normalize(glm::cross(m_Right, m_Front));
}

void Camera::Move(const glm::vec3& delta) {
    m_Position += delta;
    UpdateViewMatrix();
}

void Camera::Rotate(float deltaPitch, float deltaYaw) {
    m_Pitch += deltaPitch;
    m_Yaw += deltaYaw;
    
    // Constrain pitch
    if (m_Pitch > 89.0f)
        m_Pitch = 89.0f;
    if (m_Pitch < -89.0f)
        m_Pitch = -89.0f;
    
    UpdateVectors();
    UpdateViewMatrix();
}

void Camera::ProcessKeyboard(int key, float deltaTime) {
    float velocity = m_MovementSpeed * deltaTime;
    
    if (key == SDLK_W)
        Move(m_Front * velocity);
    if (key == SDLK_S)
        Move(-m_Front * velocity);
    if (key == SDLK_A)
        Move(-m_Right * velocity);
    if (key == SDLK_D)
        Move(m_Right * velocity);
    if (key == SDLK_Q)
        Move(-m_Up * velocity);
    if (key == SDLK_E)
        Move(m_Up * velocity);
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= m_MouseSensitivity;
    yoffset *= m_MouseSensitivity;
    
    m_Yaw += xoffset;
    m_Pitch += yoffset;
    
    if (constrainPitch) {
        if (m_Pitch > 89.0f)
            m_Pitch = 89.0f;
        if (m_Pitch < -89.0f)
            m_Pitch = -89.0f;
    }
    
    UpdateVectors();
    UpdateViewMatrix();
}

void Camera::ProcessMouseScroll(float yoffset) {
    m_FOV -= yoffset * m_ZoomSensitivity;
    
    if (m_FOV < 1.0f)
        m_FOV = 1.0f;
    if (m_FOV > 120.0f)
        m_FOV = 120.0f;
    
    if (m_ProjectionType == CameraProjection::Perspective) {
        SetPerspective(m_FOV, m_AspectRatio, m_NearPlane, m_FarPlane);
    }
}

void Camera::UpdateViewMatrix() {
    m_ViewMatrix = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
}

void Camera::UpdateVectors() {
    // Calculate the new front vector
    glm::vec3 front;
    front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    front.y = sin(glm::radians(m_Pitch));
    front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    m_Front = glm::normalize(front);
    
    // Re-calculate the right and up vectors
    m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
    m_Up = glm::normalize(glm::cross(m_Right, m_Front));
}

} // namespace metagfx