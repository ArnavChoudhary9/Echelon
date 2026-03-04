#pragma once

/**
 * @file PlatformTypes.hpp
 * @brief Shared / cross-cutting types used by the Platform abstraction layer.
 *
 * Domain-specific types live alongside their interface:
 *   Window types  -> Window.hpp
 *   Input types   -> Input.hpp
 */

#include <cstdint>
#include <string>
#include <functional>

namespace Echelon {

    // ================================================================
    // Backend selection
    // ================================================================

    /**
     * @brief Enumerates the supported platform backends.
     *
     * Each backend provides concrete implementations of the Window and
     * Input interfaces.  Engine code should never reference a specific
     * backend directly — use the factory functions instead.
     */
    enum class PlatformBackend : uint8_t
    {
        None = 0,   ///< No backend — null/stub (useful for unit tests)
        GLFW,       ///< GLFW  (recommended first backend)
        Win32,      ///< Native Win32
        SDL,        ///< SDL2 / SDL3
        Cocoa,      ///< macOS Cocoa / AppKit
        X11,        ///< X11 (Linux)
        Wayland     ///< Wayland (Linux)
    };

    // ================================================================
    // Window descriptor
    // ================================================================

    /**
     * @brief Parameters for creating a platform window.
     *
     * Pass this to Window::Create() to configure the window before it
     * is opened.  Sensible defaults are provided.
     */
    struct WindowDesc
    {
        uint32_t    Width       = 1280;
        uint32_t    Height      = 720;
        std::string Title       = "Echelon";

        bool        Resizable   = true;
        bool        Fullscreen  = false;
        bool        VSync       = true;

        /** The platform backend to use.  Defaults to GLFW. */
        PlatformBackend Backend = PlatformBackend::GLFW;
    };

} // namespace Echelon
