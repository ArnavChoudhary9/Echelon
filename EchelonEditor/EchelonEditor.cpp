#include "Echelon.h"

using namespace Echelon;

class EchelonEditor : public Application {
public:
    EchelonEditor(ApplicationConfig& config) : Application(config) {
        // Initialize editor-specific layers and components here
        m_Logger.Info("Echelon Editor initialized.");
    }
};

Application* CreateApplication(ApplicationCommandLineArgs& args) {
    ApplicationConfig config;
    config.Name = "Echelon Editor";
    config.Args = args;

    return new EchelonEditor(config);
}
