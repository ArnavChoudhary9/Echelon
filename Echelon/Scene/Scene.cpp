#include "Scene.hpp"

namespace Echelon {
    Scene::Scene() {
        m_EntityRegistry = CreateRef<EntityRegistry>();
        m_SelfRef = CreateRef<Scene>(*this);
    }

    Scene::~Scene() = default;

    Entity Scene::AddEntity() {
        entt::entity entityHandle = m_EntityRegistry->create();
        Entity entity(entityHandle, CreateWeakRef(m_SelfRef));
        return entity;
    }

    void Scene::RemoveEntity(Entity entity) {
        m_EntityRegistry->destroy(entity);
    }
}
