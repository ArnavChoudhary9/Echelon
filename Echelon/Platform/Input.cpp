#include "Echelon/Platform/Input.hpp"
#include "Echelon/Core/Base.hpp"

// Backend headers — add new backends here
#include "Echelon/Platform/Backends/GLFW/GLFWInput.hpp"

namespace Echelon {

    // Singleton instance — set by Application during startup
    Input* Input::s_Instance = nullptr;

    Scope<Input> Input::Create(PlatformBackend backend)
    {
        Scope<Input> input = nullptr;

        switch (backend)
        {
            case PlatformBackend::GLFW:
                input = CreateScope<GLFWInput>();
                break;
            case PlatformBackend::Win32:
                // TODO: input = CreateScope<Win32Input>();
                break;
            case PlatformBackend::SDL:
                // TODO: input = CreateScope<SDLInput>();
                break;
            case PlatformBackend::Cocoa:
                // TODO: input = CreateScope<CocoaInput>();
                break;
            case PlatformBackend::X11:
                // TODO: input = CreateScope<X11Input>();
                break;
            case PlatformBackend::Wayland:
                // TODO: input = CreateScope<WaylandInput>();
                break;
            case PlatformBackend::None:
            default:
                break;
        }

        // Install singleton so static query methods work
        if (input)
            s_Instance = input.get();

        return input;
    }

} // namespace Echelon
