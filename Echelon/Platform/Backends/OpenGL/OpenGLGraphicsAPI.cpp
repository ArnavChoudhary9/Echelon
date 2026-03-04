#include "OpenGLGraphicsAPI.hpp"
#include "OpenGLDevice.hpp"
#include "Echelon/Core/Log.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace Echelon {

    // ---- GL debug callback (OpenGL 4.3+) ----
#ifdef ECHELON_DEBUG
    static void GLAPIENTRY GLDebugCallback(GLenum /*source*/, GLenum type,
                                           GLuint /*id*/, GLenum severity,
                                           GLsizei /*length*/, const GLchar* message,
                                           const void* /*userParam*/)
    {
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

        switch (type) {
            case GL_DEBUG_TYPE_ERROR:
                ECHELON_LOG_ERROR("[OpenGL] {}", message);
                break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                ECHELON_LOG_WARN("[OpenGL] {}", message);
                break;
            default:
                ECHELON_LOG_INFO("[OpenGL] {}", message);
                break;
        }
    }
#endif

    OpenGLGraphicsAPI::OpenGLGraphicsAPI()
    {
    }

    OpenGLGraphicsAPI::~OpenGLGraphicsAPI()
    {
    }

    bool OpenGLGraphicsAPI::InitGlad()
    {
        if (m_GladInitialized) return true;

        int version = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
        if (!version) {
            ECHELON_LOG_ERROR("Failed to initialise glad (OpenGL loader)");
            return false;
        }

        ECHELON_LOG_INFO("OpenGL loaded: {}.{}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
        ECHELON_LOG_INFO("  Vendor:   {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
        ECHELON_LOG_INFO("  Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        ECHELON_LOG_INFO("  Version:  {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

#ifdef ECHELON_DEBUG
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(GLDebugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
#endif

        m_GladInitialized = true;
        return true;
    }

    bool OpenGLGraphicsAPI::InitLoader()
    {
        return InitGlad();
    }

    Ref<Device> OpenGLGraphicsAPI::CreateDevice()
    {
        if (!m_GladInitialized) {
            InitGlad();
        }
        return CreateRef<OpenGLDevice>();
    }

    void OpenGLGraphicsAPI::Submit(const Ref<CommandBuffer>& /*commandBuffer*/)
    {
        // OpenGL is immediate-mode.  Commands have already executed.
        // Nothing to do here.
    }

    void OpenGLGraphicsAPI::BeginFrame()
    {
        // No per-frame synchronisation needed in OpenGL
    }

    void OpenGLGraphicsAPI::EndFrame()
    {
        // Could insert a glFinish() for synchronisation if needed
    }

} // namespace Echelon
