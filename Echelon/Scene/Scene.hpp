#pragma once

#include "Core/Base.hpp"
#include "ECS/Entity.hpp"

namespace Echelon {
    class Scene {
    public:
        Scene();
        ~Scene();

        Entity AddEntity();
        void RemoveEntity(Entity entity);

        WeakRef<EntityRegistry> GetEntityRegistry() { return CreateWeakRef(m_EntityRegistry); }
        const EntityRegistry& GetEntityRegistry() const { return *m_EntityRegistry; }

    private:
        Ref<EntityRegistry> m_EntityRegistry;
    };
}
