#include "Echelon/GraphicsAPI/GraphicsAPI.hpp"
#include "Echelon/Core/Base.hpp"
#include "Core/Log.hpp"

// Include compiled-in backend headers.
// Each include is guarded by its own ECHELON_GRAPHICS_BACKEND_* define so that
// multiple backends can be compiled into the same binary simultaneously.
#ifdef ECHELON_GRAPHICS_BACKEND_OPENGL
#include "Echelon/Platform/Backends/OpenGL/OpenGLGraphicsAPI.hpp"
#endif
// #ifdef ECHELON_GRAPHICS_BACKEND_VULKAN
// #include "Echelon/Platform/Backends/Vulkan/VulkanGraphicsAPI.hpp"
// #endif

namespace Echelon {

    Scope<GraphicsAPI> GraphicsAPI::Create(GraphicsBackend backend)
    {
        switch (backend)
        {
            case GraphicsBackend::OpenGL:
#ifdef ECHELON_GRAPHICS_BACKEND_OPENGL
                return CreateScope<OpenGLGraphicsAPI>();
#else
                ECHELON_LOG_ERROR("[GraphicsAPI] OpenGL backend was not compiled into this build.");
                break;
#endif

            case GraphicsBackend::Vulkan:
#ifdef ECHELON_GRAPHICS_BACKEND_VULKAN
                // return CreateScope<VulkanGraphicsAPI>();
                ECHELON_LOG_ERROR("[GraphicsAPI] Vulkan backend is not yet implemented.");
                break;
#else
                ECHELON_LOG_ERROR("[GraphicsAPI] Vulkan backend was not compiled into this build.");
                break;
#endif

            case GraphicsBackend::DirectX12:
#ifdef ECHELON_GRAPHICS_BACKEND_DIRECTX12
                // return CreateScope<DirectX12GraphicsAPI>();
                ECHELON_LOG_ERROR("[GraphicsAPI] DirectX12 backend is not yet implemented.");
                break;
#else
                ECHELON_LOG_ERROR("[GraphicsAPI] DirectX12 backend was not compiled into this build.");
                break;
#endif

            case GraphicsBackend::Metal:
#ifdef ECHELON_GRAPHICS_BACKEND_METAL
                // return CreateScope<MetalGraphicsAPI>();
                ECHELON_LOG_ERROR("[GraphicsAPI] Metal backend is not yet implemented.");
                break;
#else
                ECHELON_LOG_ERROR("[GraphicsAPI] Metal backend was not compiled into this build.");
                break;
#endif

            case GraphicsBackend::Headless:
            case GraphicsBackend::None:
            default:
                break;
        }

        return nullptr;
    }

    GraphicsBackend GraphicsAPI::GetDefaultBackend()
    {
        // ECHELON_DEFAULT_GRAPHICS_BACKEND_* is set by premake to the first entry
        // of --graphics-backends.  It is independent of which backends are compiled,
        // so selecting OpenGL as default while also compiling Vulkan works correctly.
#if defined(ECHELON_DEFAULT_GRAPHICS_BACKEND_OPENGL)
        return GraphicsBackend::OpenGL;
#elif defined(ECHELON_DEFAULT_GRAPHICS_BACKEND_VULKAN)
        return GraphicsBackend::Vulkan;
#elif defined(ECHELON_DEFAULT_GRAPHICS_BACKEND_DIRECTX12)
        return GraphicsBackend::DirectX12;
#elif defined(ECHELON_DEFAULT_GRAPHICS_BACKEND_METAL)
        return GraphicsBackend::Metal;
#else
#   error "No default graphics backend set. Pass --graphics-backends=<name> to premake."
#endif
    }

} // namespace Echelon
