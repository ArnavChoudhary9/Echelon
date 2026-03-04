#include "OpenGLCommandBuffer.hpp"
#include "OpenGLPipeline.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLFramebuffer.hpp"
#include "OpenGLRenderPass.hpp"
#include "OpenGLDescriptorSet.hpp"
#include "OpenGLUtils.hpp"

namespace Echelon {

    void OpenGLCommandBuffer::Begin()
    {
        // OpenGL is immediate-mode — nothing to record
    }

    void OpenGLCommandBuffer::End()
    {
        glFlush();
    }

    void OpenGLCommandBuffer::BeginRenderPass(const Ref<RenderPass>& renderPass,
                                               const Ref<Framebuffer>& framebuffer)
    {
        auto glPass = std::static_pointer_cast<OpenGLRenderPass>(renderPass);

        if (framebuffer) {
            auto glFB = std::static_pointer_cast<OpenGLFramebuffer>(framebuffer);
            glFB->Bind();
        } else {
            // Default framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // Apply clear operations from the render pass description
        if (glPass) {
            const auto& desc = glPass->GetDesc();

            for (uint32_t i = 0; i < static_cast<uint32_t>(desc.ColorAttachments.size()); ++i) {
                const auto& att = desc.ColorAttachments[i];
                if (att.Load == LoadOp::Clear) {
                    GLfloat color[4] = { att.Clear.R, att.Clear.G, att.Clear.B, att.Clear.A };
                    glClearBufferfv(GL_COLOR, i, color);
                }
            }

            if (desc.HasDepthAttachment && desc.DepthAttachment.Load == LoadOp::Clear) {
                glDepthMask(GL_TRUE);
                GLfloat depth = desc.DepthAttachment.Clear.Depth;
                glClearBufferfv(GL_DEPTH, 0, &depth);
            }
        }
    }

    void OpenGLCommandBuffer::EndRenderPass()
    {
        OpenGLFramebuffer::Unbind();
    }

    void OpenGLCommandBuffer::BindPipeline(const Ref<Pipeline>& pipeline)
    {
        auto glPipeline = std::static_pointer_cast<OpenGLPipeline>(pipeline);
        if (glPipeline) {
            glPipeline->Bind();
            m_CurrentTopology = glPipeline->GetTopology();
            m_CurrentPipeline = glPipeline;
        }
    }

    void OpenGLCommandBuffer::BindComputePipeline(const Ref<ComputePipeline>& pipeline)
    {
        auto glPipeline = std::static_pointer_cast<OpenGLComputePipeline>(pipeline);
        if (glPipeline) {
            glPipeline->Bind();
        }
    }

    void OpenGLCommandBuffer::BindVertexBuffer(const Ref<Buffer>& buffer,
                                                uint32_t binding, uint64_t offset)
    {
        auto glBuf = std::static_pointer_cast<OpenGLBuffer>(buffer);
        if (glBuf) {
            // Use GL 4.3+ glBindVertexBuffer to associate the buffer with
            // a binding point in the currently bound VAO.  The attribute
            // format was already set up by OpenGLPipeline using
            // glVertexAttribFormat / glVertexAttribBinding.
            GLsizei stride = m_CurrentPipeline
                ? m_CurrentPipeline->GetBindingStride(binding)
                : 0;
            glBindVertexBuffer(binding, glBuf->GetHandle(),
                               static_cast<GLintptr>(offset), stride);
        }
    }

    void OpenGLCommandBuffer::BindIndexBuffer(const Ref<Buffer>& buffer,
                                               IndexType indexType, uint64_t /*offset*/)
    {
        auto glBuf = std::static_pointer_cast<OpenGLBuffer>(buffer);
        if (glBuf) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuf->GetHandle());
            m_CurrentIndexType = OpenGLUtils::ToGLIndexType(indexType);
        }
    }

    void OpenGLCommandBuffer::BindDescriptorSet(const Ref<DescriptorSet>& descriptorSet,
                                                  uint32_t setIndex)
    {
        auto glSet = std::static_pointer_cast<OpenGLDescriptorSet>(descriptorSet);
        if (glSet) {
            glSet->Bind(setIndex);
        }
    }

    void OpenGLCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount,
                                    uint32_t firstVertex, uint32_t /*firstInstance*/)
    {
        if (instanceCount > 1) {
            glDrawArraysInstanced(m_CurrentTopology, firstVertex, vertexCount, instanceCount);
        } else {
            glDrawArrays(m_CurrentTopology, firstVertex, vertexCount);
        }
    }

    void OpenGLCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                           uint32_t firstIndex, int32_t vertexOffset,
                                           uint32_t /*firstInstance*/)
    {
        const void* offset = reinterpret_cast<const void*>(
            static_cast<uintptr_t>(firstIndex) *
            (m_CurrentIndexType == GL_UNSIGNED_SHORT ? 2u : 4u));

        if (instanceCount > 1) {
            glDrawElementsInstancedBaseVertex(
                m_CurrentTopology, indexCount, m_CurrentIndexType,
                offset, instanceCount, vertexOffset);
        } else if (vertexOffset != 0) {
            glDrawElementsBaseVertex(
                m_CurrentTopology, indexCount, m_CurrentIndexType,
                offset, vertexOffset);
        } else {
            glDrawElements(m_CurrentTopology, indexCount, m_CurrentIndexType, offset);
        }
    }

    void OpenGLCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY,
                                        uint32_t groupCountZ)
    {
        glDispatchCompute(groupCountX, groupCountY, groupCountZ);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    void OpenGLCommandBuffer::SetViewport(const Viewport& viewport)
    {
        glViewport(static_cast<GLint>(viewport.X),
                   static_cast<GLint>(viewport.Y),
                   static_cast<GLsizei>(viewport.Width),
                   static_cast<GLsizei>(viewport.Height));
        glDepthRange(viewport.MinDepth, viewport.MaxDepth);
    }

    void OpenGLCommandBuffer::SetScissor(const ScissorRect& scissor)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(scissor.X, scissor.Y, scissor.Width, scissor.Height);
    }

} // namespace Echelon
