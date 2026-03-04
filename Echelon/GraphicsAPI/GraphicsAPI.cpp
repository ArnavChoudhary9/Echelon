#include "Echelon/GraphicsAPI/GraphicsAPI.hpp"
#include "Echelon/Core/Base.hpp"

namespace Echelon {

    Scope<GraphicsAPI> GraphicsAPI::Create(GraphicsBackend backend)
    {
        switch (backend)
        {
            case GraphicsBackend::Headless:
                // TODO: return CreateScope<HeadlessGraphicsAPI>();
                break;
            case GraphicsBackend::OpenGL:
                // TODO: return CreateScope<OpenGLGraphicsAPI>();
                break;
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

} // namespace Echelon
