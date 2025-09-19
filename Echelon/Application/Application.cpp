#include "Application.h"
#include <iostream>

namespace Echelon {
    Application::Application() {
        std::cout << "Application Created" << std::endl;
    }

    Application::~Application() {
        std::cout << "Application Destroyed" << std::endl;
    }

    void Application::Run() {
        std::cout << "Application Running..." << std::endl;
    }
}