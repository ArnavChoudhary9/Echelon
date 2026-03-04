#pragma once

/**
 * @file Window.hpp
 * @brief Backend-agnostic window interface.
 *
 * Engine code interacts exclusively with this abstraction.
 * Concrete implementations (GLFW, Win32, SDL, …) live in
 * Platform/Backends/<Backend>/.
 *
 * Usage:
 * @code
 *     WindowDesc desc;
 *     desc.Title  = "My App";
 *     desc.Width  = 1920;
 *     desc.Height = 1080;
 *
 *     auto window = Window::Create(desc);
 *     window->SetEventCallback([](Event& e) { ... });
 *
 *     while (!window->ShouldClose())
 *     {
 *         window->PollEvents();
 *         // ... render ...
 *         window->SwapBuffers();
 *     }
 * @endcode
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/Events/Event.hpp"
#include "Echelon/Platform/PlatformTypes.hpp"

#include <functional>
#include <string>

namespace Echelon {

    /**
     * @brief Callback signature for dispatching platform events.
     *
     * The window backend captures OS events, wraps them in engine
     * Event objects, and invokes this callback so that the Application
     * (or any subscriber) can handle them.
     */
    using EventCallbackFn = std::function<void(Event&)>;

    /**
     * @brief Backend-agnostic window abstraction.
     *
     * Responsibilities:
     *   - Create and manage the application window
     *   - Poll OS events
     *   - Provide window dimensions and state
     *   - Provide the native window handle for graphics APIs
     */
    class Window
    {
    public:
        virtual ~Window() = default;

        // ---- Per-frame operations ----

        /**
         * @brief Poll the operating system for pending events.
         *
         * This should be called once per frame at the top of the main loop.
         * The backend translates OS events into engine Event objects and
         * dispatches them via the registered EventCallbackFn.
         */
        virtual void PollEvents() = 0;

        /**
         * @brief Present the back-buffer to the screen.
         *
         * For OpenGL this calls glfwSwapBuffers, for Vulkan it may be a
         * no-op (presentation is handled by the swapchain).
         */
        virtual void SwapBuffers() = 0;

        // ---- Queries ----

        /**
         * @brief Get the current window width in pixels.
         */
        virtual uint32_t GetWidth() const = 0;

        /**
         * @brief Get the current window height in pixels.
         */
        virtual uint32_t GetHeight() const = 0;

        /**
         * @brief Get the window title.
         */
        virtual const std::string& GetTitle() const = 0;

        /**
         * @brief Whether the window has received a close request.
         */
        virtual bool ShouldClose() const = 0;

        /**
         * @brief Retrieve the platform-native window handle.
         *
         * The handle type is platform-specific:
         *   - GLFW  : GLFWwindow*
         *   - Win32 : HWND
         *   - SDL   : SDL_Window*
         *   - Cocoa : NSWindow*
         *
         * Required for creating:
         *   - Vulkan surfaces
         *   - DirectX swap chains
         *   - Metal layers
         *
         * @return void* Opaque pointer to the native handle.
         */
        virtual void* GetNativeHandle() const = 0;

        // ---- Configuration ----

        /**
         * @brief Enable or disable vertical sync.
         * @param enabled True to enable VSync, false to disable.
         */
        virtual void SetVSync(bool enabled) = 0;

        /**
         * @brief Query VSync state.
         */
        virtual bool IsVSync() const = 0;

        /**
         * @brief Register the function that will receive all window/input events.
         *
         * The Application layer typically sets this during initialisation so
         * that platform events flow into the engine event system.
         *
         * @param callback The callback to invoke for each event.
         */
        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        // ---- Factory ----

        /**
         * @brief Create a platform window from the given descriptor.
         *
         * The backend is selected via @c desc.Backend (defaults to GLFW).
         *
         * @param desc Window creation parameters.
         * @return Scope<Window> Owning pointer to the created window.
         */
        static Scope<Window> Create(const WindowDesc& desc = WindowDesc{});
    };

} // namespace Echelon
