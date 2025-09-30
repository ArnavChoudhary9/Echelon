#include "Echelon.h"

using namespace Echelon;

class EchelonEditor : public Application {
public:
    EchelonEditor(ApplicationConfig& config) : Application(config), m_Logger("EchelonEditor") {
        m_Logger.AddSink(ConsoleSink);
        m_Logger.AddSink(FileSink("EchelonEditor.log"));

        m_Logger.Debug("Argument Count: {}", config.Args.GetArgCount());
    }
    ~EchelonEditor() override = default;

    void Run() override {
        m_Logger.Info("Echelon Editor Started");
        for (int i = 0; i < 5; ++i) {
            m_Logger.Trace("Trace message: {}", i);
            m_Logger.Debug("Debug message: {}", i);
            m_Logger.Info("Info message: {}", i);
            m_Logger.Warn("Warning message: {}", i);
            m_Logger.Error("Error message: {}", i);
        }

        m_Logger.Fatal("Echelon Editor Ending");
    }

private:
    Logger m_Logger;
};

Application* CreateApplication(int argc, char** argv) {
    ApplicationConfig config;
    config.Args = ApplicationCommandLineArgs(argc, argv);
    return new EchelonEditor(config);
}
