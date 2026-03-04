#pragma once

/**
 * @file OpenGLPipeline.hpp
 * @brief OpenGL implementation of Pipeline / ComputePipeline.
 *
 * In OpenGL there's no first-class pipeline object. This class captures
 * all the state (shader, vertex layout, blend, depth, raster, topology)
 * and applies it when bound.
 */

#include "Echelon/GraphicsAPI/Pipeline.hpp"
#include "Echelon/Core/Base.hpp"
#include "OpenGLShader.hpp"

#include <glad/gl.h>
#include <unordered_map>

namespace Echelon {

    /**
     * @brief OpenGL graphics pipeline — wraps a VAO, shader program, and
     *        fixed-function state.
     */
    class OpenGLPipeline : public Pipeline {
    public:
        OpenGLPipeline(const PipelineDesc& desc);
        ~OpenGLPipeline() override;

        /** @brief Apply all pipeline state to the current GL context. */
        void Bind() const;

        /** @brief Restore default GL state. */
        void Unbind() const;

        GLuint GetVAO() const { return m_VAO; }
        GLenum GetTopology() const { return m_Topology; }
        Ref<Shader> GetShader() const override { return m_Shader; }
        const Ref<OpenGLShader>& GetGLShader() const { return m_Shader; }

        /** @brief Return the stride for a given vertex binding point. */
        GLsizei GetBindingStride(uint32_t binding) const {
            auto it = m_BindingStrides.find(binding);
            return it != m_BindingStrides.end() ? it->second : 0;
        }

    private:
        GLuint m_VAO = 0;
        GLenum m_Topology = GL_TRIANGLES;

        Ref<OpenGLShader> m_Shader;
        RasterState       m_Raster;
        DepthState        m_Depth;
        BlendState        m_Blend;
        VertexLayout      m_Layout;

        /** Stride per vertex binding point (for glBindVertexBuffer). */
        std::unordered_map<uint32_t, GLsizei> m_BindingStrides;
    };

    /**
     * @brief OpenGL compute pipeline.
     */
    class OpenGLComputePipeline : public ComputePipeline {
    public:
        OpenGLComputePipeline(const ComputePipelineDesc& desc);
        ~OpenGLComputePipeline() override = default;

        void Bind() const;
        const Ref<OpenGLShader>& GetShader() const { return m_Shader; }

    private:
        Ref<OpenGLShader> m_Shader;
    };

} // namespace Echelon
