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

        /** @brief A unit cube ([-0.5, 0.5]) with per-face normals and 0..1 UVs. */
        Ref<Mesh> CreateCube();

        /** @brief A unit ground plane on XZ ([-0.5,0.5], y=0), +Y normal, 0..1 UVs. */
        Ref<Mesh> CreatePlane();

        /**
         * @brief A UV sphere of radius 0.5 (unit diameter) with smooth outward normals
         *        and lat/long UVs — ideal for showcasing PBR metallic/roughness.
         * @param segments  Longitude/latitude subdivision (default 32).
         */
        Ref<Mesh> CreateSphere(uint32_t segments = 32);

        // Extend here: CreateQuad(), CreateCylinder(), ...

    } // namespace MeshPrimitives

} // namespace Echelon
