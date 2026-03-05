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
#include "GraphicsAPI/Buffer.hpp"
#include "Renderer/Camera.hpp"

#include "glm/glm.hpp"
#include "yaml-cpp/yaml.h"

#include <string>
#include <vector>
#include <cstdint>

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

// ---- YAML helpers for glm::vec4 ----
namespace YAML {
    template<>
    struct convert<glm::vec4> {
        static Node encode(const glm::vec4& v) {
            Node node;
            node.push_back(v.x);
            node.push_back(v.y);
            node.push_back(v.z);
            node.push_back(v.w);
            node.SetStyle(YAML::EmitterStyle::Flow);
            return node;
        }

        static bool decode(const Node& node, glm::vec4& v) {
            if (!node.IsSequence() || node.size() != 4)
                return false;
            v.x = node[0].as<float>();
            v.y = node[1].as<float>();
            v.z = node[2].as<float>();
            v.w = node[3].as<float>();
            return true;
        }
    };
}

// ---- YAML emitter operator for glm::vec4 ----
namespace YAML {
    inline Emitter& operator<<(Emitter& out, const glm::vec4& v) {
        out << Flow;
        out << BeginSeq << v.x << v.y << v.z << v.w << EndSeq;
        return out;
    }
}

namespace Echelon {

    // Forward declarations
    class Pipeline;

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

    // ==================================================================
    // MeshComponent  (runtime GPU mesh data)
    // ==================================================================
    /**
     * @brief Holds the raw GPU mesh data (vertex buffer, index buffer) and
     *        associated metadata needed for rendering.
     *
     * This is a runtime-only component — GPU handles are not serialized.
     * The serializer stores a MeshSource tag (e.g. "Triangle", "Cube") so
     * meshes can be reconstructed on load.
     *
     * The component also carries a version counter that is bumped whenever
     * the mesh data changes.  The RenderGraph uses this to detect stale
     * command-buffer recordings cheaply (O(1) per entity).
     */
    class MeshComponent {
    public:
        Ref<Buffer>  VertexBuffer  = nullptr;   ///< GPU vertex buffer
        Ref<Buffer>  IndexBuffer   = nullptr;   ///< GPU index buffer (optional)
        uint32_t     VertexCount   = 0;         ///< Number of vertices
        uint32_t     IndexCount    = 0;         ///< Number of indices (0 = non-indexed)
        std::string  MeshSource    = "";        ///< Tag for serialization / reconstruction

        /** Bumped whenever VB/IB or counts change.  Cheap dirty check. */
        uint64_t     Version       = 0;

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
        MeshComponent& operator=(const MeshComponent&) = default;
        ~MeshComponent() = default;

        /** Convenience: is the mesh ready to render? */
        bool IsValid() const { return VertexBuffer != nullptr && VertexCount > 0; }

        /** Bump the version — call after modifying VB/IB/counts. */
        void Invalidate() { ++Version; }

        // ---- Serialization (only the source tag — GPU data is transient) ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "MeshComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "MeshSource"  << YAML::Value << MeshSource;
            out << YAML::Key << "VertexCount" << YAML::Value << VertexCount;
            out << YAML::Key << "IndexCount"  << YAML::Value << IndexCount;
            out << YAML::EndMap;
        }

