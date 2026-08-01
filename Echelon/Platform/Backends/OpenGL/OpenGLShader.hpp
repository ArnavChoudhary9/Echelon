#pragma once

/**
 * @file OpenGLShader.hpp
 * @brief OpenGL implementation of the Shader interface.
 *
 * Ingests SPIR-V directly via GL_ARB_gl_spirv (glShaderBinary +
 * glSpecializeShader). A GLSL text path is retained as a fallback for shaders
 * whose stage Format is GLSL. Uniforms are supplied via UBOs (SPIR-V forbids
 * default-block uniforms), so the classic glUniform* setters are gone.
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
        const ShaderReflection& GetReflection() const override { return m_Reflection; }

        GLuint GetProgram() const { return m_Program; }

    private:
        /**
         * @brief Compile one stage from a ShaderStageDesc.
         * SPIR-V stages are ingested with glShaderBinary + glSpecializeShader;
         * GLSL stages fall back to glShaderSource + glCompileShader.
         */
        GLuint CompileStage(const ShaderStageDesc& stage);

        GLuint           m_Program = 0;
        std::string      m_Name;
        std::unordered_set<ShaderStage> m_Stages;
        ShaderReflection m_Reflection;
    };

} // namespace Echelon
