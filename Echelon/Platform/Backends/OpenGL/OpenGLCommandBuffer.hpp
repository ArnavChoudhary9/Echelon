#pragma once

/**
 * @file OpenGLCommandBuffer.hpp
 * @brief OpenGL implementation of the CommandBuffer interface.
 *
 * OpenGL is immediate-mode: commands execute as they are called.
 * This wrapper applies state and issues GL draw/dispatch calls directly,
 * matching the CommandBuffer API contract.
 */

#include "Echelon/GraphicsAPI/CommandBuffer.hpp"
#include "Echelon/Core/Base.hpp"

#include <glad/gl.h>

namespace Echelon {

    class OpenGLPipeline;  // forward declaration

    class OpenGLCommandBuffer : public CommandBuffer {
    public:
        OpenGLCommandBuffer() = default;
        ~OpenGLCommandBuffer() override = default;

        // ---- Lifetime ----
        void Begin() override;
        void End() override;

        // ---- Render pass ----
        void BeginRenderPass(const Ref<RenderPass>& renderPass,
                             const Ref<Framebuffer>& framebuffer) override;
        void EndRenderPass() override;

        // ---- Pipeline binding ----
        void BindPipeline(const Ref<Pipeline>& pipeline) override;
        void BindComputePipeline(const Ref<ComputePipeline>& pipeline) override;

        // ---- Resource binding ----
        void BindVertexBuffer(const Ref<Buffer>& buffer,
                              uint32_t binding = 0, uint64_t offset = 0) override;
        void BindIndexBuffer(const Ref<Buffer>& buffer,
                             IndexType indexType = IndexType::UInt32,
                             uint64_t offset = 0) override;
        void BindDescriptorSet(const Ref<DescriptorSet>& descriptorSet,
                               uint32_t setIndex = 0) override;

        // ---- Drawing ----
        void Draw(uint32_t vertexCount, uint32_t instanceCount = 1,
                  uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                         uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                         uint32_t firstInstance = 0) override;

        // ---- Compute ----
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY = 1,
                      uint32_t groupCountZ = 1) override;

        // ---- Dynamic state ----
        void SetViewport(const Viewport& viewport) override;
        void SetScissor(const ScissorRect& scissor) override;

    private:
        GLenum m_CurrentTopology  = GL_TRIANGLES;
        GLenum m_CurrentIndexType = GL_UNSIGNED_INT;
        Ref<OpenGLPipeline> m_CurrentPipeline;
    };

} // namespace Echelon
