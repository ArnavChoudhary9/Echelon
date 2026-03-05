#include "SceneSerializer.hpp"
#include "Scene.hpp"
#include "ECS/Components.hpp"
#include "Core/Log.hpp"

#include "yaml-cpp/yaml.h"

#include <fstream>

namespace Echelon {

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
        : m_Scene(scene) {}

    // ------------------------------------------------------------------
    // Serialize
    // ------------------------------------------------------------------
    void SceneSerializer::SerializeEntity(Entity entity, YAML::Emitter& out) const {
        out << YAML::BeginMap; // Entity

        // IDComponent (required)
        if (entity.HasComponent<IDComponent>()) {
            entity.GetComponent<IDComponent>().Serialize(out);
        }

        // TagComponent
        if (entity.HasComponent<TagComponent>()) {
            entity.GetComponent<TagComponent>().Serialize(out);
        }

        // TransformComponent
        if (entity.HasComponent<TransformComponent>()) {
            entity.GetComponent<TransformComponent>().Serialize(out);
        }

        // RelationshipComponent
        if (entity.HasComponent<RelationshipComponent>()) {
            entity.GetComponent<RelationshipComponent>().Serialize(out);
        }

        // MeshComponent
        if (entity.HasComponent<MeshComponent>()) {
            entity.GetComponent<MeshComponent>().Serialize(out);
        }

        // CameraComponent
        if (entity.HasComponent<CameraComponent>()) {
            entity.GetComponent<CameraComponent>().Serialize(out);
        }

        // MaterialComponent
        if (entity.HasComponent<MaterialComponent>()) {
            entity.GetComponent<MaterialComponent>().Serialize(out);
        }

        out << YAML::EndMap; // Entity
    }

    bool SceneSerializer::Serialize(const std::filesystem::path& filepath) const {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene"    << YAML::Value << m_Scene->GetName();
        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        auto registry = m_Scene->GetEntityRegistry().lock();
        if (registry) {
            auto view = registry->view<IDComponent>();
            for (auto&& [entityID, id] : view.each()) {
                Entity entity(entityID, m_Scene);
                if (!entity)
                    continue;
                SerializeEntity(entity, out);
            }
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(filepath);
        if (!fout.is_open()) {
            ECHELON_LOG_ERROR("[SceneSerializer] Could not write to {}", filepath.string());
            return false;
        }

        fout << out.c_str();
        return true;
    }

    // ------------------------------------------------------------------
    // Deserialize
    // ------------------------------------------------------------------
    bool SceneSerializer::Deserialize(const std::filesystem::path& filepath) {
        if (!std::filesystem::exists(filepath)) {
            ECHELON_LOG_ERROR("[SceneSerializer] File not found: {}", filepath.string());
            return false;
        }

        YAML::Node data;
        try {
            data = YAML::LoadFile(filepath.string());
        }
        catch (const YAML::Exception& e) {
            ECHELON_LOG_ERROR("[SceneSerializer] YAML error: {}", e.what());
            return false;
        }

        auto sceneName = data["Scene"].as<std::string>("Untitled Scene");
        m_Scene->SetName(sceneName);

        auto entities = data["Entities"];
        if (!entities)
            return true; // Valid but empty scene

        // Clear existing entities
        m_Scene->Clear();

        for (const auto& entityNode : entities) {
            // Recover UUID
            uint64_t uuid = 0;
            if (entityNode["IDComponent"])
                uuid = entityNode["IDComponent"]["ID"].as<uint64_t>();

            // Recover tag / name
            std::string name = "Entity";
            if (entityNode["TagComponent"])
                name = entityNode["TagComponent"]["Tag"].as<std::string>("Entity");

            // Create entity with specific UUID
            Entity entity = m_Scene->AddEntityWithUUID(UUID(uuid), name);

            // TransformComponent
            if (entityNode["TransformComponent"]) {
                auto& tc = entity.GetComponent<TransformComponent>();
                auto deserialized = TransformComponent::Deserialize(entityNode["TransformComponent"]);
                tc.Position = deserialized.Position;
                tc.Rotation = deserialized.Rotation;
                tc.Scale    = deserialized.Scale;
            }

            // RelationshipComponent
            if (entityNode["RelationshipComponent"]) {
                auto rc = RelationshipComponent::Deserialize(entityNode["RelationshipComponent"]);
                if (entity.HasComponent<RelationshipComponent>()) {
                    entity.GetComponent<RelationshipComponent>() = rc;
                } else {
                    entity.AddComponent<RelationshipComponent>(rc.Parent);
                    entity.GetComponent<RelationshipComponent>().Children = rc.Children;
                }
            }

            // MeshComponent (reconstructs metadata only — GPU buffers
            // must be re-created by the application / renderer)
            if (entityNode["MeshComponent"]) {
                auto mc = MeshComponent::Deserialize(entityNode["MeshComponent"]);
                entity.AddComponent<MeshComponent>(mc);
            }

            // CameraComponent
            if (entityNode["CameraComponent"]) {
                auto cc = CameraComponent::Deserialize(entityNode["CameraComponent"]);
                entity.AddComponent<CameraComponent>(cc);
            }

            // MaterialComponent (pipeline ref is transient — reassigned by renderer)
            if (entityNode["MaterialComponent"]) {
                auto mc = MaterialComponent::Deserialize(entityNode["MaterialComponent"]);
                entity.AddComponent<MaterialComponent>(mc);
            }
        }

        // Rebuild the scene graph after loading
        m_Scene->MarkSceneGraphDirty();

        return true;
    }
}
