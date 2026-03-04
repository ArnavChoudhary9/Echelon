#include "Entity.hpp"
#include "Echelon/Scene/Scene.hpp"

namespace Echelon {
    Entity::Entity(entt::entity handle, WeakRef<Scene> scene)
        : m_EntityHandle(handle), m_Scene(scene) {}

    EntityRegistry& Entity::GetRegistry() {
        return *m_Scene.lock()->GetEntityRegistry().lock();
    }
}
