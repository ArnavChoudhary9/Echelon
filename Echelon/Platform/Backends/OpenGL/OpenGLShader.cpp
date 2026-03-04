#include "OpenGLShader.hpp"
#include "OpenGLUtils.hpp"
#include "Echelon/Core/Log.hpp"

#include <vector>

namespace Echelon {

    OpenGLShader::OpenGLShader(const ShaderDesc& desc)
        : m_Name(desc.DebugName)
    {
        m_Program = glCreateProgram();

        std::vector<GLuint> shaders;
        for (const auto& stage : desc.Stages) {
            GLenum glStage = OpenGLUtils::ToGLShaderStage(stage.Stage);
            GLuint shader  = CompileStage(glStage,
                                          reinterpret_cast<const char*>(stage.Source.data()),
                                          static_cast<uint32_t>(stage.Source.size()));
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
            std::vector<char> log(len);
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

    GLuint OpenGLShader::CompileStage(GLenum type, const char* source, uint32_t length)
    {
        GLuint shader = glCreateShader(type);
        GLint len = static_cast<GLint>(length);
        glShaderSource(shader, 1, &source, &len);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint logLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
            std::vector<char> log(logLen);
            glGetShaderInfoLog(shader, logLen, &logLen, log.data());
            ECHELON_LOG_ERROR("Shader compile failure: {}", log.data());
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    // ---- Uniform helpers ----

    void OpenGLShader::SetInt(const std::string& name, int value) const
    {
        GLint loc = glGetUniformLocation(m_Program, name.c_str());
        glUniform1i(loc, value);
    }

    void OpenGLShader::SetFloat(const std::string& name, float value) const
    {
        GLint loc = glGetUniformLocation(m_Program, name.c_str());
        glUniform1f(loc, value);
    }

    void OpenGLShader::SetVec3(const std::string& name, float x, float y, float z) const
    {
        GLint loc = glGetUniformLocation(m_Program, name.c_str());
        glUniform3f(loc, x, y, z);
    }

    void OpenGLShader::SetVec4(const std::string& name, float x, float y, float z, float w) const
    {
        GLint loc = glGetUniformLocation(m_Program, name.c_str());
        glUniform4f(loc, x, y, z, w);
    }

    void OpenGLShader::SetMat4(const std::string& name, const float* value) const
    {
        GLint loc = glGetUniformLocation(m_Program, name.c_str());
        glUniformMatrix4fv(loc, 1, GL_FALSE, value);
    }

} // namespace Echelon
