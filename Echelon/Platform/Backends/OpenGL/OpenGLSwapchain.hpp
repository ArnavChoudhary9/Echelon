#pragma once

/**
 * @file OpenGLSwapchain.hpp
 * @brief OpenGL "swapchain" — wraps the default framebuffer and GLFW swap.
 *
 * OpenGL doesn't have a true swapchain object. This adapts GLFW's
 * double-buffering to the engine's Swapchain interface.
 */

#include "Echelon/GraphicsAPI/Swapchain.hpp"
#include "Echelon/Core/Base.hpp"

struct GLFWwindow;

namespace Echelon {

    class OpenGLSwapchain : public Swapchain {
    public:
        OpenGLSwapchain(const SwapchainDesc& desc);
        ~OpenGLSwapchain() override = default;

        Ref<Texture>     AcquireNextImage() override;
        void             Present() override;
        void             Resize(uint32_t width, uint32_t height) override;

        TextureFormat    GetFormat() const override { return m_Format; }
        uint32_t         GetWidth() const override  { return m_Width; }
        uint32_t         GetHeight() const override { return m_Height; }
        bool             IsHeadless() const override { return m_Headless; }
        void             SetVSync(bool enabled) override;
        bool             IsVSync() const override { return m_VSync; }
        uint32_t         GetCurrentImageIndex() const override { return m_CurrentImage; }
        uint32_t         GetImageCount() const override { return m_BufferCount; }
        Ref<Framebuffer> GetCurrentFramebuffer() override;

    private:
        GLFWwindow*   m_Window      = nullptr;
        TextureFormat m_Format      = TextureFormat::RGBA8_UNORM;
        uint32_t      m_Width       = 0;
        uint32_t      m_Height      = 0;
        uint32_t      m_BufferCount = 2;
        uint32_t      m_CurrentImage = 0;
        bool          m_Headless    = false;
        bool          m_VSync       = true;
    };

} // namespace Echelon
