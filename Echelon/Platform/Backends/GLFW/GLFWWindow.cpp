#include "GLFWWindow.hpp"
#include "Echelon/Core/Base.hpp"

#include <GLFW/glfw3.h>

namespace Echelon {

    // ----------------------------------------------------------------
    // Track GLFW init state so we only call glfwInit/glfwTerminate once
    // even when multiple windows exist.
    // ----------------------------------------------------------------
    static uint32_t s_GLFWWindowCount = 0;

    // ----------------------------------------------------------------
    // GLFW error callback (installed once)
    // ----------------------------------------------------------------
    static void GLFWErrorCallback(int error, const char* description)
    {
        (void)error;
        (void)description;
        // TODO: route through Echelon logger once available globally
        // ECHELON_LOG_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    // ================================================================
    // Construction / Destruction
    // ================================================================

    GLFWWindow::GLFWWindow(const WindowDesc& desc)
    {
        m_Data.Title  = desc.Title;
        m_Data.Width  = desc.Width;
        m_Data.Height = desc.Height;
        m_Data.VSync  = desc.VSync;

        if (s_GLFWWindowCount == 0)
        {
            int success = glfwInit();
            (void)success; // TODO: ECHELON_ASSERT(success, "Failed to initialise GLFW!");
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        glfwWindowHint(GLFW_RESIZABLE, desc.Resizable ? GLFW_TRUE : GLFW_FALSE);

        m_Window = glfwCreateWindow(
            static_cast<int>(desc.Width),
            static_cast<int>(desc.Height),
            desc.Title.c_str(),
            desc.Fullscreen ? glfwGetPrimaryMonitor() : nullptr,
            nullptr
        );

        ++s_GLFWWindowCount;

        // Make the OpenGL context current (needed for SwapBuffers / VSync)
        glfwMakeContextCurrent(m_Window);

        // Attach our data struct so static callbacks can reach instance state
        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(desc.VSync);
        SetupCallbacks();
    }

    GLFWWindow::~GLFWWindow()
    {
        glfwDestroyWindow(m_Window);
        --s_GLFWWindowCount;

        if (s_GLFWWindowCount == 0)
            glfwTerminate();
    }

    // ================================================================
    // Per-frame
    // ================================================================

    void GLFWWindow::PollEvents()
    {
        glfwPollEvents();
    }

    void GLFWWindow::SwapBuffers()
    {
        glfwSwapBuffers(m_Window);
    }

    // ================================================================
    // Queries
    // ================================================================

    bool GLFWWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }

    // ================================================================
    // Configuration
    // ================================================================

    void GLFWWindow::SetVSync(bool enabled)
    {
        if (enabled)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        m_Data.VSync = enabled;
    }

    // ================================================================
    // Callbacks
    // ================================================================

    void GLFWWindow::SetupCallbacks()
    {
        // --- Window size ---
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            data.Width  = static_cast<uint32_t>(width);
            data.Height = static_cast<uint32_t>(height);

            WindowResizeEvent event(width, height);
            if (data.EventCallback)
                data.EventCallback(event);
        });

        // --- Window close ---
        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            WindowCloseEvent event;
            if (data.EventCallback)
                data.EventCallback(event);
        });

        // --- Keyboard ---
        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            switch (action)
            {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(static_cast<KeyCode>(key), 0);
                    if (data.EventCallback) data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(static_cast<KeyCode>(key));
                    if (data.EventCallback) data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(static_cast<KeyCode>(key), 1);
                    if (data.EventCallback) data.EventCallback(event);
                    break;
                }
            }
        });

        // --- Character (text input) ---
        glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            KeyTypedEvent event(static_cast<KeyCode>(keycode));
            if (data.EventCallback) data.EventCallback(event);
        });

        // --- Mouse button ---
        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int /*mods*/)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            switch (action)
            {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(static_cast<MouseCode>(button));
                    if (data.EventCallback) data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(static_cast<MouseCode>(button));
                    if (data.EventCallback) data.EventCallback(event);
                    break;
                }
            }
        });

        // --- Mouse scroll ---
        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            MouseScrolledEvent event(static_cast<float>(xOffset), static_cast<float>(yOffset));
            if (data.EventCallback) data.EventCallback(event);
        });

        // --- Mouse position ---
        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
        {
            auto& data = *static_cast<WindowData*>(glfwGetWindowUserPointer(window));
            MouseMovedEvent event(static_cast<float>(xPos), static_cast<float>(yPos));
            if (data.EventCallback) data.EventCallback(event);
        });
    }

} // namespace Echelon
