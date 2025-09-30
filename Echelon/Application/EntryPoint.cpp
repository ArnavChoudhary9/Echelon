#include "../Core/Base.h"
#include "Application.h"
#include "Core/Log.h"

int main(int argc, char** argv) {
    INIT_ECHELON_LOGGER()

    ECHELON_LOG_TRACE("Creating Application . . .");
    Echelon::ApplicationCommandLineArgs args(argc, argv);
    Echelon::Application* app = CreateApplication(args);

    ECHELON_LOG_TRACE("Running Application . . .");
    app->Run();

    ECHELON_LOG_TRACE("Deleting Application . . .");
    delete app;
    
    return 0;
}
