#include "Application.h"

int main(int argc, char** argv) {
    Echelon::Application* app = CreateApplication();
    app->Run();
    delete app;
    return 0;
}
