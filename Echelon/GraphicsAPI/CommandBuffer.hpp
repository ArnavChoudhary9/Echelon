#pragma once

/**
 * @file CommandBuffer.hpp
 * @brief Command buffer interface for recording all GPU work.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // IndexType, Viewport, ScissorRect

#include <cstdint>
#include <vector>

namespace Echelon {

    // Forward declarations
    class Pipeline;
    class ComputePipeline;
    class Buffer;
    class Texture;
    class RenderPass;
    class Framebuffer;
    class DescriptorSet;

    /**
     * @brief Abstract command buffer for recording GPU work.
     *
     * All GPU operations — draws, dispatches, resource copies — are recorded
     * into command buffers which are then submitted to GPU queues.  This mirrors
     * the explicit Vulkan / DX12 / Metal model.
     *
     * Typical usage:
     * @code
     *     cmd->Begin();
     *       cmd->BeginRenderPass(pass, framebuffer);
     *         cmd->BindPipeline(pipeline);
     *         cmd->BindVertexBuffer(vb);
     *         cmd->BindIndexBuffer(ib);
     *         cmd->BindDescriptorSet(set, 0);
     *         cmd->DrawIndexed(indexCount);
     *       cmd->EndRenderPass();
     *     cmd->End();
     * @endcode
     */
    class CommandBuffer
    {
    public:
        virtual ~CommandBuffer() = default;

        // ---- Lifetime ----

        /**
         * @brief Begin recording commands.  Must be called before any other
         *        recording method.
         */
        virtual void Begin() = 0;

        /**
         * @brief End recording commands.  The buffer is ready for submission.
         */
        virtual void End() = 0;

        // ---- Render pass scope ----

        /**
         * @brief Begin a render pass targeting the given framebuffer.
         *
         * @param renderPass   The render pass describing attachments & ops.
         * @param framebuffer  The framebuffer providing the actual textures
         *                     (color + optional depth) to render into.
         */
        virtual void BeginRenderPass(const Ref<RenderPass>& renderPass,
                                     const Ref<Framebuffer>& framebuffer) = 0;

        /**
         * @brief End the current render pass.
         */
        virtual void EndRenderPass() = 0;

        // ---- Pipeline binding ----

        /**
         * @brief Bind a graphics pipeline for subsequent draw calls.
         * @param pipeline The graphics pipeline to bind.
         */
        virtual void BindPipeline(const Ref<Pipeline>& pipeline) = 0;

        /**
         * @brief Bind a compute pipeline for subsequent dispatch calls.
         * @param pipeline The compute pipeline to bind.
         */
        virtual void BindComputePipeline(const Ref<ComputePipeline>& pipeline) = 0;

        // ---- Resource binding ----

        /**
         * @brief Bind a vertex buffer.
         * @param buffer  The vertex buffer.
         * @param binding The vertex buffer binding index.
         * @param offset  Byte offset into the buffer.
         */
        virtual void BindVertexBuffer(const Ref<Buffer>& buffer,
                                      uint32_t binding = 0,
                                      uint64_t offset = 0) = 0;

        /**
         * @brief Bind an index buffer.
         * @param buffer    The index buffer.
         * @param indexType Data type of indices (UInt16 or UInt32).
         * @param offset    Byte offset into the buffer.
         */
        virtual void BindIndexBuffer(const Ref<Buffer>& buffer,
                                     IndexType indexType = IndexType::UInt32,
                                     uint64_t offset = 0) = 0;

        /**
         * @brief Bind a descriptor set.
         * @param descriptorSet The descriptor set to bind.
         * @param setIndex      The set index (e.g. 0, 1, 2 …).
         */
        virtual void BindDescriptorSet(const Ref<DescriptorSet>& descriptorSet,
                                       uint32_t setIndex = 0) = 0;

        // ---- Drawing ----

        /**
         * @brief Issue a non-indexed draw call.
         * @param vertexCount   Number of vertices to draw.
         * @param instanceCount Number of instances.
         * @param firstVertex   Offset of the first vertex.
         * @param firstInstance Offset of the first instance.
         */
        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1,
                          uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;

        /**
         * @brief Issue an indexed draw call.
         * @param indexCount    Number of indices to draw.
         * @param instanceCount Number of instances.
         * @param firstIndex    Offset of the first index.
         * @param vertexOffset  Value added to each index before vertex fetch.
         * @param firstInstance Offset of the first instance.
         */
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                                 uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                                 uint32_t firstInstance = 0) = 0;

        // ---- Compute dispatch ----

        /**
         * @brief Dispatch compute work groups.
         * @param groupCountX Number of work groups in X dimension.
         * @param groupCountY Number of work groups in Y dimension.
         * @param groupCountZ Number of work groups in Z dimension.
         */
        virtual void Dispatch(uint32_t groupCountX,
                              uint32_t groupCountY = 1,
                              uint32_t groupCountZ = 1) = 0;

        // ---- Dynamic state ----

        /**
         * @brief Set the viewport rectangle dynamically.
         * @param viewport Viewport parameters.
         */
        virtual void SetViewport(const Viewport& viewport) = 0;

        /**
         * @brief Set the scissor rectangle dynamically.
         * @param scissor Scissor parameters.
         */
        virtual void SetScissor(const ScissorRect& scissor) = 0;
    };

} // namespace Echelon
