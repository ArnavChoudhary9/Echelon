#pragma once

#include "../Layer/LayerStack.h"
#include "../Logger/Logger.h"
#include "../Events/Event.h"
#include "../Events/ApplicationEvent.h"

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
        uint16_t Width = 1280;
        uint16_t Height = 720;
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

        void Run();

        /**
         * @brief Handles events dispatched to the application.
         * 
         * @param Event& event The event to handle.
         */
        void OnEvent(Event&);
        void OnEvent(Event&&);

        // Layer management; Forwarded to LayerStack
        void PushLayer(Layer*);
        void PushOverlay(Overlay*);
        void PopLayer(Layer*);
        void PopOverlay(Overlay*);

        // Event Handlers
        bool OnWindowClose(WindowCloseEvent&) { m_Running = false; return true; };

    protected:
        bool m_Running = true;

        ApplicationConfig m_Config;
        LayerStack m_LayerStack;
        Logger m_Logger;
    };
}

/**
 * @brief Create a Application object
 * 
 * @param int argc
 * @param char** argv
 * @return Echelon::Application*
 */
extern Echelon::Application* CreateApplication(Echelon::ApplicationCommandLineArgs&);
