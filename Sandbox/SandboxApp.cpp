#include "Echelon.h"
#include "iostream"

class SandboxApp : public Echelon::Application {
public:
    SandboxApp() {}
    ~SandboxApp() {}

    void Run() override {
        Echelon::Logger::Init();
        Echelon::Logger::GetCoreLogger()->warn("Initialized Log!");
        int a = 5;
        int b = 10;
        Echelon::Logger::GetClientLogger()->info("Hello! Var={0}, {1}", a, b);
        while (true);
    }
};

Echelon::Application* CreateApplication() {
    return new SandboxApp();
}
