#pragma once

/**
 * @file OpenGLShader.hpp
 * @brief OpenGL implementation of the Shader interface.
 *
 * Compiles GLSL source into an OpenGL shader program.
 */

#include "Echelon/GraphicsAPI/Shader.hpp"

#include <glad/gl.h>
#include <unordered_set>

namespace Echelon {

    class OpenGLShader : public Shader {
    public:
        OpenGLShader(const ShaderDesc& desc);
        ~OpenGLShader() override;

        const std::string& GetName() const override { return m_Name; }
        bool HasStage(ShaderStage stage) const override;

        GLuint GetProgram() const { return m_Program; }

        /** @brief Set a uniform by name (helpers for common types). */
        void SetInt(const std::string& name, int value) const override;
        void SetFloat(const std::string& name, float value) const override;
        void SetVec3(const std::string& name, float x, float y, float z) const override;
        void SetVec4(const std::string& name, float x, float y, float z, float w) const override;
        void SetMat4(const std::string& name, const float* value) const override;

    private:
        GLuint CompileStage(GLenum type, const char* source, uint32_t length);

        GLuint      m_Program = 0;
        std::string m_Name;
        std::unordered_set<ShaderStage> m_Stages;
    };

} // namespace Echelon
