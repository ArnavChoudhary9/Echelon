#include "Echelon/Echelon.hpp"
#include "EditorLayer.hpp"

using namespace Echelon;

class EchelonEditor : public Application {
public:
    EchelonEditor(ApplicationConfig& config) : Application(config) {
        // EditorLayer is the coordinator overlay; it spawns the panel overlays
        // (Toolbar/Viewport/Hierarchy/Inspector/Stats) in its OnAttach.
        PushOverlay(CreateRef<EditorLayer>());
    }
};

Scope<Application> CreateApplication(ApplicationCommandLineArgs& args) {
    ApplicationConfig config;
    config.Name = "Echelon Editor";
    config.Args = args;
    config.WindowDimensions = { 1280, 720 };

    return CreateScope<EchelonEditor>(config);
}
