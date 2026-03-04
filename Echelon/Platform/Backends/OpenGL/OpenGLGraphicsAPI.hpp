#pragma once

/**
 * @file OpenGLGraphicsAPI.hpp
 * @brief OpenGL implementation of the top-level GraphicsAPI interface.
 *
 * Initialises glad, creates the OpenGL device, and provides frame lifecycle
 * management.
 */

#include "Echelon/GraphicsAPI/GraphicsAPI.hpp"
#include "Echelon/Core/Base.hpp"

namespace Echelon {

    class OpenGLGraphicsAPI : public GraphicsAPI {
    public:
        OpenGLGraphicsAPI();
        ~OpenGLGraphicsAPI() override;

        Ref<Device>      CreateDevice() override;
        GraphicsBackend  GetBackend() const override { return GraphicsBackend::OpenGL; }
        bool             InitLoader() override;
        bool             IsHeadless() const override { return false; }
        void             Submit(const Ref<CommandBuffer>& commandBuffer) override;
        void             BeginFrame() override;
        void             EndFrame() override;

        /** @brief Initialise glad. Must be called after a GL context is current. */
        bool InitGlad();

    private:
        bool m_GladInitialized = false;
    };

} // namespace Echelon
