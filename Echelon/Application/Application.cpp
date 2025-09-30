#include "Application.h"
#include "../Core/Base.h"

namespace Echelon {
    Application::Application(ApplicationConfig& config) 
        : m_Config(config), m_LayerStack(), m_Logger(config.Name)
    {
        m_Logger.AddSink(ConsoleSink);
        m_Logger.AddSink(FileSink(config.Name + ".log"));

        m_Logger.Info("Application '{}' initialized with size {}x{}", m_Config.Name, m_Config.Width, m_Config.Height);
    };

    Application::~Application() {
        m_Running = false;

        for (Layer* layer : m_LayerStack) {
            layer->OnDetach();
            delete layer;
        }
    };

    void Application::Run() {
        m_Logger.Trace("Application is starting...");

        while (m_Running) {
            for (Layer* layer : m_LayerStack) {
                layer->OnUpdate(0.0f);
            }
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
