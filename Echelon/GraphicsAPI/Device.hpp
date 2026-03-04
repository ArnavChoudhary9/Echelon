#pragma once

/**
 * @file Device.hpp
 * @brief The central GPU controller responsible for creating all resources.
 *
 * All creation descriptors live alongside their respective interfaces:
 *   BufferDesc            → Buffer.hpp
 *   TextureDesc           → Texture.hpp
 *   ShaderDesc            → Shader.hpp
 *   PipelineDesc          → Pipeline.hpp
 *   ComputePipelineDesc   → Pipeline.hpp
 *   RenderPassDesc        → RenderPass.hpp
 *   FramebufferDesc       → Framebuffer.hpp
 *   SwapchainDesc         → Swapchain.hpp
 *   DescriptorSetLayoutDesc → DescriptorSet.hpp
 */

#include "Echelon/Core/Base.hpp"

// Include descriptors from their canonical locations
#include "Echelon/GraphicsAPI/Buffer.hpp"
#include "Echelon/GraphicsAPI/Texture.hpp"
#include "Echelon/GraphicsAPI/Shader.hpp"
#include "Echelon/GraphicsAPI/Pipeline.hpp"
#include "Echelon/GraphicsAPI/RenderPass.hpp"
#include "Echelon/GraphicsAPI/Framebuffer.hpp"
#include "Echelon/GraphicsAPI/Swapchain.hpp"
#include "Echelon/GraphicsAPI/CommandBuffer.hpp"
#include "Echelon/GraphicsAPI/DescriptorSet.hpp"

namespace Echelon {

    /**
     * @brief The Device is the central GPU controller responsible for creating
     *        all GPU resources, pipelines and command buffers.
     *
     * Each backend (OpenGL, Vulkan, DirectX12, Metal, Headless) provides a
     * concrete implementation of this interface.  Engine code must interact
     * exclusively through this abstraction.
     */
    class Device
    {
    public:
        virtual ~Device() = default;

        // ---- Resource creation ----

        /**
         * @brief Create a GPU buffer (vertex, index, uniform, storage, etc.).
         * @param desc Buffer descriptor (see Buffer.hpp).
         * @return Ref<Buffer> Shared handle to the created buffer.
         */
        virtual Ref<Buffer> CreateBuffer(const BufferDesc& desc) = 0;

        /**
         * @brief Create a texture resource.
         * @param desc Texture descriptor (see Texture.hpp).
         * @return Ref<Texture> Shared handle to the created texture.
         */
        virtual Ref<Texture> CreateTexture(const TextureDesc& desc) = 0;

        /**
         * @brief Create a shader program from one or more compiled stages.
         * @param desc Shader descriptor (see Shader.hpp).
         * @return Ref<Shader> Shared handle to the created shader.
         */
        virtual Ref<Shader> CreateShader(const ShaderDesc& desc) = 0;

        // ---- Pipeline creation ----

        /**
         * @brief Create a graphics pipeline (rasterization).
         * @param desc Pipeline descriptor (see Pipeline.hpp).
         * @return Ref<Pipeline> Shared handle to the created pipeline.
         */
        virtual Ref<Pipeline> CreatePipeline(const PipelineDesc& desc) = 0;

        /**
         * @brief Create a compute pipeline.
         * @param desc Compute pipeline descriptor (see Pipeline.hpp).
         * @return Ref<ComputePipeline> Shared handle to the created compute pipeline.
         */
        virtual Ref<ComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) = 0;

        // ---- Render pass & framebuffer ----

        /**
         * @brief Create a render pass describing attachments and load/store operations.
         * @param desc Render pass descriptor (see RenderPass.hpp).
         * @return Ref<RenderPass> Shared handle to the created render pass.
         */
        virtual Ref<RenderPass> CreateRenderPass(const RenderPassDesc& desc) = 0;

        /**
         * @brief Create a framebuffer (collection of render-target textures).
         *
         * Framebuffers enable multi-pass rendering, offscreen / headless
         * rendering, and render-to-texture effects.
         *
         * @param desc Framebuffer descriptor (see Framebuffer.hpp).
         * @return Ref<Framebuffer> Shared handle to the created framebuffer.
         */
        virtual Ref<Framebuffer> CreateFramebuffer(const FramebufferDesc& desc) = 0;

        // ---- Command recording ----

        /**
         * @brief Create a command buffer for recording GPU work.
         * @return Ref<CommandBuffer> Shared handle to the created command buffer.
         */
        virtual Ref<CommandBuffer> CreateCommandBuffer() = 0;

        // ---- Presentation ----

        /**
         * @brief Create a swapchain for presenting to a window surface.
         *
         * Pass SwapchainDesc with Headless=true for off-screen rendering.
         *
         * @param desc Swapchain descriptor (see Swapchain.hpp).
         * @return Ref<Swapchain> Shared handle to the created swapchain.
         */
        virtual Ref<Swapchain> CreateSwapchain(const SwapchainDesc& desc) = 0;

        // ---- Descriptors ----

        /**
         * @brief Create a descriptor set layout describing resource bindings.
         * @param desc Layout descriptor (see DescriptorSet.hpp).
         * @return Ref<DescriptorSetLayout> Shared handle to the layout.
         */
        virtual Ref<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) = 0;

        /**
         * @brief Allocate a descriptor set from a given layout.
         * @param layout The layout that the descriptor set should match.
         * @return Ref<DescriptorSet> Shared handle to the allocated set.
         */
        virtual Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) = 0;

        // ---- Synchronization ----

        /**
         * @brief Block on the CPU until all submitted GPU work has completed.
         */
        virtual void WaitIdle() = 0;
    };

} // namespace Echelon
