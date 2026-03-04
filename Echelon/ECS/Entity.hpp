#pragma once

#include "Core/Base.hpp"
#include "entt/entt.hpp"

namespace Echelon {
    using EntityRegistry = entt::registry;
    class Scene;

    class Entity {
    public:
        Entity() = default;
        Entity(entt::entity, WeakRef<Scene>);
        ~Entity() = default;

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args);

        template<typename T>
        T& GetComponent();

        template<typename T>
        bool HasComponent();

        template<typename T>
        void RemoveComponent();

         operator bool() const { return m_EntityHandle != entt::null; }
         operator entt::entity() const { return m_EntityHandle; }
         operator uint32_t() const { return (uint32_t)m_EntityHandle; }

    private:
        entt::entity m_EntityHandle{entt::null};
        WeakRef<Scene> m_Scene;
    };
}
