#include "Echelon.h"
#include "iostream"

class SandboxApp : public Echelon::Application {
public:
    SandboxApp() {}
    ~SandboxApp() {}

    void Run() override {
        std::cout << "Sandbox Application Running!" << std::endl;
    }
};

Echelon::Application* CreateApplication() {
    return new SandboxApp();
}
