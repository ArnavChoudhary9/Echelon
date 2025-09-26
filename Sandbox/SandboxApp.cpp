#include "Echelon.h"

class SandboxApp : public Echelon::Application {
public:
    void Run() override {
        Echelon::Logger logger("SandboxAppLogger");
        logger.AddSink(Echelon::ConsoleSink);
        logger.AddSink(Echelon::FileSink("sandbox_log.log"));

        logger.Info("Sandbox Application Started");
        for (int i = 0; i < 5; ++i) {
            logger.Trace("Trace message " + std::to_string(i));
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
