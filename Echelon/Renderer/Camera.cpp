#include "Camera.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

namespace Echelon {

    Camera::Camera() {
        Recalculate();
    }

    // ------------------------------------------------------------------
    // Perspective
    // ------------------------------------------------------------------

    void Camera::SetPerspective(float fovDegrees, float nearClip, float farClip) {
        m_FOV      = fovDegrees;
        m_NearClip = nearClip;
        m_FarClip  = farClip;
        m_ProjectionType = ProjectionType::Perspective;
        RecalculateProjection();
    }

    // ------------------------------------------------------------------
    // Orthographic
    // ------------------------------------------------------------------

    void Camera::SetOrthographic(float size, float nearClip, float farClip) {
        m_OrthoSize = size;
        m_OrthoNear = nearClip;
        m_OrthoFar  = farClip;
        m_ProjectionType = ProjectionType::Orthographic;
        RecalculateProjection();
    }

    // ------------------------------------------------------------------
    // Projection type
    // ------------------------------------------------------------------

    void Camera::SetProjectionType(ProjectionType type) {
        m_ProjectionType = type;
        RecalculateProjection();
    }

    // ------------------------------------------------------------------
    // Viewport
    // ------------------------------------------------------------------

    void Camera::SetViewportSize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return;
        m_ViewportWidth  = width;
        m_ViewportHeight = height;
        m_AspectRatio    = static_cast<float>(width) / static_cast<float>(height);
        RecalculateProjection();
    }

    // ------------------------------------------------------------------
    // View transform
    // ------------------------------------------------------------------

    void Camera::SetPosition(const glm::vec3& position) {
        m_Position = position;
        RecalculateView();
    }

    void Camera::SetRotation(const glm::vec3& eulerDegrees) {
        m_Rotation = eulerDegrees;
        RecalculateView();
    }

    // ------------------------------------------------------------------
    // Direction vectors
    // ------------------------------------------------------------------

    glm::vec3 Camera::GetForward() const {
        glm::quat orientation = glm::quat(glm::radians(m_Rotation));
        return glm::normalize(orientation * glm::vec3(0.0f, 0.0f, -1.0f));
    }

    glm::vec3 Camera::GetRight() const {
        glm::quat orientation = glm::quat(glm::radians(m_Rotation));
        return glm::normalize(orientation * glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 Camera::GetUp() const {
        glm::quat orientation = glm::quat(glm::radians(m_Rotation));
        return glm::normalize(orientation * glm::vec3(0.0f, 1.0f, 0.0f));
    }

    // ------------------------------------------------------------------
    // Full recalculation
    // ------------------------------------------------------------------

    void Camera::Recalculate() {
        RecalculateView();
        RecalculateProjection();
    }

    // ------------------------------------------------------------------
    // View matrix
    // ------------------------------------------------------------------

    void Camera::RecalculateView() {
        glm::quat orientation = glm::quat(glm::radians(m_Rotation));
        glm::mat4 rotation    = glm::toMat4(orientation);
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), -m_Position);

        // View = inverse(T * R) = R^T * T^-1
        m_ViewMatrix = glm::transpose(rotation) * translation;
    }

    // ------------------------------------------------------------------
    // Projection matrix
    // ------------------------------------------------------------------

    void Camera::RecalculateProjection() {
        if (m_ProjectionType == ProjectionType::Perspective) {
            m_ProjectionMatrix = glm::perspective(
                glm::radians(m_FOV),
                m_AspectRatio,
                m_NearClip,
                m_FarClip
            );
        } else {
            float halfW = m_OrthoSize * m_AspectRatio * 0.5f;
            float halfH = m_OrthoSize * 0.5f;
            m_ProjectionMatrix = glm::ortho(
                -halfW, halfW,
                -halfH, halfH,
                m_OrthoNear,
                m_OrthoFar
            );
        }
    }

} // namespace Echelon
