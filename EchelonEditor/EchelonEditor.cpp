#include "Echelon/Echelon.hpp"
#include "EditorOverlay.hpp"

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
    config.WindowDimensions = { 1280, 720 };

    return new EchelonEditor(config);
}
