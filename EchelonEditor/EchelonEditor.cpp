#include "Echelon.h"
#include "EditorOverlay.cpp"

using namespace Echelon;

class EchelonEditor : public Application {
public:
    EchelonEditor(ApplicationConfig& config) : Application(config) {
        PushOverlay(new EditorOverlay());
    }
};

Application* CreateApplication(ApplicationCommandLineArgs& args) {
    ApplicationConfig config;
    config.Name = "Echelon Editor";
    config.Args = args;

    return new EchelonEditor(config);
}
