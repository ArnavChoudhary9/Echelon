#pragma once

/**
 * @file MeshVertex.hpp
 * @brief Canonical interleaved vertex format for Mesh assets.
 */

#include "glm/glm.hpp"

namespace Echelon {

    /**
     * @brief One interleaved mesh vertex.
     *
     * IMPORTANT: this layout must match the renderer's default "Flat" pipeline
     * (a_Position: loc 0, Float3, offset 0; a_Color: loc 1, Float3, offset 12;
     * stride 24 bytes). See Ray/RayRenderer.cpp::CreateDefaultResources. When
     * richer pipelines are added, extend this struct and carry a VertexLayout on
     * the Mesh so importers/pipelines stay in sync.
     */
    struct MeshVertex {
        glm::vec3 Position{ 0.0f };
        glm::vec3 Color{ 1.0f };
    };

} // namespace Echelon
