#include "Application.hpp"
#include "Core/Clock.hpp"
#include "Instrumentation/Instrumentation.hpp"
#include "Core/Base.hpp"
#include "Platform/Window.hpp"
#include "Platform/Input.hpp"
#include "Project/Project.hpp"

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
    };

    Application::~Application() {
        m_Running = false;
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
        if (fs::exists(projectPath) && fs::is_directory(projectPath)) {
            for (const auto& entry : fs::directory_iterator(projectPath)) {
                if (entry.path().extension() == ".ehproj") {
                    ehprojFile = entry.path();
                    break;
                }
            }
        } else if (fs::exists(projectPath) && projectPath.extension() == ".ehproj") {
            // The user pointed directly at a .ehproj file
            ehprojFile = projectPath;
        }

        if (!ehprojFile.empty()) {
            m_Logger.Info("Loading project from: {}", ehprojFile.string());
            m_Project = Project::Load(ehprojFile);
            if (m_Project) {
                m_Logger.Info("Project '{}' loaded successfully.", m_Project->GetConfig().Name);
            } else {
                m_Logger.Error("Failed to load project from: {}", ehprojFile.string());
            }
        } else {
            // Create a new project
            std::string projectName = projectPath.filename().string();
            if (projectName.empty()) projectName = "DefaultProject";

            m_Logger.Info("Creating new project '{}' at: {}", projectName, projectPath.string());
            m_Project = Project::Create(projectPath, projectName);
            m_Logger.Info("Project '{}' created.", m_Project->GetConfig().Name);
        }
    }
}
