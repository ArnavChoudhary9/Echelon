#include "Application.hpp"
#include "Core/Clock.hpp"
#include "Instrumentation/Instrumentation.hpp"
#include "Core/Base.hpp"
#include "Platform/Window.hpp"
#include "Platform/Input.hpp"
#include "Project/Project.hpp"
#include "Renderer/RendererService.hpp"

#include <filesystem>

namespace Echelon {
    Application* Application::s_Instance = nullptr;

    Application::Application(ApplicationConfig& config) 
        : m_Config(config), m_LayerStack(), m_Logger(config.Name)
    {
        s_Instance = this;
        m_Logger.AddSink(ConsoleSink);
        m_Logger.AddSink(FileSink(config.Name + ".log"));

        m_Logger.Info(
            "Application '{}' initialized with size {}x{}",
            m_Config.Name,
            m_Config.WindowDimensions.Width,
            m_Config.WindowDimensions.Height
        );

        // ---- Initialize Project ----
        InitializeProject();

        // ---- Create platform Window ----
        WindowDesc winDesc = config.WindowDescription;
        winDesc.Title  = config.Name;
        winDesc.Width  = config.WindowDimensions.Width;
        winDesc.Height = config.WindowDimensions.Height;
        m_Window = Window::Create(winDesc);

        if (m_Window)
        {
            m_Window->SetEventCallback(EH_BIND_EVENT_FN(OnEvent));
            m_Logger.Info("Platform window created ({}x{})", winDesc.Width, winDesc.Height);
        }

        // ---- Create platform Input ----
        m_Input = Input::Create(winDesc.Backend);

        // ---- Load + initialise the default renderer (engine-owned) ----
        // Done here (after the window exists) so it is ready before any layer's
        // OnAttach runs. The default plugin name is renderer-agnostic and comes
        // from the application config (build-time ECHELON_DEFAULT_RENDERER).
        if (m_Window)
        {
            if (Renderer::Get().Init(*m_Window, m_Config.DefaultRenderer))
                m_Logger.Info("Renderer '{}' initialized.", Renderer::Get().GetActiveName());
            else
                m_Logger.Error("Failed to initialize renderer '{}'.", m_Config.DefaultRenderer);
        }
    };

    Application::~Application() {
        m_Running = false;

        // Tear down while the window / GL context is still alive. Order mirrors
        // setup in reverse:
        //   1. Detach layers — they release the GPU resources created in OnAttach.
        //   2. Release the active project and its scene. Scene mesh/material
        //      components own GPU handles (vertex buffers, pipelines), and the
        //      scene is co-owned by the static Project::s_ActiveProject. Without
        //      an explicit release here the scene would be destroyed at program
        //      exit — after the window (and its GL context) is gone — calling
        //      glDelete* on a dead context and crashing. Dropping both owning refs
        //      now frees those handles while the context is still current.
        //   3. Shut the renderer down (releases its own GPU resources).
        //
        // m_Window is declared after m_LayerStack, so relying on member destruction
        // order alone would run all of this on a dead GL context.
        m_LayerStack.Clear();

        Project::SetActive(nullptr);
        m_Project = nullptr;

        Renderer::Get().Shutdown();
    };

    void Application::Run() {
        ECHELON_PROFILE_FUNCTION();
        m_Logger.Trace("Application is running...");

        while (m_Running) {
            ECHELON_PROFILE_SCOPE("Update Loop");
            double frameStart = m_Window ? m_Window->GetTime() : 0.0;
            
            // --- Poll platform events ---
            if (m_Window)
                m_Window->PollEvents();

            // --- Update layers ---
            for (auto& layer : m_LayerStack) {
                layer->OnUpdate(m_FrameDuration);
            }

            // --- Present ---
            if (m_Window)
                m_Window->SwapBuffers();

            // --- Check for close request ---
            if (m_Window && m_Window->ShouldClose())
                m_Running = false;

            double frameEnd = m_Window ? m_Window->GetTime() : 0.0;
            m_FrameDuration = static_cast<float>(frameEnd - frameStart);
        }
    };

