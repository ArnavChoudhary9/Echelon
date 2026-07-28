#pragma once

/**
 * @file Camera.hpp
 * @brief Camera class encapsulating view and projection math.
 *
 * Best Practices:
 *  - Camera owns no GPU resources — it is pure math.
 *  - Call Recalculate() after modifying Position / Rotation / FOV etc.
 *    or use the convenience setters which recalculate automatically.
 *  - Supports both perspective and orthographic projections.
 */

#include "Echelon/Core/Base.hpp"

#include "glm/glm.hpp"

#include <cstdint>

namespace Echelon {

    // ================================================================
    // Projection type
    // ================================================================

    enum class ProjectionType : uint8_t {
        Perspective  = 0,
        Orthographic = 1
    };

    // ================================================================
    // Camera
    // ================================================================

    class Camera {
    public:
        Camera();
        ~Camera() = default;

        // ---- Perspective parameters ----

        void  SetPerspective(float fovDegrees, float nearClip, float farClip);
        float GetFOV()      const { return m_FOV; }
        float GetNearClip() const { return m_NearClip; }
        float GetFarClip()  const { return m_FarClip; }

        // ---- Orthographic parameters ----

        void  SetOrthographic(float size, float nearClip, float farClip);
        float GetOrthoSize()     const { return m_OrthoSize; }
        float GetOrthoNearClip() const { return m_OrthoNear; }
        float GetOrthoFarClip()  const { return m_OrthoFar; }

        // ---- Projection type ----

        void           SetProjectionType(ProjectionType type);
        ProjectionType GetProjectionType() const { return m_ProjectionType; }

        // ---- Viewport ----

        void     SetViewportSize(uint32_t width, uint32_t height);
        uint32_t GetViewportWidth()  const { return m_ViewportWidth; }
        uint32_t GetViewportHeight() const { return m_ViewportHeight; }
        float    GetAspectRatio()    const { return m_AspectRatio; }

        // ---- View transform (position + orientation) ----

        void SetPosition(const glm::vec3& position);
        void SetRotation(const glm::vec3& eulerDegrees);

        const glm::vec3& GetPosition() const { return m_Position; }
        const glm::vec3& GetRotation() const { return m_Rotation; }

        // ---- Derived direction vectors ----

        glm::vec3 GetForward() const;
        glm::vec3 GetRight()   const;
        glm::vec3 GetUp()      const;

        // ---- Matrices (pre-computed) ----

        const glm::mat4& GetViewMatrix()       const { return m_ViewMatrix; }
        const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
        glm::mat4         GetViewProjection()   const { return m_ProjectionMatrix * m_ViewMatrix; }

        // ---- Manual recalculation ----

        /** Recompute view + projection from current parameters. */
        void Recalculate();

    private:
        void RecalculateView();
        void RecalculateProjection();

        // Projection
        ProjectionType m_ProjectionType = ProjectionType::Perspective;
        float m_FOV      = 60.0f;   // degrees
        float m_NearClip = 0.1f;
        float m_FarClip  = 1000.0f;

        float m_OrthoSize = 10.0f;
        float m_OrthoNear = -1.0f;
        float m_OrthoFar  = 1.0f;

        // Viewport
        uint32_t m_ViewportWidth  = 1280;
        uint32_t m_ViewportHeight = 720;
        float    m_AspectRatio    = 16.0f / 9.0f;

        // View transform
        glm::vec3 m_Position = { 0.0f, 0.0f, 3.0f };
        glm::vec3 m_Rotation = { 0.0f, 0.0f, 0.0f }; // Euler degrees (pitch, yaw, roll)

        // Cached matrices
        glm::mat4 m_ViewMatrix       = glm::mat4(1.0f);
        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
    };

} // namespace Echelon
