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
 *  - All filesystem paths are stored as fs::path for portability.
 *  - Project files use the .ehproj extension and are serialized with YAML.
 *  - Sub-directories are lazily created on first save / explicit Init().
 *  - The project path can be supplied via command-line args; if missing,
 *    the engine defaults to "./DefaultProject/".
 */

#include "Core/Base.hpp"

#include <string>
#include <filesystem>

namespace Echelon {
    class Scene;

    /**
     * @brief Holds all paths and metadata for an Echelon project.
     */
    struct ProjectConfig {
        std::string Name        = "Untitled";
        fs::path RootDirectory;                        // The project working directory
        fs::path ScenesDirectory;                     // <Root>/Scenes/
        fs::path AssetsDirectory;                      // <Root>/Assets/
        fs::path ResourcesDirectory;                  // <Root>/Resources/
        fs::path BuildsDirectory;                      // <Root>/Builds/
        fs::path StartScene;                           // Relative path to the start scene
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
        static Ref<Project> Create(const fs::path& projectDir, const std::string& name = "Untitled");

        /**
         * @brief Load an existing project from a .ehproj file.
         * 
         * @param projectFilePath Full path to the .ehproj file.
         * @return Ref<Project> The loaded project, or nullptr on failure.
         */
        static Ref<Project> Load(const fs::path& projectFilePath);

        /**
         * @brief Save the current project state to its .ehproj file.
         * @return true on success, false on failure.
         */
        bool Save() const;

        // ---- Accessors ----

        const ProjectConfig& GetConfig() const { return m_Config; }
        ProjectConfig& GetConfig() { return m_Config; }

        const fs::path& GetRootDirectory()      const { return m_Config.RootDirectory; }
        const fs::path& GetScenesDirectory()    const { return m_Config.ScenesDirectory; }
        const fs::path& GetAssetsDirectory()    const { return m_Config.AssetsDirectory; }
        const fs::path& GetResourcesDirectory() const { return m_Config.ResourcesDirectory; }
        const fs::path& GetBuildsDirectory()    const { return m_Config.BuildsDirectory; }

        /**
         * @brief Get the full path to the .ehproj file.
         */
        fs::path GetProjectFilePath() const;

        // ---- Scene Management ----

        /**
         * @brief Create a new scene with the given name.
         *        The scene is set as the current scene but not saved to disk until SaveScene() is called.
         * 
         * @param name The name for the new scene.
         * @return Ref<Scene> The newly created scene.
         */
        Ref<Scene> NewScene(const std::string& name = "Untitled Scene");

        /**
         * @brief Open an existing scene from a relative or absolute path.
         *        If the path is relative, it's resolved from the project's Scenes directory.
         *        The loaded scene becomes the current scene.
         * 
         * @param path Path to the scene file (relative to Scenes/ or absolute).
         * @return Ref<Scene> The loaded scene, or nullptr on failure.
         */
        Ref<Scene> OpenScene(const fs::path& path);

        /**
         * @brief Save the current scene to its current path.
         *        If the scene hasn't been saved before, this will fail.
         *        Use SaveSceneAs() for new scenes.
         * 
         * @return true on success, false on failure.
         */
        bool SaveScene();

        /**
         * @brief Save the current scene to a new path (relative to Scenes directory).
         *        Updates the current scene path.
         * 
         * @param relativePath Path relative to the Scenes directory (e.g., "MyScene.ehscene").
         * @return true on success, false on failure.
         */
        bool SaveSceneAs(const fs::path& relativePath);

        /**
         * @brief Get the currently active scene.
         * @return Ref<Scene> The current scene, or nullptr if none is active.
         */
        Ref<Scene> GetCurrentScene() const { return m_CurrentScene; }

        /**
         * @brief Get the path to the current scene file.
         * @return const fs::path& The current scene's file path.
         */
        const fs::path& GetCurrentScenePath() const { return m_CurrentScenePath; }

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
        Ref<Scene> m_CurrentScene;
        fs::path m_CurrentScenePath;

        static Ref<Project> s_ActiveProject;
    };
}
