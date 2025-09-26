#pragma once

namespace Echelon {
    class Application {
    public:
        Application() = default;
        virtual ~Application() = default;

        virtual void Run() = 0;
    };
}

extern Echelon::Application* CreateApplication();
