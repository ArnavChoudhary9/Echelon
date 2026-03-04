#pragma once

/**
 * @file Project.hpp
 * @brief Echelon Project System
 * 
 * Manages project-level state: working directory, sub-directory paths
 * (scenes, resources, builds), and project metadata.
 * 
 * Best Practices:
 *  - A single Project instance is held by the Application; access via Project::Get().
 *  - All filesystem paths are stored as std::filesystem::path for portability.
 *  - Project files use the .ehproj extension and are serialized with YAML.
 *  - Sub-directories are lazily created on first save / explicit Init().
 *  - The project path can be supplied via command-line args; if missing,
 *    the engine defaults to "./DefaultProject/".
 */

#include "Core/Base.hpp"

#include <string>
#include <filesystem>

namespace Echelon {

    /**
     * @brief Holds all paths and metadata for an Echelon project.
     */
    struct ProjectConfig {
        std::string Name        = "Untitled";
        std::filesystem::path RootDirectory;          // The project working directory
        std::filesystem::path ScenesDirectory;        // <Root>/Scenes/
        std::filesystem::path ResourcesDirectory;     // <Root>/Resources/
        std::filesystem::path BuildsDirectory;        // <Root>/Builds/
        std::string StartScene  = "";                 // Relative path to the start scene
    };

    /**
     * @brief Manages the lifecycle of an Echelon project (.ehproj).
     * 
     * Responsibilities:
     *  - Parse / create project files in YAML format.
     *  - Ensure required sub-directories exist.
     *  - Provide accessor for all canonical paths.
     */
    class Project {
    public:
        Project() = default;
        ~Project() = default;

        // ---- Static access (singleton-like, one active project at a time) ----

        /**
         * @brief Get the currently active project.
         * @return Ref<Project> Shared pointer to the active project (may be null).
         */
        static Ref<Project> GetActive() { return s_ActiveProject; }

        /**
         * @brief Set the active project.
         * @param project The project to make active.
         */
        static void SetActive(Ref<Project> project) { s_ActiveProject = project; }

        // ---- Lifecycle ----

        /**
         * @brief Create a brand-new project at the given directory.
         *        Creates sub-directories and writes a default .ehproj file.
         * 
         * @param projectDir Root directory for the project.
         * @param name       Human-readable project name.
         * @return Ref<Project> The newly created project.
         */
        static Ref<Project> Create(const std::filesystem::path& projectDir, const std::string& name = "Untitled");

        /**
         * @brief Load an existing project from a .ehproj file.
         * 
         * @param projectFilePath Full path to the .ehproj file.
         * @return Ref<Project> The loaded project, or nullptr on failure.
         */
        static Ref<Project> Load(const std::filesystem::path& projectFilePath);

        /**
         * @brief Save the current project state to its .ehproj file.
         * @return true on success, false on failure.
         */
        bool Save() const;

        // ---- Accessors ----

        const ProjectConfig& GetConfig() const { return m_Config; }
        ProjectConfig& GetConfig() { return m_Config; }

        const std::filesystem::path& GetRootDirectory()      const { return m_Config.RootDirectory; }
        const std::filesystem::path& GetScenesDirectory()    const { return m_Config.ScenesDirectory; }
        const std::filesystem::path& GetResourcesDirectory() const { return m_Config.ResourcesDirectory; }
        const std::filesystem::path& GetBuildsDirectory()    const { return m_Config.BuildsDirectory; }

        /**
         * @brief Get the full path to the .ehproj file.
         */
        std::filesystem::path GetProjectFilePath() const;

    private:
        /**
         * @brief Ensure all sub-directories (Scenes, Resources, Builds) exist on disk.
         */
        void EnsureDirectories() const;

        /**
         * @brief Populate derived paths (ScenesDirectory, etc.) from RootDirectory.
         */
        void DeriveSubPaths();

        ProjectConfig m_Config;

        static Ref<Project> s_ActiveProject;
    };
}
