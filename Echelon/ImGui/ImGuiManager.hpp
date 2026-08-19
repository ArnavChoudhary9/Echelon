#pragma once

/**
 * @file ImGuiManager.hpp
 * @brief Owns the Dear ImGui context + GLFW/OpenGL3 backends and drives the
 *        per-frame ImGui lifecycle.
 *
 * Lives in the engine (libEchelon) so it binds to the engine's single GLFW and
 * glad instance. The ImGui core is whole-archived into libEchelon (see
 * Echelon/premake5.lua), so the application/editor can call ImGui:: directly for
 * their own panels while sharing this one context — there is no per-DLL-boundary
 * duplication of the ImGui globals.
 *
 * The Application drives this each frame:
 * @code
 *     imgui->BeginFrame();
 *       for each overlay: OnImGUIBegin() / OnImGUIRender() / OnImGUIEnd();
 *     imgui->EndFrame();     // renders the UI onto the window backbuffer
 * @endcode
 */

#include "Echelon/Core/Base.hpp"

// Forward declare the GLFW handle to avoid pulling <GLFW/glfw3.h> into engine headers.
struct GLFWwindow;

namespace Echelon {

    class Window;

    class ImGuiManager {
    public:
        /**
         * @brief Create the ImGui context and initialise the GLFW + OpenGL3 backends.
         *
         * Requires a current GL context (the renderer must be initialised first) and
         * a GLFW-backed window. Docking is enabled. Installs chained GLFW callbacks so
         * both ImGui and the engine's event system receive input.
         */
        void Init(Window& window);

        /** @brief Shut the backends down and destroy the context (GL context must be alive). */
        void Shutdown();

        /** @brief Begin a new ImGui frame (call before dispatching overlay OnImGUI* hooks). */
        void BeginFrame();

        /** @brief Finalise the frame and render the UI onto the window's default framebuffer. */
        void EndFrame();

        bool IsInitialized() const { return m_Initialized; }

    private:
        bool        m_Initialized = false;
        GLFWwindow* m_Window      = nullptr;
    };

} // namespace Echelon
