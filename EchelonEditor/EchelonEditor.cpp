#include "Echelon.h"

class EchelonEditor : public Echelon::Application {
public:
    EchelonEditor() : m_Logger("EchelonEditor") {
        m_Logger.AddSink(Echelon::ConsoleSink);
        m_Logger.AddSink(Echelon::FileSink("EchelonEditor.log"));
    }
    ~EchelonEditor() override = default;

    void Run() override {
        m_Logger.Info("Echelon Editor Started");
        for (int i = 0; i < 5; ++i) {
            m_Logger.Trace("Trace message " + std::to_string(i));
            m_Logger.Debug("Debug message " + std::to_string(i));
            m_Logger.Info("Info message " + std::to_string(i));
            m_Logger.Warn("Warning message " + std::to_string(i));
            m_Logger.Error("Error message " + std::to_string(i));
        }
        m_Logger.Fatal("Echelon Editor Ending");
    }

private:
    Echelon::Logger m_Logger;
};

Echelon::Application* CreateApplication() {
    return new EchelonEditor();
}
