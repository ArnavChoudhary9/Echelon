#include "Project.hpp"

#include "Core/Log.hpp"

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
}
