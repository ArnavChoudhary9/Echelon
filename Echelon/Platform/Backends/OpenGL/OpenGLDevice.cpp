#include "OpenGLDevice.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLShader.hpp"
#include "OpenGLPipeline.hpp"
#include "OpenGLRenderPass.hpp"
#include "OpenGLFramebuffer.hpp"
#include "OpenGLCommandBuffer.hpp"
#include "OpenGLSwapchain.hpp"
#include "OpenGLDescriptorSet.hpp"

#include <glad/gl.h>

namespace Echelon {

    Ref<Buffer> OpenGLDevice::CreateBuffer(const BufferDesc& desc)
    {
        return CreateRef<OpenGLBuffer>(desc);
    }

    Ref<Texture> OpenGLDevice::CreateTexture(const TextureDesc& desc)
    {
        return CreateRef<OpenGLTexture>(desc);
    }

    Ref<Shader> OpenGLDevice::CreateShader(const ShaderDesc& desc)
    {
        return CreateRef<OpenGLShader>(desc);
    }

    Ref<Pipeline> OpenGLDevice::CreatePipeline(const PipelineDesc& desc)
    {
        return CreateRef<OpenGLPipeline>(desc);
    }

    Ref<ComputePipeline> OpenGLDevice::CreateComputePipeline(const ComputePipelineDesc& desc)
    {
        return CreateRef<OpenGLComputePipeline>(desc);
    }

    Ref<RenderPass> OpenGLDevice::CreateRenderPass(const RenderPassDesc& desc)
    {
        return CreateRef<OpenGLRenderPass>(desc);
    }

    Ref<Framebuffer> OpenGLDevice::CreateFramebuffer(const FramebufferDesc& desc)
    {
        return CreateRef<OpenGLFramebuffer>(desc);
    }

    Ref<CommandBuffer> OpenGLDevice::CreateCommandBuffer()
    {
        return CreateRef<OpenGLCommandBuffer>();
    }

    Ref<Swapchain> OpenGLDevice::CreateSwapchain(const SwapchainDesc& desc)
    {
        return CreateRef<OpenGLSwapchain>(desc);
    }

    Ref<DescriptorSetLayout> OpenGLDevice::CreateDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
    {
        return CreateRef<OpenGLDescriptorSetLayout>(desc);
    }

    Ref<DescriptorSet> OpenGLDevice::AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout)
    {
        auto glLayout = std::static_pointer_cast<OpenGLDescriptorSetLayout>(layout);
        return CreateRef<OpenGLDescriptorSet>(glLayout);
    }

    void OpenGLDevice::WaitIdle()
    {
        glFinish();
    }

} // namespace Echelon
