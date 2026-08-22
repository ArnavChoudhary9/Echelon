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
#include "Asset/Asset.hpp"
#include "ECS/Entity.hpp"
#include "Scene/SceneGraph.hpp"

#include "glm/glm.hpp"

#include <string>

namespace Echelon {
    class Scene : public Asset {
    public:
        Scene(const std::string& name = "Untitled Scene");
        ~Scene() override;

        // ---- Asset interface ----
        AssetType GetType() const override { return AssetType::Scene; }
        bool IsValid() const override { return true; }

        // ---- Entity management ----

        Entity AddEntity(const std::string& name);

        /**
         * @brief Add an entity with a specific UUID (used during deserialization).
         */
        Entity AddEntityWithUUID(UUID uuid, const std::string& name);

        void RemoveEntity(const std::string& name);
        void RemoveEntity(Entity entity);

        /**
         * @brief Destroy an entity.
         * @param entity          The entity to destroy.
         * @param destroyChildren When true (default) the entire subtree is removed;
         *                        when false, children are promoted to root level.
         *
         * Unlike RemoveEntity, this also unlinks the entity from its parent's child
         * list so no dangling UUID references remain in the scene graph.
         */
        void DestroyEntity(Entity entity, bool destroyChildren = true);

        /**
         * @brief Destroy all entities in the scene.
         */
        void Clear();

        // ---- Hierarchy helpers ----

        /**
         * @brief Set a parent-child relationship between two entities.
         *
         * The child is first detached from any previous parent, and the operation is
         * rejected (returns false, no change) if it would create a cycle — i.e. if
         * `parent` is `child` itself or one of its descendants.
         *
         * NOTE: this treats the child's TransformComponent as already-local and does
         * NOT preserve world position. Use ReparentKeepingWorldTransform() for the
         * editor drag-drop behaviour where the child should stay put visually.
         *
         * @param child  The entity to become a child.
         * @param parent The entity to become the parent.
         * @return true on success, false if the relationship would form a cycle.
         */
        bool SetParent(Entity child, Entity parent);

        /**
         * @brief Remove the parent of an entity, making it a root entity.
         */
        void DetachFromParent(Entity entity);

        /**
         * @brief Reparent `child` under `newParent`, keeping its world transform.
         *
         * The child's local TransformComponent is recomputed so the entity does not
         * move visually. Passing an invalid `newParent` detaches the child to root.
         * Cycle-forming reparents are ignored.
         */
        void ReparentKeepingWorldTransform(Entity child, Entity newParent);

        /**
         * @brief Compute an entity's world transform (its parent chain composed).
         * @return The world matrix, or identity for an invalid entity.
         */
        glm::mat4 GetWorldTransform(Entity entity);

        /**
         * @brief Decompose an entity's world transform into position / euler-degrees
         *        rotation / scale. Convenience for consumers (cameras, gizmos) that
         *        need world TRS without pulling in the matrix-decompose headers.
         * @return false (outputs left untouched) for an invalid entity.
         */
        bool GetWorldTRS(Entity entity, glm::vec3& position, glm::vec3& eulerDegrees, glm::vec3& scale);

        /**
         * @brief Test whether `ancestor` is `node` itself or an ancestor of `node`.
         */
        bool IsAncestorOf(Entity ancestor, Entity node);

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

