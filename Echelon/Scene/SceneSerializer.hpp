#pragma once

/**
 * @file SceneSerializer.hpp
 * @brief YAML-based scene serialization / deserialization (.ehscene).
 *
 * Best Practices:
 *  - The serializer is decoupled from Scene so that serialization
 *    format can change without modifying game logic.
 *  - Every component type must be explicitly handled in
 *    SerializeEntity / DeserializeEntity.  Add new component
 *    blocks here when new components are introduced.
 *  - Scene files are human-readable YAML; avoid binary blobs.
 */

#include "Core/Base.hpp"
#include "ECS/Entity.hpp"

#include "yaml-cpp/yaml.h"

#include <string>
#include <filesystem>

namespace Echelon {
    class Scene;

    class SceneSerializer {
    public:
        explicit SceneSerializer(const Ref<Scene>& scene);
        ~SceneSerializer() = default;

        /**
         * @brief Serialize the bound scene to a .ehscene YAML file.
         * @param filepath Full path to the output file.
         * @return true on success.
         */
        bool Serialize(const std::filesystem::path& filepath) const;

        /**
         * @brief Deserialize a .ehscene YAML file into the bound scene.
         *        Existing entities in the scene are cleared first.
         * @param filepath Full path to the input file.
         * @return true on success.
         */
        bool Deserialize(const std::filesystem::path& filepath);

    private:
        void SerializeEntity(Entity entity, YAML::Emitter& out) const;
        Ref<Scene> m_Scene;
    };
}
