#include "OpenGLPipeline.hpp"
#include "OpenGLShader.hpp"
#include "OpenGLUtils.hpp"

namespace Echelon {

    // ================================================================
    // Graphics pipeline
    // ================================================================

    OpenGLPipeline::OpenGLPipeline(const PipelineDesc& desc)
        : m_Raster(desc.Raster), m_Depth(desc.Depth), m_Blend(desc.Blend), m_Layout(desc.Layout)
    {
        m_Topology = OpenGLUtils::ToGLTopology(desc.Topology);
        m_Shader   = std::static_pointer_cast<OpenGLShader>(desc.ShaderProgram);

        // Create a VAO that encodes the vertex layout
        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);

        // Cache the stride for each binding point (used by glBindVertexBuffer at draw time)
        for (const auto& binding : m_Layout.Bindings) {
            m_BindingStrides[binding.Binding] = static_cast<GLsizei>(binding.Stride);
        }

        // Configure vertex attributes using GL 4.3+ separate format API.
        // This decouples the attribute format from the buffer binding,
        // so the actual VBO is associated later via glBindVertexBuffer.
        // The attribute index is the shader input Location (from reflection),
        // not a running counter — so it lines up with the SPIR-V layout(location=N).
        for (const auto& attr : m_Layout.Attributes) {
            const GLuint loc   = static_cast<GLuint>(attr.Location);
            uint32_t     count = OpenGLUtils::GetComponentCount(attr.Format);
            GLenum       type  = OpenGLUtils::ToGLBaseType(attr.Format);
            bool         isInt = OpenGLUtils::IsIntegerFormat(attr.Format);

            glEnableVertexAttribArray(loc);
            if (isInt) {
                glVertexAttribIFormat(loc, count, type, attr.Offset);
            } else {
                glVertexAttribFormat(loc, count, type, GL_FALSE, attr.Offset);
            }
            glVertexAttribBinding(loc, attr.Binding);
        }

        glBindVertexArray(0);
    }

    OpenGLPipeline::~OpenGLPipeline()
    {
        if (m_VAO)
            glDeleteVertexArrays(1, &m_VAO);
    }

    void OpenGLPipeline::Bind() const
    {
        // Shader
        if (m_Shader)
            glUseProgram(m_Shader->GetProgram());

        // VAO
        glBindVertexArray(m_VAO);

        // Rasterizer state
        glPolygonMode(GL_FRONT_AND_BACK, OpenGLUtils::ToGLPolygonMode(m_Raster.Polygon));

        if (m_Raster.Cull != CullMode::None) {
            glEnable(GL_CULL_FACE);
            glCullFace(OpenGLUtils::ToGLCullFace(m_Raster.Cull));
        } else {
            glDisable(GL_CULL_FACE);
        }
        glFrontFace(OpenGLUtils::ToGLFrontFace(m_Raster.Winding));

        if (m_Raster.DepthClampEnable)
            glEnable(GL_DEPTH_CLAMP);
        else
            glDisable(GL_DEPTH_CLAMP);

        // Depth state
        if (m_Depth.DepthTestEnable) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(OpenGLUtils::ToGLCompareFunc(m_Depth.DepthCompareOp));
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthMask(m_Depth.DepthWriteEnable ? GL_TRUE : GL_FALSE);

        // Blend state
        if (!m_Blend.Attachments.empty() && m_Blend.Attachments[0].BlendEnable) {
            glEnable(GL_BLEND);
            const auto& b = m_Blend.Attachments[0];
            glBlendFuncSeparate(
                OpenGLUtils::ToGLBlendFactor(b.SrcColorBlend),
                OpenGLUtils::ToGLBlendFactor(b.DstColorBlend),
                OpenGLUtils::ToGLBlendFactor(b.SrcAlphaBlend),
                OpenGLUtils::ToGLBlendFactor(b.DstAlphaBlend));
            glBlendEquationSeparate(
                OpenGLUtils::ToGLBlendOp(b.ColorBlendOp),
                OpenGLUtils::ToGLBlendOp(b.AlphaBlendOp));
        } else {
            glDisable(GL_BLEND);
        }
    }

    void OpenGLPipeline::Unbind() const
    {
        glBindVertexArray(0);
        glUseProgram(0);
    }

    // ================================================================
    // Compute pipeline
    // ================================================================

    OpenGLComputePipeline::OpenGLComputePipeline(const ComputePipelineDesc& desc)
    {
        m_Shader = std::static_pointer_cast<OpenGLShader>(desc.ComputeShader);
    }

    void OpenGLComputePipeline::Bind() const
    {
        if (m_Shader)
            glUseProgram(m_Shader->GetProgram());
    }

} // namespace Echelon
