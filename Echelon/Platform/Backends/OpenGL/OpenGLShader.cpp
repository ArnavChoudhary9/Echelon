#include "OpenGLShader.hpp"
#include "OpenGLUtils.hpp"
#include "Echelon/Core/Log.hpp"

#include <vector>

namespace Echelon {

    OpenGLShader::OpenGLShader(const ShaderDesc& desc)
        : m_Name(desc.DebugName), m_Reflection(desc.Reflection)
    {
        m_Program = glCreateProgram();

        std::vector<GLuint> shaders;
        for (const auto& stage : desc.Stages) {
            GLuint shader = CompileStage(stage);
            if (shader) {
                glAttachShader(m_Program, shader);
                shaders.push_back(shader);
                m_Stages.insert(stage.Stage);
            }
        }

        glLinkProgram(m_Program);

        GLint linked = 0;
        glGetProgramiv(m_Program, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint len = 0;
            glGetProgramiv(m_Program, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(len > 0 ? len : 1);
            glGetProgramInfoLog(m_Program, len, &len, log.data());
            ECHELON_LOG_ERROR("Shader program link failure ({}): {}", m_Name, log.data());
        }

        // Detach and delete individual shader objects
        for (GLuint s : shaders) {
            glDetachShader(m_Program, s);
            glDeleteShader(s);
        }
    }

    OpenGLShader::~OpenGLShader()
    {
        if (m_Program)
            glDeleteProgram(m_Program);
    }

    bool OpenGLShader::HasStage(ShaderStage stage) const
    {
        return m_Stages.count(stage) > 0;
    }

    GLuint OpenGLShader::CompileStage(const ShaderStageDesc& stage)
    {
        const GLenum glStage = OpenGLUtils::ToGLShaderStage(stage.Stage);
        GLuint       shader  = glCreateShader(glStage);

        if (stage.Format == ShaderSourceFormat::SPIRV) {
            // ---- SPIR-V path (GL_ARB_gl_spirv) ----
            glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V,
                           stage.Source.data(),
                           static_cast<GLsizei>(stage.Source.size()));

            const char* entry = stage.EntryPoint.empty() ? "main" : stage.EntryPoint.c_str();
            glSpecializeShader(shader, entry, 0, nullptr, nullptr);

            GLint status = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
            if (!status) {
                GLint logLen = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
                std::vector<char> log(logLen > 0 ? logLen : 1);
                glGetShaderInfoLog(shader, logLen, &logLen, log.data());
                ECHELON_LOG_ERROR("SPIR-V specialize failure ({}, entry '{}'): {}",
                                  m_Name, entry, log.data());
                glDeleteShader(shader);
                return 0;
            }
            return shader;
        }

        // ---- GLSL text fallback ----
        const char* source = reinterpret_cast<const char*>(stage.Source.data());
        GLint       len    = static_cast<GLint>(stage.Source.size());
        glShaderSource(shader, 1, &source, &len);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint logLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
            std::vector<char> log(logLen > 0 ? logLen : 1);
            glGetShaderInfoLog(shader, logLen, &logLen, log.data());
            ECHELON_LOG_ERROR("Shader compile failure ({}): {}", m_Name, log.data());
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

} // namespace Echelon
