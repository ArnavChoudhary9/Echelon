#pragma once

/**
 * @file GLFWWindow.hpp
 * @brief GLFW implementation of the Window interface.
 *
 * This is the recommended first backend per the engine design.
 * It translates GLFW callbacks into Echelon Event objects and
 * forwards them through the registered EventCallbackFn.
 */

#include "Echelon/Platform/Window.hpp"
#include "Echelon/Platform/PlatformTypes.hpp"
#include "Echelon/Events/Event.hpp"
#include "Echelon/Events/ApplicationEvent.hpp"
#include "Echelon/Events/KeyEvents.hpp"
#include "Echelon/Events/MouseEvent.hpp"

// Forward declare the GLFW handle to avoid exposing the GLFW header
// to engine code.  The .cpp includes <GLFW/glfw3.h>.
struct GLFWwindow;

namespace Echelon {

    /**
     * @brief GLFW-backed window implementation.
     *
     * Manages a single GLFWwindow and installs callbacks that convert
     * GLFW events into Echelon::Event objects dispatched via the
     * EventCallbackFn.
     */
    class GLFWWindow : public Window
    {
    public:
        /**
         * @brief Construct and open a GLFW window.
         * @param desc Window creation parameters.
         */
        explicit GLFWWindow(const WindowDesc& desc);

        ~GLFWWindow() override;

        // ---- Per-frame operations ----
        void PollEvents() override;
        void SwapBuffers() override;

        // ---- Queries ----
        uint32_t           GetWidth()        const override { return m_Data.Width; }
        uint32_t           GetHeight()       const override { return m_Data.Height; }
        const std::string& GetTitle()        const override { return m_Data.Title; }
        bool               ShouldClose()     const override;
        void*              GetNativeHandle() const override { return static_cast<void*>(m_Window); }

        // ---- Configuration ----
        void SetVSync(bool enabled) override;
        bool IsVSync() const override { return m_Data.VSync; }
        double GetTime() const override;
        void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }

    private:
        /**
         * @brief Internal data bundle attached to the GLFW window via
         *        glfwSetWindowUserPointer so that static callbacks can
         *        reach instance state.
         */
        struct WindowData
        {
            std::string     Title;
            uint32_t        Width  = 0;
            uint32_t        Height = 0;
            bool            VSync  = true;
            EventCallbackFn EventCallback;
        };

        /** @brief Install all GLFW event callbacks. */
        void SetupCallbacks();

        GLFWwindow* m_Window = nullptr;
        WindowData  m_Data;
    };

} // namespace Echelon