        static MeshComponent Deserialize(const YAML::Node& node) {
            MeshComponent mc;
            mc.MeshSource  = node["MeshSource"].as<std::string>("");
            mc.VertexCount = node["VertexCount"].as<uint32_t>(0);
            mc.IndexCount  = node["IndexCount"].as<uint32_t>(0);
            return mc;
        }
    };

    // ==================================================================
    // CameraComponent
    // ==================================================================
    /**
     * @brief Attaches camera data to an entity.
     *
     * The Camera object does the heavy math (view / projection).
     * CameraComponent wraps it so it can live in the ECS and be
     * serialized with the scene.
     */
    class CameraComponent {
    public:
        Camera  Cam;
        bool    Primary    = true;   ///< Is this the active scene camera?
        bool    FixedAspect = false; ///< Lock aspect ratio on resize?

        CameraComponent() = default;
        CameraComponent(const CameraComponent&) = default;
        CameraComponent& operator=(const CameraComponent&) = default;
        ~CameraComponent() = default;

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "CameraComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Primary"        << YAML::Value << Primary;
            out << YAML::Key << "FixedAspect"    << YAML::Value << FixedAspect;
            out << YAML::Key << "ProjectionType" << YAML::Value << static_cast<int>(Cam.GetProjectionType());
            out << YAML::Key << "FOV"            << YAML::Value << Cam.GetFOV();
            out << YAML::Key << "NearClip"       << YAML::Value << Cam.GetNearClip();
            out << YAML::Key << "FarClip"        << YAML::Value << Cam.GetFarClip();
            out << YAML::Key << "OrthoSize"      << YAML::Value << Cam.GetOrthoSize();
            out << YAML::Key << "OrthoNear"      << YAML::Value << Cam.GetOrthoNearClip();
            out << YAML::Key << "OrthoFar"       << YAML::Value << Cam.GetOrthoFarClip();
            out << YAML::EndMap;
        }

        static CameraComponent Deserialize(const YAML::Node& node) {
            CameraComponent cc;
            cc.Primary     = node["Primary"].as<bool>(true);
            cc.FixedAspect = node["FixedAspect"].as<bool>(false);

            int projType = node["ProjectionType"].as<int>(0);
            if (projType == 1) {
                cc.Cam.SetOrthographic(
                    node["OrthoSize"].as<float>(10.0f),
                    node["OrthoNear"].as<float>(-1.0f),
                    node["OrthoFar"].as<float>(1.0f)
                );
            } else {
                cc.Cam.SetPerspective(
                    node["FOV"].as<float>(60.0f),
                    node["NearClip"].as<float>(0.1f),
                    node["FarClip"].as<float>(1000.0f)
                );
            }
            return cc;
        }
    };

    // ==================================================================
    // MaterialComponent
    // ==================================================================
    /**
     * @brief Describes the visual material of an entity.
     *
     * Holds a reference to a Pipeline (defines the shader / render state)
     * and per-instance parameters (colours, roughness, etc.).
     *
     * The pipeline pointer is transient (not serialized).  The serializer
     * persists the ShaderName tag so the application can reassign the
     * correct pipeline on load.
     *
     * MaterialComponent carries a Version counter so the RenderGraph can
     * detect when an entity switches pipeline (expensive) vs only changes
     * uniform data within the same pipeline (cheap).
     */
    class MaterialComponent {
    public:
        // ---- Pipeline (shader + render state) ----
        Ref<Pipeline> PipelineRef = nullptr;     ///< GPU pipeline (transient)
        std::string   ShaderName  = "Flat";      ///< Tag for serialization

        // ---- Per-instance parameters ----
        glm::vec4  AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float      Roughness   = 0.5f;
        float      Metallic    = 0.0f;

        /** Bumped on any material change.  Cheap dirty check. */
        uint64_t   Version     = 0;

        MaterialComponent() = default;
        MaterialComponent(const MaterialComponent&) = default;
        MaterialComponent& operator=(const MaterialComponent&) = default;
        ~MaterialComponent() = default;

        /** Bump the version — call after changing pipeline or parameters. */
        void Invalidate() { ++Version; }

        /** Sort key: pointer identity of the pipeline, for batching. */
        uintptr_t GetPipelineSortKey() const {
            return reinterpret_cast<uintptr_t>(PipelineRef.get());
        }

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "MaterialComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ShaderName"  << YAML::Value << ShaderName;
            out << YAML::Key << "AlbedoColor" << YAML::Value << AlbedoColor;
            out << YAML::Key << "Roughness"   << YAML::Value << Roughness;
            out << YAML::Key << "Metallic"    << YAML::Value << Metallic;
            out << YAML::EndMap;
        }

        static MaterialComponent Deserialize(const YAML::Node& node) {
            MaterialComponent mc;
            mc.ShaderName  = node["ShaderName"].as<std::string>("Flat");
            mc.AlbedoColor = node["AlbedoColor"].as<glm::vec4>(glm::vec4(1.0f));
            mc.Roughness   = node["Roughness"].as<float>(0.5f);
            mc.Metallic    = node["Metallic"].as<float>(0.0f);
            return mc;
        }
    };
}
