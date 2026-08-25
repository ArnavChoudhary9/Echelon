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

        /** @brief Return a value copy of this component. */
        IDComponent Copy() const { return *this; }

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

        /** @brief Return a value copy of this component. */
        TransformComponent Copy() const { return *this; }

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

        TagComponent Copy() const { return *this; }

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
     * @brief References a Material instance asset (Unity-like shared material).
     *
     * The material reference (handle + readable source) is serialized; the resolved
     * material asset is transient and re-resolved at render time — mirroring
     * MeshComponent. This is a PURE asset reference: per-object variation is a *new
     * Material instance*, not an inline override (editing a shared Material affects
     * every object using it). An empty MaterialHandle falls back to the renderer's
     * default pipeline.
     *
     * This component holds only DATA. GPU objects (pipeline + descriptor set) live in
     * the renderer's material cache, keyed by material/template identity; the engine
     * never builds them. A Version counter lets the renderer detect reference changes
     * cheaply (O(1)); value edits to the shared Material bump Material::Version.
     */
    class MaterialComponent {
    public:
        // ---- Asset reference (serialized) ----
        UUID        MaterialHandle = UUID::Null();  ///< Authoritative material instance asset reference.
        std::string MaterialSource;                 ///< Readable hint: relative path / built-in name.

        // ---- Transient (resolved by the renderer at render time) ----
        // GPU objects (pipeline + descriptor set) live in the renderer's material
        // cache, keyed by material/template identity — NOT here. The component carries
        // only data: the resolved material asset + cheap change-tracking.
        Ref<Material>         RuntimeMaterial;               ///< Resolved material instance (data).
        uint64_t              ResolveEpoch = UINT64_MAX;      ///< AssetManager epoch at last resolve.
        uint64_t              Version      = 0;               ///< Cheap dirty check (reference changes).

        MaterialComponent() = default;
        MaterialComponent(const MaterialComponent&) = default;
        MaterialComponent& operator=(const MaterialComponent&) = default;
        ~MaterialComponent() = default;

        /** Bump the version — call after changing which material is referenced. */
        void Invalidate() { ++Version; }

        // ---- Serialization ----
        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "MaterialComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "MaterialHandle" << YAML::Value << MaterialHandle.ToString();
            out << YAML::Key << "MaterialSource" << YAML::Value << MaterialSource;
            out << YAML::EndMap;
        }

        static MaterialComponent Deserialize(const YAML::Node& node) {
            MaterialComponent mc;
            std::string handleStr = node["MaterialHandle"] ? node["MaterialHandle"].as<std::string>("") : "";
            mc.MaterialHandle = handleStr.empty() ? UUID::Null() : UUID(handleStr);
            mc.MaterialSource = node["MaterialSource"].as<std::string>("");
            return mc;
        }
    };

    // ==================================================================
    // LightComponent
    // ==================================================================
    /** @brief Light source type. Directional ignores position; Point ignores facing. */
    enum class LightType : uint32_t { Directional = 0, Point = 1, Spot = 2 };

    /**
     * @brief A light source. Direction/position come from the entity's
     *        TransformComponent (Rotation → facing for directional/spot, Position
     *        for point/spot). The renderer gathers all lights each frame into the
     *        g_Lights system UBO (see Echelon.slang / RayRenderer::BeginScene).
     *
     * Spot cones are authored as half-angles in DEGREES (editor-friendly) and
     * converted to the cosines the shader expects via CosInner()/CosOuter().
     * A light marked CastsShadows is eligible for a shadow map; the renderer caps
     * shadow casters to one per light type (directional / spot / point).
     */
    class LightComponent {
    public:
        LightType Type         = LightType::Directional;
        glm::vec3 Color        = glm::vec3(1.0f);
        float     Intensity    = 1.0f;
        float     Range        = 10.0f;    // point / spot falloff distance (world units)
        float     InnerAngle   = 25.0f;    // spot: inner cone half-angle (degrees) — full brightness inside
        float     OuterAngle   = 35.0f;    // spot: outer cone half-angle (degrees) — falls to zero by here
        bool      CastsShadows = true;     // eligible to cast a shadow map (renderer picks one caster per type)
        float     ShadowBias   = 0.0015f;  // depth-compare bias to combat shadow acne
        bool      Enabled      = true;     // soft on/off without removing the component

        LightComponent() = default;
        LightComponent(const LightComponent&) = default;
        LightComponent& operator=(const LightComponent&) = default;
        ~LightComponent() = default;

        LightComponent Copy() const { return *this; }

        /** @brief cos(inner half-angle) as packed into GpuLight.SpotParams.x for the shader. */
        float CosInner() const { return glm::cos(glm::radians(InnerAngle)); }
        /** @brief cos(outer half-angle) as packed into GpuLight.SpotParams.y for the shader. */
        float CosOuter() const { return glm::cos(glm::radians(OuterAngle)); }

        void Serialize(YAML::Emitter& out) const {
            out << YAML::Key << "LightComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type"         << YAML::Value << static_cast<uint32_t>(Type);
            out << YAML::Key << "Color"        << YAML::Value << Color;
            out << YAML::Key << "Intensity"    << YAML::Value << Intensity;
            out << YAML::Key << "Range"        << YAML::Value << Range;
            out << YAML::Key << "InnerAngle"   << YAML::Value << InnerAngle;
            out << YAML::Key << "OuterAngle"   << YAML::Value << OuterAngle;
            out << YAML::Key << "CastsShadows" << YAML::Value << CastsShadows;
            out << YAML::Key << "ShadowBias"   << YAML::Value << ShadowBias;
            out << YAML::Key << "Enabled"      << YAML::Value << Enabled;
            out << YAML::EndMap;
        }

        static LightComponent Deserialize(const YAML::Node& node) {
            LightComponent c;
            c.Type      = static_cast<LightType>(node["Type"].as<uint32_t>(0u));
            c.Color     = node["Color"].as<glm::vec3>(glm::vec3(1.0f));
            c.Intensity = node["Intensity"].as<float>(1.0f);
            c.Range     = node["Range"].as<float>(10.0f);

            // Prefer the new degree-based cone; fall back to the legacy cosine keys.
            if (node["InnerAngle"]) c.InnerAngle = node["InnerAngle"].as<float>(25.0f);
            else if (node["InnerCone"]) c.InnerAngle = glm::degrees(glm::acos(glm::clamp(node["InnerCone"].as<float>(0.90f), -1.0f, 1.0f)));
            if (node["OuterAngle"]) c.OuterAngle = node["OuterAngle"].as<float>(35.0f);
            else if (node["OuterCone"]) c.OuterAngle = glm::degrees(glm::acos(glm::clamp(node["OuterCone"].as<float>(0.80f), -1.0f, 1.0f)));

            c.CastsShadows = node["CastsShadows"].as<bool>(true);
            c.ShadowBias   = node["ShadowBias"].as<float>(0.0015f);
            c.Enabled      = node["Enabled"].as<bool>(true);
            return c;
        }
    };
}
