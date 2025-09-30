#pragma once

namespace Echelon {
    /**
     * @brief Represents the core application class. Extend this class to create a custom application.
     * 
     */
    class Application {
    public:
        Application() = default;
        virtual ~Application() = default;
        
        /**
         * @brief Starts the application's main loop.
         * 
         */
        virtual void Run() = 0;
    };
}

/**
 * @brief Create a Application object
 * 
 * @return Echelon::Application* 
 */
extern Echelon::Application* CreateApplication();
