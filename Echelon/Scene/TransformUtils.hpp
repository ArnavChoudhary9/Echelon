#pragma once

/**
 * @file TransformUtils.hpp
 * @brief Canonical local/world transform composition for the ECS.
 *
 * A TransformComponent stores a LOCAL transform (position, euler-degrees rotation,
 * scale) expressed relative to its parent — or relative to world space when the
 * entity has no parent. The world transform of an entity is therefore the product
 * of its ancestors' local transforms, outermost first:
 *
 *     world = parentWorld * localTRS
 *
 * The renderer (RenderGraph) and the editor (drag-drop reparenting math) MUST agree
 * on this convention, so it is defined in exactly one place here.
 */

#define GLM_ENABLE_EXPERIMENTAL

#include "ECS/Components.hpp"
#include "entt/entt.hpp"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/matrix_decompose.hpp"

namespace Echelon {

    /** @brief Compose an entity's LOCAL transform matrix (T * R * S). Rotation is euler degrees. */
    inline glm::mat4 ComposeLocalTransform(const TransformComponent& tc) {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), tc.Position);
        glm::mat4 r = glm::toMat4(glm::quat(glm::radians(tc.Rotation)));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), tc.Scale);
        return t * r * s;
    }

    /**
     * @brief Decompose a (local) transform matrix back into a TransformComponent.
     *
     * Used when reparenting keeps an entity fixed in world space: the newly derived
     * local matrix is decomposed into position / euler-degrees rotation / scale.
     * Shear introduced by non-uniform parent scale cannot be represented by TRS and
     * is discarded — an accepted limitation for editor reparenting.
     */
    inline void DecomposeToTransform(const glm::mat4& m, TransformComponent& tc) {
        glm::vec3 scale(1.0f), translation(0.0f), skew(0.0f);
        glm::vec4 perspective(0.0f);
        glm::quat orientation(1.0f, 0.0f, 0.0f, 0.0f);
        if (glm::decompose(m, scale, orientation, translation, skew, perspective)) {
            tc.Position = translation;
            tc.Scale    = scale;
            tc.Rotation = glm::degrees(glm::eulerAngles(orientation));
        }
    }

    namespace Detail {
        /** Linear UUID -> entity lookup — acceptable at editor scene scale. */
        inline entt::entity FindEntityByUUID(entt::registry& reg, const UUID& uuid) {
            auto view = reg.view<IDComponent>();
            for (auto e : view)
                if (view.get<IDComponent>(e).ID == uuid)
                    return e;
            return entt::null;
        }
    }

    /**
     * @brief Compute an entity's WORLD transform by walking its parent chain.
     *
     * Non-memoized: O(depth) with an O(n) parent lookup per hop — fine for one-off
     * editor queries (hot render paths build their own memoized map instead). A depth
     * cap guards against a corrupt / cyclic hierarchy from a hand-edited scene file.
     */
    inline glm::mat4 ComputeWorldTransform(entt::registry& reg, entt::entity e) {
        glm::mat4 world(1.0f);
        int guard = 0;
        while (e != entt::null && reg.valid(e) && reg.all_of<TransformComponent>(e) && guard++ < 4096) {
            world = ComposeLocalTransform(reg.get<TransformComponent>(e)) * world;
            const auto* rel = reg.try_get<RelationshipComponent>(e);
            if (!rel || !rel->Parent.has_value())
                break;
            e = Detail::FindEntityByUUID(reg, *rel->Parent);
        }
        return world;
    }
}
