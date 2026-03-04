#pragma once

/**
 * @file OpenGLDevice.hpp
 * @brief OpenGL implementation of the Device interface.
 *
 * Central factory that creates all OpenGL-backed GPU resources.
 */

#include "Echelon/GraphicsAPI/Device.hpp"

namespace Echelon {

    class OpenGLDevice : public Device {
    public:
        OpenGLDevice() = default;
        ~OpenGLDevice() override = default;

        // ---- Resource creation ----
        Ref<Buffer>              CreateBuffer(const BufferDesc& desc) override;
        Ref<Texture>             CreateTexture(const TextureDesc& desc) override;
        Ref<Shader>              CreateShader(const ShaderDesc& desc) override;

        // ---- Pipeline creation ----
        Ref<Pipeline>            CreatePipeline(const PipelineDesc& desc) override;
        Ref<ComputePipeline>     CreateComputePipeline(const ComputePipelineDesc& desc) override;

        // ---- Render pass & framebuffer ----
        Ref<RenderPass>          CreateRenderPass(const RenderPassDesc& desc) override;
        Ref<Framebuffer>         CreateFramebuffer(const FramebufferDesc& desc) override;

        // ---- Command recording ----
        Ref<CommandBuffer>       CreateCommandBuffer() override;

        // ---- Presentation ----
        Ref<Swapchain>           CreateSwapchain(const SwapchainDesc& desc) override;

        // ---- Descriptors ----
        Ref<DescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;
        Ref<DescriptorSet>       AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;

        // ---- Synchronization ----
        void WaitIdle() override;
    };

} // namespace Echelon
