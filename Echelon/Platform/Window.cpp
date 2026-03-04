#include "Echelon/Platform/Window.hpp"
#include "Echelon/Core/Base.hpp"

// Backend headers — add new backends here
#include "Echelon/Platform/Backends/GLFW/GLFWWindow.hpp"

namespace Echelon {

    Scope<Window> Window::Create(const WindowDesc& desc)
    {
        switch (desc.Backend)
        {
            case PlatformBackend::GLFW:
                return CreateScope<GLFWWindow>(desc);
            case PlatformBackend::Win32:
                // TODO: return CreateScope<Win32Window>(desc);
                break;
            case PlatformBackend::SDL:
                // TODO: return CreateScope<SDLWindow>(desc);
                break;
            case PlatformBackend::Cocoa:
                // TODO: return CreateScope<CocoaWindow>(desc);
                break;
            case PlatformBackend::X11:
                // TODO: return CreateScope<X11Window>(desc);
                break;
            case PlatformBackend::Wayland:
                // TODO: return CreateScope<WaylandWindow>(desc);
                break;
            case PlatformBackend::None:
            default:
                break;
        }

        return nullptr;
    }

} // namespace Echelon
