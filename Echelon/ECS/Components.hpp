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
#include "Asset/Mesh/Mesh.hpp"
#include "Asset/Material/Material.hpp"
#include "Asset/Material/MaterialInstance.hpp"

#include "glm/glm.hpp"
#include "yaml-cpp/yaml.h"

#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace YAML {
    // ---- YAML helpers for glm::vec3 ----
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

    inline Emitter& operator<<(Emitter& out, const glm::vec3& v) {
        out << Flow;
        out << BeginSeq << v.x << v.y << v.z << EndSeq;
        return out;
    }

    // ---- YAML helpers for glm::vec4 ----
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

    inline Emitter& operator<<(Emitter& out, const glm::vec4& v) {
        out << Flow;
        out << BeginSeq << v.x << v.y << v.z << v.w << EndSeq;
        return out;
    }

    // ---- YAML helpers for UUID ----
    template<>
    struct convert<Echelon::UUID> {
        static Node encode(const Echelon::UUID& uuid) {
            return Node(uuid.ToString());
        }

        static bool decode(const Node& node, Echelon::UUID& uuid) {
            if (!node.IsScalar())
                return false;
            uuid = Echelon::UUID(node.as<std::string>());
            return true;
        }
    };
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
            out << YAML::Key << "ID" << YAML::Value << ID;
            out << YAML::EndMap;
        }

        static IDComponent Deserialize(const YAML::Node& node) {
            return IDComponent(node["ID"].as<UUID>());
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
     * - Parent is nullopt for root-level entities, or the UUID of the parent.
     * - Children are stored as a vector of UUIDs.
     * - The scene graph queries these to build a hierarchy; it only
     *   rebuilds when the dirty flag is set.
     */
    class RelationshipComponent {
    public:
        std::optional<UUID> Parent;         // nullopt = root-level entity
        std::vector<UUID>   Children;       // UUIDs of child entities

        RelationshipComponent() = default;
        RelationshipComponent(std::optional<UUID> parent) : Parent(parent) {}
        RelationshipComponent(const RelationshipComponent&) = default;
        RelationshipComponent& operator=(const RelationshipComponent&) = default;
        ~RelationshipComponent() = default;

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "RelationshipComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Parent" << YAML::Value;
            if (Parent.has_value())
                out << *Parent;
            else
                out << YAML::Null;
            out << YAML::Key << "Children" << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (const auto& child : Children)
                out << child;
            out << YAML::EndSeq;
            out << YAML::EndMap;
        }

        static RelationshipComponent Deserialize(const YAML::Node& node) {
            RelationshipComponent rc;
            const auto& parentNode = node["Parent"];
            if (parentNode && !parentNode.IsNull())
                rc.Parent = parentNode.as<UUID>();
            if (node["Children"]) {
                for (const auto& child : node["Children"])
                    rc.Children.push_back(child.as<UUID>());
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
        UUID        MeshHandle  = UUID::Null(); ///< Authoritative asset reference (serialized).
        std::string MeshSource  = "";           ///< Readable hint: built-in name or relative path (serialized).
        Ref<Mesh>   RuntimeMesh = nullptr;       ///< Resolved runtime asset (transient; set by the AssetManager).

        /**
         * Transient: the AssetManager epoch at the last resolution attempt.
         * Guards against re-resolving (and re-logging) an unresolved reference
         * every frame, while still retrying after a hot-reload / renderer swap
         * (both bump the epoch).  UINT64_MAX means "never attempted".
         */
        uint64_t     ResolveEpoch  = UINT64_MAX;

        /** Bumped whenever the resolved mesh changes.  Cheap dirty check. */
        uint64_t     Version       = 0;

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
        MeshComponent& operator=(const MeshComponent&) = default;
        ~MeshComponent() = default;

        /** Convenience: is the mesh resolved and ready to render? */
        bool IsValid() const { return RuntimeMesh && RuntimeMesh->IsValid(); }

        /** Bump the version — call after the resolved mesh changes. */
        void Invalidate() { ++Version; }

        // ---- Serialization (asset reference only — GPU data is transient) ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "MeshComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "MeshHandle" << YAML::Value << MeshHandle.ToString();
            out << YAML::Key << "MeshSource" << YAML::Value << MeshSource;
            out << YAML::EndMap;
        }

        static MeshComponent Deserialize(const YAML::Node& node) {
            MeshComponent mc;
            // Prefer the stable handle; fall back to the source hint (legacy scenes).
            // Resolution to a Ref<Mesh> is deferred to render time (needs the
            // AssetManager + active renderer), so no AssetManager dependency here.
            std::string handleStr = node["MeshHandle"] ? node["MeshHandle"].as<std::string>("") : "";
            mc.MeshHandle = handleStr.empty() ? UUID::Null() : UUID(handleStr);
            mc.MeshSource = node["MeshSource"].as<std::string>("");
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
     * @brief References a Material asset (+ optional sparse instance overrides).
     *
     * The material reference (handle + readable source) is serialized; the
     * resolved GPU objects (base material, instance, pipeline) are transient and
     * rebuilt at render time — mirroring MeshComponent. Overrides are sparse
     * per-entity parameter values layered over the base material (they form a
     * runtime MaterialInstance). An empty MaterialHandle falls back to the
     * renderer's default pipeline.
     *
     * A Version counter lets the RenderGraph detect changes cheaply (O(1)).
     */
    class MaterialComponent {
    public:
        // ---- Asset reference (serialized) ----
        UUID        MaterialHandle = UUID::Null();  ///< Authoritative material asset reference.
        std::string MaterialSource;                 ///< Readable hint: relative path.

        // ---- Sparse per-entity overrides (serialized) → runtime MaterialInstance ----
        std::unordered_map<std::string, MaterialParam> Overrides;

        // ---- Transient (resolved by the RenderGraph) ----
        Ref<Material>         RuntimeMaterial;               ///< Resolved base material.
        Ref<MaterialInstance> RuntimeInstance;               ///< Built when Overrides is non-empty.
        Ref<Pipeline>         PipelineRef = nullptr;         ///< Resolved pipeline (base material's).
        uint64_t              ResolveEpoch = UINT64_MAX;      ///< AssetManager epoch at last resolve.
        uint64_t              Version      = 0;               ///< Cheap dirty check.

        MaterialComponent() = default;
        MaterialComponent(const MaterialComponent&) = default;
        MaterialComponent& operator=(const MaterialComponent&) = default;
        ~MaterialComponent() = default;

        /** Bump the version — call after changing the material or overrides. */
        void Invalidate() { ++Version; }

        /** Sort key: pointer identity of the resolved pipeline, for batching. */
        uintptr_t GetPipelineSortKey() const {
            return reinterpret_cast<uintptr_t>(PipelineRef.get());
        }

        /** The descriptor set to bind for this entity (instance overrides → base → none). */
        Ref<DescriptorSet> GetDescriptorSet() const {
            if (RuntimeInstance) return RuntimeInstance->GetDescriptorSet();
            return RuntimeMaterial ? RuntimeMaterial->GetDescriptorSet() : nullptr;
        }

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "MaterialComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "MaterialHandle" << YAML::Value << MaterialHandle.ToString();
            out << YAML::Key << "MaterialSource" << YAML::Value << MaterialSource;
            if (!Overrides.empty()) {
                out << YAML::Key << "Overrides" << YAML::Value << YAML::BeginMap;
                for (const auto& [name, p] : Overrides) {
                    const uint32_t floats = p.ByteSize() / 4;
                    out << YAML::Key << name << YAML::Value << YAML::BeginMap;
                    out << YAML::Key << "Type"  << YAML::Value << MaterialParamTypeToString(p.Type);
                    out << YAML::Key << "Value" << YAML::Value << YAML::Flow << YAML::BeginSeq;
                    for (uint32_t i = 0; i < floats; ++i) out << p.Data[i];
                    out << YAML::EndSeq << YAML::EndMap;
                }
                out << YAML::EndMap;
            }
            out << YAML::EndMap;
        }

        static MaterialComponent Deserialize(const YAML::Node& node) {
            MaterialComponent mc;
            std::string handleStr = node["MaterialHandle"] ? node["MaterialHandle"].as<std::string>("") : "";
            mc.MaterialHandle = handleStr.empty() ? UUID::Null() : UUID(handleStr);
            mc.MaterialSource = node["MaterialSource"].as<std::string>("");
            if (const YAML::Node ov = node["Overrides"]) {
                for (const auto& kv : ov) {
                    MaterialParam p;
                    p.Type = MaterialParamTypeFromString(kv.second["Type"].as<std::string>("Float4"));
                    const uint32_t floats = p.ByteSize() / 4;
                    const YAML::Node vals = kv.second["Value"];
                    if (vals && vals.IsSequence())
                        for (uint32_t i = 0; i < floats && i < vals.size(); ++i)
                            p.Data[i] = vals[i].as<float>(0.0f);
                    mc.Overrides[kv.first.as<std::string>()] = p;
                }
            }
            return mc;
        }
    };
}
