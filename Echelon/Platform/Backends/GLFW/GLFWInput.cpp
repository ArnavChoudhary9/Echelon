#include "GLFWInput.hpp"

// TODO: Uncomment when GLFW is added as a vendor dependency
// #include <GLFW/glfw3.h>
// #include "Echelon/Application/Application.hpp"

namespace Echelon {

    // Helper: get the active GLFWwindow from the Application's Window
    // static GLFWwindow* GetGLFWWindow()
    // {
    //     // TODO: Access the application's window handle
    //     // auto* window = static_cast<GLFWwindow*>(
    //     //     Application::Get().GetWindow().GetNativeHandle());
    //     // return window;
    //     return nullptr;
    // }

    bool GLFWInput::IsKeyPressedImpl(KeyCode keycode) const
    {
        // TODO: Uncomment when GLFW is available
        // auto* window = GetGLFWWindow();
        // auto state = glfwGetKey(window, static_cast<int>(keycode));
        // return state == GLFW_PRESS || state == GLFW_REPEAT;
        (void)keycode;
        return false;
    }

    bool GLFWInput::IsMouseButtonPressedImpl(MouseCode button) const
    {
        // TODO: Uncomment when GLFW is available
        // auto* window = GetGLFWWindow();
        // auto state = glfwGetMouseButton(window, static_cast<int>(button));
        // return state == GLFW_PRESS;
        (void)button;
        return false;
    }

    float GLFWInput::GetMouseXImpl() const
    {
        auto [x, y] = GetMousePositionImpl();
        return x;
    }

    float GLFWInput::GetMouseYImpl() const
    {
        auto [x, y] = GetMousePositionImpl();
        return y;
    }

    std::pair<float, float> GLFWInput::GetMousePositionImpl() const
    {
        // TODO: Uncomment when GLFW is available
        // auto* window = GetGLFWWindow();
        // double xPos, yPos;
        // glfwGetCursorPos(window, &xPos, &yPos);
        // return { static_cast<float>(xPos), static_cast<float>(yPos) };
        return { 0.0f, 0.0f };
    }

} // namespace Echelon
