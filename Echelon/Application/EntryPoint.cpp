#include "Echelon/Core/Base.h"
#include "Echelon/Core/Log.h"
#include "Echelon/Instrumentation/Instrumentation.h"
#include "Application.h"

int main(int argc, char** argv) {
    // ------ Startup ------ //
    ECHELON_PROFILE_BEGIN_SESSION("Startup", "profile_startup.json");
    INIT_ECHELON_LOGGER()

    ECHELON_LOG_TRACE("Creating Application . . .");
    Echelon::ApplicationCommandLineArgs args(argc, argv);
    Echelon::Application* app;
    {
        ECHELON_PROFILE_SCOPE("CreateApplication");
        app = CreateApplication(args);
    }
    ECHELON_PROFILE_END_SESSION(); // End startup profiling session

    // ------ Runtime ------ //
    ECHELON_PROFILE_BEGIN_SESSION("Runtime", "profile_runtime.json");
    ECHELON_LOG_TRACE("Running Application . . .");
    app->Run();
    ECHELON_PROFILE_END_SESSION(); // End runtime profiling session

    // ------ Shutdown ------ //
    ECHELON_PROFILE_BEGIN_SESSION("Shutdown", "profile_shutdown.json");
    ECHELON_LOG_TRACE("Deleting Application . . .");
    delete app;
    ECHELON_PROFILE_END_SESSION(); // End shutdown profiling session
    
    return 0;
}
