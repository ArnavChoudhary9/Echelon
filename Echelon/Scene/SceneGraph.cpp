#include "SceneGraph.hpp"
#include "ECS/Components.hpp"
#include "entt/entt.hpp"

namespace Echelon {

    const std::vector<uint64_t> SceneGraph::s_EmptyChildren = {};

    // ------------------------------------------------------------------
    // Rebuild
    // ------------------------------------------------------------------
    void SceneGraph::Rebuild(EntityRegistry& registry) {
        m_Nodes.clear();
        m_RootEntities.clear();

        // 1. Build a node for every entity that has an IDComponent.
        auto idView = registry.view<IDComponent>();
        for (auto&& [entity, id] : idView.each()) {
            uint64_t uuid  = static_cast<uint64_t>(id.ID);

            SceneGraphNode node;
            node.EntityUUID = uuid;

            // If it has a RelationshipComponent, populate parent/children.
            if (registry.any_of<RelationshipComponent>(entity)) {
                const auto& rc = registry.get<RelationshipComponent>(entity);
                node.ParentUUID   = rc.Parent;
                node.ChildrenUUIDs = rc.Children;
            }

            m_Nodes[uuid] = std::move(node);
        }

        // 2. Determine root entities (Parent == 0 or parent not found).
        for (const auto& [uuid, node] : m_Nodes) {
            if (node.ParentUUID == 0 || m_Nodes.find(node.ParentUUID) == m_Nodes.end()) {
                m_RootEntities.push_back(uuid);
            }
        }

        m_IsDirty = false;
    }

    // ------------------------------------------------------------------
    // Lazy helpers
    // ------------------------------------------------------------------
    void SceneGraph::EnsureUpToDate(EntityRegistry& registry) {
        if (m_IsDirty) {
            Rebuild(registry);
        }
    }

    const std::vector<uint64_t>& SceneGraph::GetRoots(EntityRegistry& registry) {
        EnsureUpToDate(registry);
        return m_RootEntities;
    }

    const std::vector<uint64_t>& SceneGraph::GetChildren(uint64_t entityUUID, EntityRegistry& registry) {
        EnsureUpToDate(registry);
        auto it = m_Nodes.find(entityUUID);
        if (it != m_Nodes.end())
            return it->second.ChildrenUUIDs;
        return s_EmptyChildren;
    }

    uint64_t SceneGraph::GetParent(uint64_t entityUUID, EntityRegistry& registry) {
        EnsureUpToDate(registry);
        auto it = m_Nodes.find(entityUUID);
        if (it != m_Nodes.end())
            return it->second.ParentUUID;
        return 0;
    }

    // ------------------------------------------------------------------
    // DFS Traversal
    // ------------------------------------------------------------------
    void SceneGraph::TraverseDFS(EntityRegistry& registry,
                                  const std::function<void(uint64_t uuid, int depth)>& callback) {
        EnsureUpToDate(registry);
        for (auto rootUUID : m_RootEntities) {
            TraverseNode(rootUUID, 0, callback);
        }
    }

    void SceneGraph::TraverseNode(uint64_t uuid, int depth,
                                   const std::function<void(uint64_t, int)>& callback) const {
        callback(uuid, depth);
        auto it = m_Nodes.find(uuid);
        if (it != m_Nodes.end()) {
            for (auto childUUID : it->second.ChildrenUUIDs) {
                TraverseNode(childUUID, depth + 1, callback);
            }
        }
    }
}