    void Application::OnEvent(Event& event) {
        m_Logger.Trace("Event received: {}", event.ToString());

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(EH_BIND_EVENT_FN(OnWindowClose));

        m_LayerStack.OnEvent(event);
    };
    
    void Application::OnEvent(Event&& event) {
        OnEvent(event);
    }

    void Application::PushLayer(Ref<Layer> layer) {
        m_LayerStack.PushLayer(layer);
    }

    void Application::PushOverlay(Ref<Overlay> overlay) {
        m_LayerStack.PushOverlay(overlay);
    }

    void Application::PopLayer(Ref<Layer> layer) {
        m_LayerStack.PopLayer(layer);
    }

    void Application::PopOverlay(Ref<Overlay> overlay) {
        m_LayerStack.PopOverlay(overlay);
    }

    void Application::InitializeProject() {
        namespace fs = std::filesystem;
        std::error_code ec;

        fs::path projectPath;

        // Check if a project path was supplied via command-line args
        // argv[0] = executable, argv[1] = project path (if present)
        if (m_Config.Args.GetArgCount() > 1 && m_Config.Args.GetArg(1)) {
            projectPath = m_Config.Args.GetArg(1);
        } else {
            projectPath = fs::path("./DefaultProject");
        }

        // Determine if this is an existing project or a new one
        // Look for a .ehproj file in the directory
        fs::path ehprojFile;
        if (fs::is_directory(projectPath, ec)) {
            for (const auto& entry : fs::directory_iterator(projectPath, ec)) {
                if (entry.path().extension() == ".ehproj") {
                    ehprojFile = entry.path();
                    break;
                }
            }
        } else if (fs::exists(projectPath, ec) && projectPath.extension() == ".ehproj") {
            // The user pointed directly at a .ehproj file
            ehprojFile = projectPath;
        }

        // Try to load an existing project first.
        if (!ehprojFile.empty()) {
            m_Logger.Info("Loading project from: {}", ehprojFile.string());
            m_Project = Project::Load(ehprojFile);
            if (m_Project)
                m_Logger.Info("Project '{}' loaded successfully.", m_Project->GetConfig().Name);
            else
                m_Logger.Error("Failed to load project from: {}; recreating a default project.",
                               ehprojFile.string());
        }

        // Recovery: if there was no project to load, or loading failed (missing /
        // corrupt), (re)create one at the requested location.
        if (!m_Project) {
            fs::path projectDir = (projectPath.extension() == ".ehproj")
                                      ? projectPath.parent_path()
                                      : projectPath;

            std::string projectName = projectDir.filename().string();
            if (projectName.empty()) projectName = "DefaultProject";

            m_Logger.Info("Creating new project '{}' at: {}", projectName, projectDir.string());
            m_Project = Project::Create(projectDir, projectName);
            m_Logger.Info("Project '{}' created.", m_Project->GetConfig().Name);
        }

        // Whether loaded or created, make sure a usable current scene exists.
        EnsureProjectHasScene();
    }

    void Application::EnsureProjectHasScene() {
        if (!m_Project || m_Project->GetCurrentScene())
            return; // Nothing to do — Create() already adopts a fresh scene.

        // Prefer the configured start scene.
        const std::string& startScene = m_Project->GetConfig().StartScene;
        if (!startScene.empty() && m_Project->OpenScene(startScene))
            return;

        // Either no start scene is configured, or the file it points to is missing
        // or unreadable (deleted / corrupt). Recreate an empty scene so the project
        // stays usable, keeping the configured path where possible.
        std::string relativePath = startScene.empty() ? std::string("Default.ehscene")
                                                       : startScene;

        // SaveSceneAs resolves relative to the Scenes directory; strip a legacy
        // "Scenes/" prefix so the recreated file lands in the right place.
        if (relativePath.rfind("Scenes/", 0) == 0)
            relativePath = relativePath.substr(7);

        if (startScene.empty())
            m_Logger.Info("Project has no start scene; creating a default one.");
        else
            m_Logger.Warn("Start scene '{}' is missing or unreadable; recreating it.",
                          startScene);

        m_Project->NewScene("Default Scene");
        if (m_Project->SaveSceneAs(relativePath)) {
            m_Project->GetConfig().StartScene = relativePath;
            m_Project->Save();
        }
    }
}
