#pragma once

/**
 * @file GLFWInput.hpp
 * @brief GLFW implementation of the Input interface.
 *
 * Queries GLFW for real-time keyboard and mouse state.
 * Requires a valid GLFWwindow* to be available (set during
 * window creation).
 */

#include "Echelon/Platform/Input.hpp"

struct GLFWwindow;

namespace Echelon {

    /**
     * @brief GLFW-backed input implementation.
     *
     * All query methods delegate to glfwGetKey / glfwGetMouseButton /
     * glfwGetCursorPos using the active GLFWwindow handle.
     */
    class GLFWInput : public Input
    {
    protected:
        bool  IsKeyPressedImpl(KeyCode keycode) const override;
        bool  IsMouseButtonPressedImpl(MouseCode button) const override;
        float GetMouseXImpl() const override;
        float GetMouseYImpl() const override;
        std::pair<float, float> GetMousePositionImpl() const override;
    };

} // namespace Echelon
