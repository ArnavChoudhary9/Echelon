#pragma once

#include "Core/Base.hpp"
#include <cassert>
#include "entt/entt.hpp"

#ifndef EC_CORE_ASSERT
    #define EC_CORE_ASSERT(x, msg) assert((x) && (msg))
#endif

namespace Echelon {
    using EntityRegistry = entt::registry;
    class Scene;

    class Entity {
    public:
        Entity() = default;
        Entity(entt::entity, WeakRef<Scene>);
        ~Entity() = default;

        template<typename T, typename... Args>
        T& AddComponent(Args&&... args) {
            EC_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
            return GetRegistry().template emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
        }

        template<typename T>
        T& GetComponent() {
            EC_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
            return GetRegistry().template get<T>(m_EntityHandle);
        }

        template<typename T>
        bool HasComponent() {
            return GetRegistry().template any_of<T>(m_EntityHandle);
        }

        template<typename T>
        void RemoveComponent() {
            EC_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
            GetRegistry().template remove<T>(m_EntityHandle);
        }

         operator bool() const { return m_EntityHandle != entt::null; }
         operator entt::entity() const { return m_EntityHandle; }
         operator uint32_t() const { return (uint32_t)m_EntityHandle; }

    private:
        EntityRegistry& GetRegistry();

        entt::entity m_EntityHandle{entt::null};
        WeakRef<Scene> m_Scene;
    };
}
