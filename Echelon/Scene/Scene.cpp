#include "Scene.hpp"
#include "ECS/Components.hpp"
#include "Scene/TransformUtils.hpp"

#include "glm/gtc/matrix_transform.hpp"

#include <algorithm>
#include <vector>

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

    void Scene::DestroyEntity(Entity entity, bool destroyChildren) {
        if (!entity)
            return;

        // Unlink from the parent's child list so no dangling UUID remains.
        DetachFromParent(entity);

        // Snapshot children before mutating — destroying entities swap-and-pops the
        // component pools, which would invalidate the live Children reference.
        std::vector<UUID> children = entity.GetComponent<RelationshipComponent>().Children;
        for (const auto& childUUID : children) {
            Entity childEntity = FindEntityByUUID(childUUID);
            if (!childEntity)
                continue;
            if (destroyChildren)
                DestroyEntity(childEntity, true);       // recurse: remove the whole subtree
            else
                DetachFromParent(childEntity);          // promote surviving children to root
        }

        m_EntityRegistry->destroy(static_cast<entt::entity>(entity));
        m_SceneGraph.MarkDirty();
    }

    void Scene::Clear() {
        m_EntityRegistry->clear();
        m_SceneGraph.MarkDirty();
    }

    // ------------------------------------------------------------------
    // Hierarchy helpers
    // ------------------------------------------------------------------
    bool Scene::SetParent(Entity child, Entity parent) {
        if (!child || !parent)
            return false;

        // Reject cycles: parenting under self or one of the child's descendants would
        // corrupt the graph and hang any hierarchy traversal.
        if (IsAncestorOf(child, parent))
            return false;

        // Detach from any previous parent first so the child is never listed twice.
        DetachFromParent(child);

        auto childUUID  = child.GetComponent<IDComponent>().ID;
        auto parentUUID = parent.GetComponent<IDComponent>().ID;

        child.GetComponent<RelationshipComponent>().Parent = parentUUID;

        auto& parentRC = parent.GetComponent<RelationshipComponent>();
        if (std::find(parentRC.Children.begin(), parentRC.Children.end(), childUUID)
                == parentRC.Children.end()) {
            parentRC.Children.push_back(childUUID);
        }

        m_SceneGraph.MarkDirty();
        return true;
    }

    void Scene::ReparentKeepingWorldTransform(Entity child, Entity newParent) {
        if (!child)
            return;
        // Bail on cycles before touching the transform so we never leave the child in
        // a half-reparented state.
        if (newParent && IsAncestorOf(child, newParent))
            return;

        const glm::mat4 childWorld = GetWorldTransform(child);

        glm::mat4 newLocal;
        if (newParent) {
            const glm::mat4 parentWorld = GetWorldTransform(newParent);
            newLocal = glm::inverse(parentWorld) * childWorld;
        } else {
            newLocal = childWorld;   // detaching to root: local space == world space
        }

        DecomposeToTransform(newLocal, child.GetComponent<TransformComponent>());

        if (newParent)
            SetParent(child, newParent);
        else
            DetachFromParent(child);
    }

    glm::mat4 Scene::GetWorldTransform(Entity entity) {
        if (!entity)
            return glm::mat4(1.0f);
        return ComputeWorldTransform(*m_EntityRegistry, static_cast<entt::entity>(entity));
    }

    bool Scene::GetWorldTRS(Entity entity, glm::vec3& position, glm::vec3& eulerDegrees, glm::vec3& scale) {
        if (!entity)
            return false;
        TransformComponent world;
        DecomposeToTransform(GetWorldTransform(entity), world);
        position     = world.Position;
        eulerDegrees = world.Rotation;
        scale        = world.Scale;
        return true;
    }

    bool Scene::IsAncestorOf(Entity ancestor, Entity node) {
        if (!ancestor || !node)
            return false;

        const UUID ancestorUUID = ancestor.GetComponent<IDComponent>().ID;
        Entity cur = node;
        int guard = 0;
        while (cur && guard++ < 4096) {
            if (cur.GetComponent<IDComponent>().ID == ancestorUUID)
                return true;
            auto& rc = cur.GetComponent<RelationshipComponent>();
            if (!rc.Parent.has_value())
                break;
            cur = FindEntityByUUID(*rc.Parent);
        }
        return false;
    }

    void Scene::DetachFromParent(Entity entity) {
        auto& rc = entity.GetComponent<RelationshipComponent>();
        if (!rc.Parent.has_value())
            return;

        // Remove from previous parent's children list
        auto parentEntity = FindEntityByUUID(*rc.Parent);
        if (parentEntity) {
            auto& parentRC = parentEntity.GetComponent<RelationshipComponent>();
            auto childUUID = entity.GetComponent<IDComponent>().ID;
            parentRC.Children.erase(
                std::remove(parentRC.Children.begin(), parentRC.Children.end(), childUUID),
                parentRC.Children.end()
            );
        }

        rc.Parent = std::nullopt;
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
