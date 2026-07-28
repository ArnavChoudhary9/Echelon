#include "Project.hpp"

#include "Core/Log.hpp"
#include "Scene/Scene.hpp"
#include "Scene/SceneSerializer.hpp"

#include "yaml-cpp/yaml.h"

#include <fstream>

namespace Echelon {

    // ------------------------------------------------------------------
    // Static members
    // ------------------------------------------------------------------
    Ref<Project> Project::s_ActiveProject = nullptr;

    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------
    void Project::DeriveSubPaths() {
        m_Config.ScenesDirectory    = m_Config.RootDirectory / "Scenes";
        m_Config.ResourcesDirectory = m_Config.RootDirectory / "Resources";
        m_Config.BuildsDirectory    = m_Config.RootDirectory / "Builds";
    }

    void Project::EnsureDirectories() const {
        std::filesystem::create_directories(m_Config.RootDirectory);
        std::filesystem::create_directories(m_Config.ScenesDirectory);
        std::filesystem::create_directories(m_Config.ResourcesDirectory);
        std::filesystem::create_directories(m_Config.BuildsDirectory);
    }

    std::filesystem::path Project::GetProjectFilePath() const {
        return m_Config.RootDirectory / (m_Config.Name + ".ehproj");
    }

    // ------------------------------------------------------------------
    // Create
    // ------------------------------------------------------------------
    Ref<Project> Project::Create(const std::filesystem::path& projectDir, const std::string& name) {
        auto project = CreateRef<Project>();

        project->m_Config.Name          = name;
        project->m_Config.RootDirectory = std::filesystem::absolute(projectDir);
        project->DeriveSubPaths();
        project->EnsureDirectories();

        // Create a default empty scene if one does not already exist, and adopt
        // it as the current scene so a freshly-created project is immediately
        // usable (and SaveScene() has a valid path to write to).
        auto defaultSceneRelative = std::string("Default.ehscene");
        auto defaultScenePath     = project->m_Config.ScenesDirectory / defaultSceneRelative;
        if (!std::filesystem::exists(defaultScenePath)) {
            auto scene = CreateRef<Scene>("Default Scene");
            SceneSerializer serializer(scene);
            serializer.Serialize(defaultScenePath);

            project->m_CurrentScene     = scene;
            project->m_CurrentScenePath = defaultScenePath;
            ECHELON_LOG_INFO("[Project] Created default scene: {}", defaultScenePath.string());
        }

        // StartScene is stored relative to the Scenes directory (see OpenScene),
        // so it must not carry a "Scenes/" prefix — that would double the segment
        // when resolved and leave the start scene unloadable.
        project->m_Config.StartScene = defaultSceneRelative;

        project->Save();

        s_ActiveProject = project;
        return project;
    }

    // ------------------------------------------------------------------
    // Load
    // ------------------------------------------------------------------
    Ref<Project> Project::Load(const std::filesystem::path& projectFilePath) {
        if (!std::filesystem::exists(projectFilePath)) {
            ECHELON_LOG_ERROR("[Project] File not found: {}", projectFilePath.string());
            return nullptr;
        }

        YAML::Node data;
        try {
            data = YAML::LoadFile(projectFilePath.string());
        }
        catch (const YAML::Exception& e) {
            ECHELON_LOG_ERROR("[Project] YAML parse error: {}", e.what());
            return nullptr;
        }

        auto projectNode = data["Project"];
        if (!projectNode) {
            ECHELON_LOG_ERROR("[Project] Missing 'Project' root key in {}", projectFilePath.string());
            return nullptr;
        }

        auto project = CreateRef<Project>();
        project->m_Config.Name          = projectNode["Name"].as<std::string>("Untitled");
        project->m_Config.RootDirectory = projectFilePath.parent_path();
        project->DeriveSubPaths();

        if (projectNode["StartScene"])
            project->m_Config.StartScene = projectNode["StartScene"].as<std::string>("");

        // Ensure directories exist even when loading (defensive)
        project->EnsureDirectories();

        s_ActiveProject = project;
        return project;
    }

