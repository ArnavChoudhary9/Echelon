#pragma once

#include "../Layer/LayerStack.h"

namespace Echelon {
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
        Application(int argc, char** argv) : m_Argc(argc), m_Argv(argv), m_LayerStack() {}
        virtual ~Application() = default;
        
        /**
         * @brief Starts the application's main loop.
         * 
         */
        virtual void Run() = 0;

    protected:
        int m_Argc;
        char** m_Argv;

        LayerStack m_LayerStack;
    };
}

/**
 * @brief Create a Application object
 * 
 * @param int argc
 * @param char** argv
 * @return Echelon::Application* 
 */
extern Echelon::Application* CreateApplication(int, char**);
