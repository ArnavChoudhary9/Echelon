#include "Scene.hpp"
#include "ECS/Components.hpp"

#include <algorithm>

namespace Echelon {

    Scene::Scene(const std::string& name)
        : m_Name(name)
    {
        m_EntityRegistry = CreateRef<EntityRegistry>();
        m_SelfRef = Ref<Scene>(this, [](Scene*) {});
    }

    Scene::~Scene() = default;

    // ------------------------------------------------------------------
    // Entity management
    // ------------------------------------------------------------------
    Entity Scene::AddEntity(const std::string& name) {
        return AddEntityWithUUID(UUID(), name);
    }

    Entity Scene::AddEntityWithUUID(UUID uuid, const std::string& name) {
        entt::entity entityHandle = m_EntityRegistry->create();
        Entity entity(entityHandle, CreateWeakRef(m_SelfRef));

        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<TagComponent>(name);
        entity.AddComponent<RelationshipComponent>();

        m_SceneGraph.MarkDirty();
        return entity;
    }

    void Scene::RemoveEntity(const std::string& name) {
        auto view = m_EntityRegistry->view<TagComponent>();
        for (auto&& [entity, tag] : view.each()) {
            if (tag.Tag == name) {
                m_EntityRegistry->destroy(entity);
                m_SceneGraph.MarkDirty();
                return;
            }
        }
    }

    void Scene::RemoveEntity(Entity entity) {
        m_EntityRegistry->destroy(entity);
        m_SceneGraph.MarkDirty();
    }

    void Scene::Clear() {
        m_EntityRegistry->clear();
        m_SceneGraph.MarkDirty();
    }

    // ------------------------------------------------------------------
    // Hierarchy helpers
    // ------------------------------------------------------------------
    void Scene::SetParent(Entity child, Entity parent) {
        auto childUUID  = static_cast<uint64_t>(child.GetComponent<IDComponent>().ID);
        auto parentUUID = static_cast<uint64_t>(parent.GetComponent<IDComponent>().ID);

        // Update child's relationship
        auto& childRC   = child.GetComponent<RelationshipComponent>();
        childRC.Parent  = parentUUID;

        // Add child to parent's children list (avoid duplicates)
        auto& parentRC = parent.GetComponent<RelationshipComponent>();
        auto it = std::find(parentRC.Children.begin(), parentRC.Children.end(), childUUID);
        if (it == parentRC.Children.end()) {
            parentRC.Children.push_back(childUUID);
        }

        m_SceneGraph.MarkDirty();
    }

    void Scene::DetachFromParent(Entity entity) {
        auto& rc = entity.GetComponent<RelationshipComponent>();
        if (rc.Parent == 0)
            return;

        // Remove from previous parent's children list
        auto parentEntity = FindEntityByUUID(UUID(rc.Parent));
        if (parentEntity) {
            auto& parentRC = parentEntity.GetComponent<RelationshipComponent>();
            auto childUUID = static_cast<uint64_t>(entity.GetComponent<IDComponent>().ID);
            parentRC.Children.erase(
                std::remove(parentRC.Children.begin(), parentRC.Children.end(), childUUID),
                parentRC.Children.end()
            );
        }

        rc.Parent = 0;
        m_SceneGraph.MarkDirty();
    }

    // ------------------------------------------------------------------
    // Lookup
    // ------------------------------------------------------------------
    Entity Scene::FindEntityByUUID(UUID uuid) {
        auto view = m_EntityRegistry->view<IDComponent>();
        for (auto&& [entity, id] : view.each()) {
            if (id.ID == uuid) {
                return Entity(entity, CreateWeakRef(m_SelfRef));
            }
        }
        return Entity(); // Invalid
    }
}