    // ------------------------------------------------------------------
    // Save
    // ------------------------------------------------------------------
    bool Project::Save() const {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Project" << YAML::Value << YAML::BeginMap;

        out << YAML::Key << "Name"       << YAML::Value << m_Config.Name;
        out << YAML::Key << "StartScene" << YAML::Value << m_Config.StartScene;

        out << YAML::EndMap; // Project
        out << YAML::EndMap; // root

        std::ofstream fout(GetProjectFilePath());
        if (!fout.is_open()) {
            ECHELON_LOG_ERROR("[Project] Could not write to {}", GetProjectFilePath().string());
            return false;
        }

        fout << out.c_str();
        return true;
    }

    // ------------------------------------------------------------------
    // Scene Management
    // ------------------------------------------------------------------
    Ref<Scene> Project::NewScene(const std::string& name) {
        m_CurrentScene = CreateRef<Scene>(name);
        m_CurrentScenePath.clear(); // No path yet until SaveSceneAs is called
        ECHELON_LOG_INFO("[Project] Created new scene: {}", name);
        return m_CurrentScene;
    }

    Ref<Scene> Project::OpenScene(const std::filesystem::path& path) {
        std::filesystem::path fullPath;

        if (path.is_absolute()) {
            fullPath = path;
        } else {
            // Relative paths resolve against the Scenes directory first. Fall back
            // to the project root so legacy "Scenes/<name>" values (which would
            // otherwise double the Scenes segment) still resolve. If neither
            // exists, keep the canonical Scenes path for a clear error message.
            fullPath = m_Config.ScenesDirectory / path;
            if (!std::filesystem::exists(fullPath)) {
                std::filesystem::path rootRelative = m_Config.RootDirectory / path;
                if (std::filesystem::exists(rootRelative))
                    fullPath = rootRelative;
            }
        }

        if (!std::filesystem::exists(fullPath)) {
            ECHELON_LOG_ERROR("[Project] Scene file not found: {}", fullPath.string());
            return nullptr;
        }

        auto scene = CreateRef<Scene>();
        SceneSerializer serializer(scene);
        
        if (!serializer.Deserialize(fullPath)) {
            ECHELON_LOG_ERROR("[Project] Failed to deserialize scene: {}", fullPath.string());
            return nullptr;
        }

        m_CurrentScene = scene;
        m_CurrentScenePath = fullPath;
        ECHELON_LOG_INFO("[Project] Opened scene: {}", fullPath.string());
        return m_CurrentScene;
    }

    bool Project::SaveScene() {
        if (!m_CurrentScene) {
            ECHELON_LOG_ERROR("[Project] No active scene to save.");
            return false;
        }

        if (m_CurrentScenePath.empty()) {
            ECHELON_LOG_ERROR("[Project] Current scene has no path. Use SaveSceneAs() instead.");
            return false;
        }

        // The scene directory may have been removed while the editor was running
        // (e.g. the folder was deleted); recreate it so serialization succeeds.
        std::error_code ec;
        std::filesystem::create_directories(m_CurrentScenePath.parent_path(), ec);

        SceneSerializer serializer(m_CurrentScene);
        if (!serializer.Serialize(m_CurrentScenePath)) {
            ECHELON_LOG_ERROR("[Project] Failed to save scene to: {}", m_CurrentScenePath.string());
            return false;
        }

        ECHELON_LOG_INFO("[Project] Scene saved: {}", m_CurrentScenePath.string());
        return true;
    }

    bool Project::SaveSceneAs(const std::filesystem::path& relativePath) {
        if (!m_CurrentScene) {
            ECHELON_LOG_ERROR("[Project] No active scene to save.");
            return false;
        }

        std::filesystem::path fullPath = m_Config.ScenesDirectory / relativePath;

        // Ensure the directory exists
        std::filesystem::create_directories(fullPath.parent_path());

        SceneSerializer serializer(m_CurrentScene);
        if (!serializer.Serialize(fullPath)) {
            ECHELON_LOG_ERROR("[Project] Failed to save scene to: {}", fullPath.string());
            return false;
        }

        m_CurrentScenePath = fullPath;
        ECHELON_LOG_INFO("[Project] Scene saved as: {}", fullPath.string());
        return true;
    }
}
