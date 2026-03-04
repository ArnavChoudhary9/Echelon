#include "OpenGLSwapchain.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace Echelon {

    OpenGLSwapchain::OpenGLSwapchain(const SwapchainDesc& desc)
        : m_Format(desc.Format), m_Width(desc.Width), m_Height(desc.Height),
          m_BufferCount(desc.BufferCount), m_Headless(desc.Headless),
          m_VSync(desc.VSync)
    {
        m_Window = static_cast<GLFWwindow*>(desc.NativeWindow);

        if (m_Window) {
            glfwSwapInterval(m_VSync ? 1 : 0);
        }
    }

    Ref<Texture> OpenGLSwapchain::AcquireNextImage()
    {
        // OpenGL's default framebuffer is always "acquired"
        // Return nullptr to signal "use the default framebuffer"
        m_CurrentImage = (m_CurrentImage + 1) % m_BufferCount;
        return nullptr;
    }

    void OpenGLSwapchain::Present()
    {
        if (m_Window && !m_Headless) {
            glfwSwapBuffers(m_Window);
        }
    }

    void OpenGLSwapchain::Resize(uint32_t width, uint32_t height)
    {
        m_Width  = width;
        m_Height = height;
        glViewport(0, 0, width, height);
    }

    Ref<Framebuffer> OpenGLSwapchain::GetCurrentFramebuffer()
    {
        // Default framebuffer (0) — we return nullptr to mean "use default"
        return nullptr;
    }

    void OpenGLSwapchain::SetVSync(bool enabled)
    {
        m_VSync = enabled;
        if (m_Window) {
            glfwSwapInterval(m_VSync ? 1 : 0);
        }
    }

} // namespace Echelon
