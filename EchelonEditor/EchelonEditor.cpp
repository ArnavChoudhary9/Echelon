#include "Echelon.h"

class EchelonEditor : public Echelon::Application {
public:
    void Run() override {
        Echelon::Logger logger("EchelonEditor");
        logger.AddSink(Echelon::ConsoleSink);
        logger.AddSink(Echelon::FileSink("EchelonEditor.log"));

        logger.Info("Echelon Editor Started");
        for (int i = 0; i < 5; ++i) {
            logger.Trace("Trace message " + std::to_string(i));
            logger.Debug("Debug message " + std::to_string(i));
            logger.Info("Info message " + std::to_string(i));
            logger.Warn("Warning message " + std::to_string(i));
            logger.Error("Error message " + std::to_string(i));
        }
        logger.Fatal("Echelon Editor Ending");
    }
};

Echelon::Application* CreateApplication() {
    return new EchelonEditor();
}
