#include "Echelon/GraphicsAPI/GraphicsAPI.hpp"
#include "Echelon/Core/Base.hpp"
#include "Echelon/Platform/Backends/OpenGL/OpenGLGraphicsAPI.hpp"

namespace Echelon {

    Scope<GraphicsAPI> GraphicsAPI::Create(GraphicsBackend backend)
    {
        switch (backend)
        {
            case GraphicsBackend::Headless:
                // TODO: return CreateScope<HeadlessGraphicsAPI>();
                break;
            case GraphicsBackend::OpenGL:
                return CreateScope<OpenGLGraphicsAPI>();
            case GraphicsBackend::Vulkan:
                // TODO: return CreateScope<VulkanGraphicsAPI>();
                break;
            case GraphicsBackend::DirectX12:
                // TODO: return CreateScope<DirectX12GraphicsAPI>();
                break;
            case GraphicsBackend::Metal:
                // TODO: return CreateScope<MetalGraphicsAPI>();
                break;
            case GraphicsBackend::None:
            default:
                break;
        }

        return nullptr;
    }

    GraphicsBackend GraphicsAPI::GetDefaultBackend()
    {
#if defined(ECHELON_GRAPHICS_BACKEND_VULKAN)
        return GraphicsBackend::Vulkan;
#elif defined(ECHELON_GRAPHICS_BACKEND_DX12)
        return GraphicsBackend::DirectX12;
#elif defined(ECHELON_GRAPHICS_BACKEND_METAL)
        return GraphicsBackend::Metal;
#elif defined(ECHELON_GRAPHICS_BACKEND_HEADLESS)
        return GraphicsBackend::Headless;
#else
        // Default to OpenGL
        return GraphicsBackend::OpenGL;
#endif
    }

} // namespace Echelon
