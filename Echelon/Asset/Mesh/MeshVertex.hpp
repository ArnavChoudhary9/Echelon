#pragma once

/**
 * @file MeshVertex.hpp
 * @brief The single canonical interleaved vertex format for Mesh assets.
 *
 * There is exactly ONE vertex format in the engine. The mesh loader always fills
 * these fields and never knows about any specific shader's layout — a shader's
 * reflected vertex inputs are matched onto this canonical vertex by *location*
 * (see StandardVertex.hpp). Location convention: Position=0, Normal=1, TexCoord=2.
 */

#include "glm/glm.hpp"

namespace Echelon {

    /** @brief One interleaved mesh vertex (32 bytes). */
    struct MeshVertex {
        glm::vec3 Position{ 0.0f };   // location 0
        glm::vec3 Normal{ 0.0f };     // location 1
        glm::vec2 TexCoord{ 0.0f };   // location 2
    };

} // namespace Echelon
