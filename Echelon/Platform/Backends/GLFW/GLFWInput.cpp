#include "GLFWInput.hpp"

#include <GLFW/glfw3.h>
#include "Echelon/Application/Application.hpp"

namespace Echelon {

    static GLFWwindow* GetGLFWWindow()
    {
        auto* window = static_cast<GLFWwindow*>(
            Application::Get().GetWindow().GetNativeHandle());
        return window;
    }

    bool GLFWInput::IsKeyPressedImpl(KeyCode keycode) const
    {
        auto* window = GetGLFWWindow();
        auto state = glfwGetKey(window, static_cast<int>(keycode));
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool GLFWInput::IsMouseButtonPressedImpl(MouseCode button) const
    {
        auto* window = GetGLFWWindow();
        auto state = glfwGetMouseButton(window, static_cast<int>(button));
        return state == GLFW_PRESS;
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
        auto* window = GetGLFWWindow();
        double xPos, yPos;
        glfwGetCursorPos(window, &xPos, &yPos);
        return { static_cast<float>(xPos), static_cast<float>(yPos) };
    }

} // namespace Echelon
