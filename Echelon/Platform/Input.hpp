#pragma once

/**
 * @file Input.hpp
 * @brief Backend-agnostic input polling interface.
 *
 * Engine code queries input state through this static API rather than
 * reaching into platform libraries directly.  Each platform backend
 * provides a concrete implementation that is installed at startup.
 *
 * Usage:
 * @code
 *     if (Input::IsKeyPressed(Key::Space))
 *         player.Jump();
 *
 *     auto [mx, my] = Input::GetMousePosition();
 * @endcode
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/Core/KeyCodes.hpp"
#include "Echelon/Core/MouseCodes.hpp"
#include "Echelon/Platform/PlatformTypes.hpp"

#include <utility>

namespace Echelon {

    /**
     * @brief Backend-agnostic input abstraction.
     *
     * The Input class provides a static interface for querying the current
     * state of keyboard and mouse input.  It delegates to a platform-specific
     * implementation that is created via Input::Create() and installed as
     * the singleton instance.
     *
     * Supported input types:
     *   - Keyboard key state
     *   - Mouse button state
     *   - Mouse position (absolute)
     *   - Mouse scroll (via events)
     */
    class Input
    {
    public:
        virtual ~Input() = default;

        // ---- Static query API (delegates to the installed backend) ----

        /**
         * @brief Query whether a keyboard key is currently pressed.
         * @param keycode Engine-defined key code (see KeyCodes.hpp).
         * @return True if the key is held down.
         */
        static bool IsKeyPressed(KeyCode keycode)
        {
            return s_Instance->IsKeyPressedImpl(keycode);
        }

        /**
         * @brief Query whether a mouse button is currently pressed.
         * @param button Engine-defined mouse button code (see MouseCodes.hpp).
         * @return True if the button is held down.
         */
        static bool IsMouseButtonPressed(MouseCode button)
        {
            return s_Instance->IsMouseButtonPressedImpl(button);
        }

        /**
         * @brief Get the current mouse X position in window coordinates.
         */
        static float GetMouseX()
        {
            return s_Instance->GetMouseXImpl();
        }

        /**
         * @brief Get the current mouse Y position in window coordinates.
         */
        static float GetMouseY()
        {
            return s_Instance->GetMouseYImpl();
        }

        /**
         * @brief Get the current mouse position as (x, y).
         */
        static std::pair<float, float> GetMousePosition()
        {
            return s_Instance->GetMousePositionImpl();
        }

        // ---- Factory ----

        /**
         * @brief Create and install the platform-specific Input backend.
         *
         * Called once during Application initialisation.  After this call
         * all static query methods are valid.
         *
         * @param backend The platform backend to use for input.
         * @return Scope<Input> Owning pointer (also stored internally).
         */
        static Scope<Input> Create(PlatformBackend backend = PlatformBackend::GLFW);

    protected:
        // ---- Implementation hooks (overridden by backends) ----

        virtual bool  IsKeyPressedImpl(KeyCode keycode) const = 0;
        virtual bool  IsMouseButtonPressedImpl(MouseCode button) const = 0;
        virtual float GetMouseXImpl() const = 0;
        virtual float GetMouseYImpl() const = 0;
        virtual std::pair<float, float> GetMousePositionImpl() const = 0;

    private:
        /** @brief The currently installed input backend (singleton). */
        static Input* s_Instance;
    };

} // namespace Echelon
