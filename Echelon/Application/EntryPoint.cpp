#include "Application.h"
#include "Logger/Logger.h"

int main(int argc, char** argv) {
    // Initialize Core Logger
    Echelon::s_CoreLogger = std::make_shared<Echelon::Logger>("Echelon");
    Echelon::s_CoreLogger->AddSink(Echelon::ConsoleSink);
    Echelon::s_CoreLogger->AddSink(Echelon::FileSink("Echelon.log"));

    Echelon::s_CoreLogger->Trace("Creating Application . . .");
    Echelon::Application* app = CreateApplication(argc, argv);

    Echelon::s_CoreLogger->Trace("Running Application . . .");
    app->Run();

    Echelon::s_CoreLogger->Trace("Deleting Application . . .");
    delete app;
    
    return 0;
}
