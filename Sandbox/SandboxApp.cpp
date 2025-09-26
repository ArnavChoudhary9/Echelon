#include "Echelon.h"

class SandboxApp : public Echelon::Application {
public:
    SandboxApp() {}
    ~SandboxApp() {}

    void Run() override {
        Echelon::Logger logger("SandboxAppLogger");
        logger.AddSink(Echelon::ConsoleSink);

        logger.Info("Sandbox Application Started");
        for (int i = 0; i < 5; ++i) {
            logger.Debug("Debug message " + std::to_string(i));
            logger.Info("Info message " + std::to_string(i));
            logger.Warn("Warning message " + std::to_string(i));
            logger.Error("Error message " + std::to_string(i));
        }
        logger.Fatal("Sandbox Application Ending");
    }
};

Echelon::Application* CreateApplication() {
    return new SandboxApp();
}
