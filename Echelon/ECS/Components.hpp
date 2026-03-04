#pragma once

/**
 * @file Components.hpp
 * @brief ECS component definitions with YAML serialization support.
 *
 * Best Practices:
 *  - Every component exposes Serialize(YAML::Emitter&) and a static
 *    Deserialize(const YAML::Node&) so the scene serializer can treat
 *    them uniformly.
 *  - Components are plain-old-data-like value types; avoid heavy
 *    resources inside components (use handles / asset IDs instead).
 *  - New components must implement the same Serialize / Deserialize
 *    contract to participate in scene persistence.
 */

#include "Core/UUID.hpp"

#include "glm/glm.hpp"
#include "yaml-cpp/yaml.h"

#include <string>
#include <vector>

// ---- YAML helpers for glm::vec3 ----
namespace YAML {
    template<>
    struct convert<glm::vec3> {
        static Node encode(const glm::vec3& v) {
            Node node;
            node.push_back(v.x);
            node.push_back(v.y);
            node.push_back(v.z);
            node.SetStyle(YAML::EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::vec3& v) {
            if (!node.IsSequence() || node.size() != 3)
                return false;
            v.x = node[0].as<float>();
            v.y = node[1].as<float>();
            v.z = node[2].as<float>();
            return true;
        }
    };
}

// ---- YAML emitter operator for glm::vec3 ----
namespace YAML {
    inline Emitter& operator<<(Emitter& out, const glm::vec3& v) {
        out << Flow;
        out << BeginSeq << v.x << v.y << v.z << EndSeq;
        return out;
    }
}

namespace Echelon {

    // ==================================================================
    // IDComponent
    // ==================================================================
    class IDComponent {
    public:
        UUID ID;

        IDComponent() : ID() {}
        IDComponent(const UUID& id) : ID(id) {}
        IDComponent(const IDComponent& other) : ID(other.ID) {}

        IDComponent& operator=(const IDComponent& other) {
            if (this != &other) {
                ID = other.ID;
            }
            return *this;
        }

        ~IDComponent() = default;

        IDComponent& Copy() {
            auto copy = new IDComponent(*this);
            return *copy;
        }

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "IDComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ID" << YAML::Value << static_cast<uint64_t>(ID);
            out << YAML::EndMap;
        }

        static IDComponent Deserialize(const YAML::Node& node) {
            return IDComponent(UUID(node["ID"].as<uint64_t>()));
        }
    };

    // ==================================================================
    // TransformComponent
    // ==================================================================
    class TransformComponent {
    public:
        glm::vec3 Position;
        glm::vec3 Rotation;
        glm::vec3 Scale;

        TransformComponent() 
            : Position(0.0f), Rotation(0.0f), Scale(1.0f) {}

        TransformComponent(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
            : Position(position), Rotation(rotation), Scale(scale) {}

        TransformComponent(const TransformComponent& other)
            : Position(other.Position), Rotation(other.Rotation), Scale(other.Scale) {}

        TransformComponent& operator=(const TransformComponent& other) {
            if (this != &other) {
                Position = other.Position;
                Rotation = other.Rotation;
                Scale = other.Scale;
            }
            return *this;
        }

        ~TransformComponent() = default;

        TransformComponent& Copy() {
            auto copy = new TransformComponent(*this);
            return *copy;
        }

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "TransformComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value << Position;
            out << YAML::Key << "Rotation" << YAML::Value << Rotation;
            out << YAML::Key << "Scale"    << YAML::Value << Scale;
            out << YAML::EndMap;
        }

        static TransformComponent Deserialize(const YAML::Node& node) {
            return TransformComponent(
                node["Position"].as<glm::vec3>(),
                node["Rotation"].as<glm::vec3>(),
                node["Scale"].as<glm::vec3>()
            );
        }
    };

    // ==================================================================
    // TagComponent
    // ==================================================================
    class TagComponent {
    public:
        std::string Tag;

        TagComponent() : Tag("") {}
        TagComponent(const std::string& tag) : Tag(tag) {}
        TagComponent(const TagComponent& other) : Tag(other.Tag) {}

        TagComponent& operator=(const TagComponent& other) {
            if (this != &other) {
                Tag = other.Tag;
            }
            return *this;
        }

        ~TagComponent() = default;

        TagComponent& Copy() {
            auto copy = new TagComponent(*this);
            return *copy;
        }

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "TagComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Tag" << YAML::Value << Tag;
            out << YAML::EndMap;
        }

        static TagComponent Deserialize(const YAML::Node& node) {
            return TagComponent(node["Tag"].as<std::string>());
        }
    };

    // ==================================================================
    // RelationshipComponent  (used by the Scene Graph)
    // ==================================================================
    /**
     * @brief Stores parent-child relationships for the scene graph.
     *
     * - Parent is stored as a UUID (0 / invalid means root-level).
     * - Children are stored as a vector of UUIDs.
     * - The scene graph queries these to build a hierarchy; it only
     *   rebuilds when the dirty flag is set.
     */
    class RelationshipComponent {
    public:
        uint64_t Parent = 0;                // UUID of parent entity (0 = root)
        std::vector<uint64_t> Children;     // UUIDs of child entities

        RelationshipComponent() = default;
        RelationshipComponent(uint64_t parent) : Parent(parent) {}
        RelationshipComponent(const RelationshipComponent&) = default;
        RelationshipComponent& operator=(const RelationshipComponent&) = default;
        ~RelationshipComponent() = default;

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "RelationshipComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Parent" << YAML::Value << Parent;
            out << YAML::Key << "Children" << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (auto child : Children)
                out << child;
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        static RelationshipComponent Deserialize(const YAML::Node& node) {
            RelationshipComponent rc;
            rc.Parent = node["Parent"].as<uint64_t>(0);
            if (node["Children"]) {
                for (const auto& child : node["Children"])
                    rc.Children.push_back(child.as<uint64_t>());
            }
            return rc;
        }
    };
}
