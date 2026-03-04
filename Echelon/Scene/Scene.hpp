#pragma once

/**
 * @file Scene.hpp
 * @brief Scene container — owns entities, holds the scene graph, and
 *        provides helpers for serialization.
 *
 * Best Practices:
 *  - Use AddEntity / RemoveEntity for all mutations so the dirty flag
 *    on the scene graph is maintained automatically.
 *  - Scene name is metadata used for display & file naming; it does
 *    not affect runtime behaviour.
 *  - The scene graph is lazily rebuilt — call MarkSceneGraphDirty()
 *    only when parent-child relationships change (the Scene helpers
 *    do this for you).
 */

#include "Core/Base.hpp"
#include "Core/UUID.hpp"
#include "ECS/Entity.hpp"
#include "Scene/SceneGraph.hpp"

#include <string>

namespace Echelon {
    class Scene {
    public:
        Scene(const std::string& name = "Untitled Scene");
        ~Scene();

        // ---- Entity management ----

        Entity AddEntity(const std::string& name);

        /**
         * @brief Add an entity with a specific UUID (used during deserialization).
         */
        Entity AddEntityWithUUID(UUID uuid, const std::string& name);

        void RemoveEntity(const std::string& name);
        void RemoveEntity(Entity entity);

        /**
         * @brief Destroy all entities in the scene.
         */
        void Clear();

        // ---- Hierarchy helpers ----

        /**
         * @brief Set a parent-child relationship between two entities.
         * @param child  The entity to become a child.
         * @param parent The entity to become the parent.
         */
        void SetParent(Entity child, Entity parent);

        /**
         * @brief Remove the parent of an entity, making it a root entity.
         */
        void DetachFromParent(Entity entity);

        // ---- Scene Graph ----

        SceneGraph& GetSceneGraph() { return m_SceneGraph; }
        const SceneGraph& GetSceneGraph() const { return m_SceneGraph; }
        void MarkSceneGraphDirty() { m_SceneGraph.MarkDirty(); }

        // ---- Metadata ----

        const std::string& GetName() const { return m_Name; }
        void SetName(const std::string& name) { m_Name = name; }

        // ---- Registry access ----

        WeakRef<EntityRegistry> GetEntityRegistry() { return CreateWeakRef(m_EntityRegistry); }
        const EntityRegistry& GetEntityRegistry() const { return *m_EntityRegistry; }

        /**
         * @brief Find an entity by its UUID.
         * @return The entity, or an invalid Entity if not found.
         */
        Entity FindEntityByUUID(UUID uuid);

    private:
        std::string m_Name;
        Ref<EntityRegistry> m_EntityRegistry;
        Ref<Scene> m_SelfRef;
        SceneGraph m_SceneGraph;
    };
}

