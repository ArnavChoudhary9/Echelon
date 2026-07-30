#pragma once

/**
 * @file Primitives.hpp
 * @brief Procedural built-in mesh shapes ("the internal shape repository").
 *
 * These are generated in C++ so they are always available regardless of any
 * files on disk. Register them with AssetManager::RegisterPrimitive (the engine
 * registers "Cube" at startup); user code can add more the same way.
 */

#include "Core/Base.hpp"

namespace Echelon {

    class Mesh;

    namespace MeshPrimitives {

        /** @brief A unit cube ([-0.5, 0.5]) with per-corner colors. */
        Ref<Mesh> CreateCube();

        // Extend here: CreatePlane(), CreateSphere(), CreateQuad(), ...

    } // namespace MeshPrimitives

} // namespace Echelon
