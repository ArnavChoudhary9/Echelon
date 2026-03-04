#pragma once

/**
 * @file SceneGraph.hpp
 * @brief Lazy-evaluated hierarchical scene graph.
 *
 * Best Practices:
 *  - The graph is only rebuilt when explicitly marked dirty via
 *    MarkDirty().  This avoids per-frame O(n) rebuilds when the
 *    hierarchy hasn't changed.
 *  - Scene operations that alter parent-child relationships MUST
 *    call MarkDirty() to keep the graph consistent.
 *  - Traversal helpers (TraverseDFS, GetChildren, GetParent) are
 *    O(1)-per-node once the graph is up to date.
 *  - The graph stores lightweight UUID keys — no raw pointers to
 *    entt handles — so it survives entity re-ordering safely.
 */

#include "Core/Base.hpp"
#include "Core/UUID.hpp"
#include "ECS/Entity.hpp"

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Echelon {

    /**
     * @brief A single node in the scene hierarchy.
     */
    struct SceneGraphNode {
        uint64_t EntityUUID  = 0;                   // Entity this node represents
        uint64_t ParentUUID  = 0;                   // 0 == root level
        std::vector<uint64_t> ChildrenUUIDs;        // Direct children
    };

    /**
     * @brief Cached, lazily-rebuilt scene hierarchy.
     *
     * Ownership: held by Scene. Rebuilt from RelationshipComponents
     * only when the dirty flag is set.
     */
    class SceneGraph {
    public:
        SceneGraph() = default;
        ~SceneGraph() = default;

        // ---- Dirty flag ----

        /** Mark the graph as needing a rebuild. */
        void MarkDirty() { m_IsDirty = true; }

        /** Check whether the graph needs rebuilding. */
        bool IsDirty() const { return m_IsDirty; }

        // ---- Rebuild ----

        /**
         * @brief Rebuild the cached graph from current RelationshipComponents.
         *        Called automatically by accessors when dirty.
         * 
         * @param registry Reference to the entity registry.
         */
        void Rebuild(EntityRegistry& registry);

        // ---- Queries (auto-rebuild if dirty) ----

        /** Get the root-level entity UUIDs (entities with no parent). */
        const std::vector<uint64_t>& GetRoots(EntityRegistry& registry);

        /** Get children UUIDs of a given entity. */
        const std::vector<uint64_t>& GetChildren(uint64_t entityUUID, EntityRegistry& registry);

        /** Get parent UUID of a given entity (0 if root). */
        uint64_t GetParent(uint64_t entityUUID, EntityRegistry& registry);

        // ---- Traversal ----

        /**
         * @brief Depth-first traversal of the hierarchy.
         * @param callback  Called for each node with (uuid, depth).
         */
        void TraverseDFS(EntityRegistry& registry,
                         const std::function<void(uint64_t uuid, int depth)>& callback);

    private:
        void EnsureUpToDate(EntityRegistry& registry);
        void TraverseNode(uint64_t uuid, int depth,
                          const std::function<void(uint64_t, int)>& callback) const;

        bool m_IsDirty = true;

        std::vector<uint64_t> m_RootEntities;                             // Root-level entities
        std::unordered_map<uint64_t, SceneGraphNode> m_Nodes;             // UUID -> node
        static const std::vector<uint64_t> s_EmptyChildren;               // Returned when a UUID has no children
    };
}
