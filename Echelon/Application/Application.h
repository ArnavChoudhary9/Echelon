#pragma once

namespace Echelon {
    class Application {
    public:
        Application();
        virtual ~Application();

        virtual void Run();
    };
}

extern Echelon::Application* CreateApplication();
