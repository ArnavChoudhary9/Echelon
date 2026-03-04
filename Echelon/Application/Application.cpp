#include "Application.hpp"
#include "Echelon/Instrumentation/Instrumentation.hpp"
#include "Echelon/Core/Base.hpp"
#include "Echelon/Platform/Window.hpp"
#include "Echelon/Platform/Input.hpp"

namespace Echelon {
    Application::Application(ApplicationConfig& config) 
        : m_Config(config), m_LayerStack(), m_Logger(config.Name)
    {
        m_Logger.AddSink(ConsoleSink);
        m_Logger.AddSink(FileSink(config.Name + ".log"));

        m_Logger.Info(
            "Application '{}' initialized with size {}x{}",
            m_Config.Name,
            m_Config.WindowDimensions.Width,
            m_Config.WindowDimensions.Height
        );

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

        for (Layer* layer : m_LayerStack) {
            layer->OnDetach();
            delete layer;
        }
    };

    void Application::Run() {
        ECHELON_PROFILE_FUNCTION();
        m_Logger.Trace("Application is running...");

        while (m_Running) {
            ECHELON_PROFILE_SCOPE("Update Loop");

            // --- Poll platform events ---
            if (m_Window)
                m_Window->PollEvents();

            // --- Update layers ---
            for (Layer* layer : m_LayerStack) {
                layer->OnUpdate(0.0f);
            }

            // --- Present ---
            if (m_Window)
                m_Window->SwapBuffers();

            // --- Check for close request ---
            if (m_Window && m_Window->ShouldClose())
                m_Running = false;
        }
    };

    void Application::OnEvent(Event& event) {
        m_Logger.Trace("Event received: {}", event.ToString());

        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(EH_BIND_EVENT_FN(OnWindowClose));

        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
            (*--it)->OnEvent(event);
            if (event.Handled) {
                break;
            }
        }
    };
    
    void Application::OnEvent(Event&& event) {
        OnEvent(event);
    }

    void Application::PushLayer(Layer* layer) {
        m_LayerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void Application::PushOverlay(Overlay* overlay) {
        m_LayerStack.PushOverlay(overlay);
        overlay->OnAttach();
    }

    void Application::PopLayer(Layer* layer) {
        m_LayerStack.PopLayer(layer);
        layer->OnDetach();
        delete layer;
    }

    void Application::PopOverlay(Overlay* overlay) {
        m_LayerStack.PopOverlay(overlay);
        overlay->OnDetach();
        delete overlay;
    }
}
