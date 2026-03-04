#pragma once

#include "Echelon/Core/Base.hpp"
#include "Echelon/Layer/LayerStack.hpp"
#include "Echelon/Logger/Logger.hpp"
#include "Echelon/Events/Event.hpp"
#include "Echelon/Events/ApplicationEvent.hpp"
#include "Echelon/Platform/Window.hpp"
#include "Echelon/Platform/Input.hpp"

#include <cstdint>

namespace Echelon {
    class ApplicationCommandLineArgs {
    public:
        ApplicationCommandLineArgs(int argc, char** argv) : m_Argc(argc), m_Argv(argv) {}

        /**
         * @brief Get the argument count.
         * 
         * @return int 
         */
        int GetArgCount() const { return m_Argc; }

        /**
         * @brief Get the argument at the specified index.
         * 
         * @param index The index of the argument to retrieve.
         * @return const char* The argument at the specified index, or nullptr if the index is out of bounds.
         */
        const char* GetArg(int index) const {
            if (index < m_Argc) {
                return m_Argv[index];
            }
            return nullptr;
        }
    
    private:
        int m_Argc;
        char** m_Argv;
    };

    class ApplicationConfig {
    public:
        ApplicationConfig() = default;
        ~ApplicationConfig() = default;

        std::string Name = "Echelon Application";
        ApplicationCommandLineArgs Args = ApplicationCommandLineArgs(0, nullptr);
        Dimension WindowDimensions = { 1280, 720 };

        /** Platform window creation parameters. */
        WindowDesc WindowDescription;
    };

    /**
     * @brief Represents the core application class. Extend this class to create a custom application.
     * 
     */
    class Application {
    public:
        Application() = delete;
        /**
         * @brief Construct a new Application object
         * 
         * @param int argc Argument count from the command line.
         * @param char** argv Argument vector from the command line.
         */
        Application(ApplicationConfig&);
        ~Application();

        /**
         * @brief Get the singleton Application instance.
         * @return Application& Reference to the running application.
         */
        static Application& Get() { return *s_Instance; }

        void Run();

        /**
         * @brief Handles events dispatched to the application.
         * 
         * @param Event& event The event to handle.
         */
        void OnEvent(Event&);
        void OnEvent(Event&&);

        // Layer management; Forwarded to LayerStack
        void PushLayer(Ref<Layer> layer);
        void PushOverlay(Ref<Overlay> overlay);
        void PopLayer(Ref<Layer> layer);
        void PopOverlay(Ref<Overlay> overlay);

        /**
         * @brief Get the application's Window.
         * @return Window& Reference to the platform window.
         */
        Window& GetWindow() { return *m_Window; }
        const Window& GetWindow() const { return *m_Window; }

        // Event Handlers
        bool OnWindowClose(WindowCloseEvent&) { m_Running = false; return true; };

    protected:
        bool m_Running = true;

        ApplicationConfig m_Config;
        LayerStack m_LayerStack;
        Logger m_Logger;

        Scope<Window> m_Window;
        Scope<Input>  m_Input;

    private:
        static Application* s_Instance;
    };
}

/**
 * @brief Create a Application object
 * 
 * @param int argc
 * @param char** argv
 * @return Echelon::Scope<Application>
 */
extern Echelon::Scope<Echelon::Application> CreateApplication(Echelon::ApplicationCommandLineArgs&);
