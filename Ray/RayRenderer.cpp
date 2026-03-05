#include "RayRenderer.hpp"
#include "RayShaders.hpp"

#include "Echelon/Core/Log.hpp"
#include "Echelon/GraphicsAPI/Buffer.hpp"
#include "Echelon/GraphicsAPI/Pipeline.hpp"
#include "Echelon/GraphicsAPI/Shader.hpp"
#include "Echelon/GraphicsAPI/Swapchain.hpp"
#include "Echelon/GraphicsAPI/RenderPass.hpp"
#include "Echelon/GraphicsAPI/CommandBuffer.hpp"

#include <cstring>

namespace Echelon {

    // ------------------------------------------------------------------
    // Construction / destruction
    // ------------------------------------------------------------------

    RayRenderer::RayRenderer() = default;

    RayRenderer::~RayRenderer() {
        if (m_Initialized) {
            Shutdown();
        }
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    bool RayRenderer::Init(void* windowHandle, uint32_t width, uint32_t height) {
        m_ViewportWidth  = width;
        m_ViewportHeight = height;
        m_Stats          = {};

        // Create the graphics API using the build-configured default backend
        m_GraphicsAPI = GraphicsAPI::Create(GraphicsAPI::GetDefaultBackend());
        if (!m_GraphicsAPI) {
            ECHELON_LOG_ERROR("Ray: Failed to create GraphicsAPI");
            return false;
        }

        if (!m_GraphicsAPI->InitLoader()) {
            ECHELON_LOG_ERROR("Ray: Failed to initialise graphics loader");
            return false;
        }

        // Create the device
        m_Device = m_GraphicsAPI->CreateDevice();
        if (!m_Device) {
            ECHELON_LOG_ERROR("Ray: Failed to create device");
            return false;
        }

        // Create command buffer
        m_CommandBuffer = m_Device->CreateCommandBuffer();

        // Create default render pass (render to default framebuffer)
        RenderPassDesc rpDesc;
        ColorAttachmentDesc colorAtt;
        colorAtt.Format = TextureFormat::RGBA8_UNORM;
        colorAtt.Load   = LoadOp::Clear;
        colorAtt.Store  = StoreOp::Store;
        colorAtt.Clear  = { 0.1f, 0.1f, 0.12f, 1.0f };
        rpDesc.ColorAttachments.push_back(colorAtt);

        DepthAttachmentDesc depthAtt;
        depthAtt.Format = TextureFormat::D32_FLOAT;
        depthAtt.Load   = LoadOp::Clear;
        depthAtt.Store  = StoreOp::DontCare;
        rpDesc.DepthAttachment    = depthAtt;
        rpDesc.HasDepthAttachment = true;
        rpDesc.DebugName = "Ray_DefaultPass";
        m_DefaultRenderPass = m_Device->CreateRenderPass(rpDesc);

        // Create swapchain
        SwapchainDesc swapDesc;
        swapDesc.Width        = width;
        swapDesc.Height       = height;
        swapDesc.NativeWindow = windowHandle;
        swapDesc.VSync        = true;
        m_Swapchain = m_Device->CreateSwapchain(swapDesc);

        // Create default shaders and pipeline
        CreateDefaultResources();

        m_Initialized = true;
        ECHELON_LOG_INFO("Ray PBR Renderer initialised ({}x{})", width, height);
        return true;
    }

    void RayRenderer::Shutdown() {
        m_FlatPipeline      = nullptr;
        m_FlatShader        = nullptr;
        m_BasicShader       = nullptr;
        m_Swapchain         = nullptr;
        m_DefaultRenderPass = nullptr;
        m_CommandBuffer     = nullptr;
        m_Device            = nullptr;
        m_GraphicsAPI       = nullptr;
        m_Initialized       = false;

        ECHELON_LOG_INFO("Ray PBR Renderer shut down");
    }

    // ------------------------------------------------------------------
    // Resource creation
    // ------------------------------------------------------------------

    static Ref<Shader> CompileGLSL(const Ref<Device>& device,
                                    const std::string& vertSrc, const std::string& fragSrc,
                                    const std::string& name)
    {
        ShaderDesc desc;
        desc.DebugName = name;

        ShaderStageDesc vertStage;
        vertStage.Stage  = ShaderStage::Vertex;
        vertStage.Format = ShaderSourceFormat::GLSL;
        vertStage.Source.assign(reinterpret_cast<const uint8_t*>(vertSrc.data()),
                                reinterpret_cast<const uint8_t*>(vertSrc.data()) + vertSrc.size());
        desc.Stages.push_back(vertStage);

        ShaderStageDesc fragStage;
        fragStage.Stage  = ShaderStage::Fragment;
        fragStage.Format = ShaderSourceFormat::GLSL;
        fragStage.Source.assign(reinterpret_cast<const uint8_t*>(fragSrc.data()),
                                reinterpret_cast<const uint8_t*>(fragSrc.data()) + fragSrc.size());
        desc.Stages.push_back(fragStage);

        return device->CreateShader(desc);
    }

    void RayRenderer::CreateDefaultResources()
    {
        // Compile the basic PBR shader (file-based with fallback)
        m_BasicShader = CompileGLSL(m_Device,
                                     RayShaders::GetBasicVertexShader(),
                                     RayShaders::GetBasicFragmentShader(),
                                     "Ray_BasicShader");

        // Compile the flat colour shader (file-based with fallback)
        m_FlatShader = CompileGLSL(m_Device,
                                    RayShaders::GetFlatVertexShader(),
                                    RayShaders::GetFlatFragmentShader(),
                                    "Ray_FlatShader");

        // Create a pipeline for the flat shader (position + color)
        PipelineDesc pipeDesc;
        pipeDesc.ShaderProgram = m_FlatShader;
        pipeDesc.Topology      = PrimitiveTopology::TriangleList;
        pipeDesc.Pass          = m_DefaultRenderPass;
        pipeDesc.DebugName     = "Ray_FlatPipeline";

        // Vertex layout: vec3 position + vec3 color
        VertexBinding vb;
        vb.Binding   = 0;
        vb.Stride    = sizeof(float) * 6;  // 3 pos + 3 color
        vb.InputRate = VertexInputRate::PerVertex;

        VertexAttribute posAttr;
        posAttr.Name    = "a_Position";
        posAttr.Format  = VertexAttributeFormat::Float3;
        posAttr.Offset  = 0;
        posAttr.Binding = 0;

        VertexAttribute colorAttr;
        colorAttr.Name    = "a_Color";
        colorAttr.Format  = VertexAttributeFormat::Float3;
        colorAttr.Offset  = sizeof(float) * 3;
        colorAttr.Binding = 0;

        pipeDesc.Layout.Bindings   = { vb };
        pipeDesc.Layout.Attributes = { posAttr, colorAttr };

        // Depth test enabled, back-face culling disabled for demo
        pipeDesc.Depth.DepthTestEnable  = true;
        pipeDesc.Depth.DepthWriteEnable = true;
        pipeDesc.Raster.Cull            = CullMode::None;

        m_FlatPipeline = m_Device->CreatePipeline(pipeDesc);
    }

    // ------------------------------------------------------------------
    // Frame lifecycle
    // ------------------------------------------------------------------

    void RayRenderer::BeginFrame(const glm::mat4& viewMatrix,
                                  const glm::mat4& projectionMatrix,
                                  const ClearValue& clearValue) {
        m_ViewMatrix       = viewMatrix;
        m_ProjectionMatrix = projectionMatrix;
        m_Stats            = {};
        (void)clearValue; // Clear colour is configured on the render pass

        m_GraphicsAPI->BeginFrame();
        m_CommandBuffer->Begin();

        // Set viewport for this frame
        Viewport vp;
        vp.X      = 0.0f;
        vp.Y      = 0.0f;
        vp.Width  = static_cast<float>(m_ViewportWidth);
        vp.Height = static_cast<float>(m_ViewportHeight);
        m_CommandBuffer->SetViewport(vp);

        // Render to default framebuffer — clear ops are driven by the render pass
        m_CommandBuffer->BeginRenderPass(m_DefaultRenderPass, nullptr);
    }

    void RayRenderer::EndFrame() {
        m_CommandBuffer->EndRenderPass();
        m_CommandBuffer->End();

        m_GraphicsAPI->Submit(m_CommandBuffer);
        m_GraphicsAPI->EndFrame();

        // Note: presentation (buffer swap) is handled by the Application
        // loop via Window::SwapBuffers().  We do NOT call Swapchain::Present()
        // here to avoid double-swapping.
    }

    // ------------------------------------------------------------------
    // Scene scope
    // ------------------------------------------------------------------

    void RayRenderer::BeginScene(const Ref<Scene>& /*scene*/) {
        // Upload scene-global data (lights, environment) — placeholder
    }

    void RayRenderer::EndScene() {
        // Post-processing passes — placeholder
    }

    // ------------------------------------------------------------------
    // Scene-driven rendering via RenderGraph
    // ------------------------------------------------------------------

    void RayRenderer::RenderScene(const Ref<Scene>& scene) {
        if (!scene) return;

        // Update the render graph — early-outs if nothing changed
        m_RenderGraph.Update(scene, m_FlatPipeline);

        // Issue draw calls from pipeline-sorted groups.
        // Each PipelineGroup shares a pipeline — bind once, draw all batches.
        for (const auto& group : m_RenderGraph.GetPipelineGroups()) {
            const auto& pipeline = group.PipelineRef ? group.PipelineRef : m_FlatPipeline;

            for (const auto& batch : group.Batches) {
                for (size_t i = 0; i < batch.Transforms.size(); ++i) {
                    const auto& transform = batch.Transforms[i];

                    if (batch.IndexBuffer && batch.IndexCount > 0) {
                        DrawIndexed(batch.VertexBuffer,
                                    batch.IndexBuffer,
                                    pipeline,
                                    transform,
                                    batch.IndexCount);
                    } else {
                        Draw(batch.VertexBuffer,
                             pipeline,
                             transform,
                             batch.VertexCount);
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Draw commands
    // ------------------------------------------------------------------

    void RayRenderer::DrawIndexed(const Ref<Buffer>& vertexBuffer,
                                   const Ref<Buffer>& indexBuffer,
                                   const Ref<Pipeline>& pipeline,
                                   const glm::mat4& transform,
                                   uint32_t indexCount) {
        m_Stats.DrawCalls++;

        // Bind pipeline
        m_CommandBuffer->BindPipeline(pipeline);

        // Set uniforms via the abstract Shader interface
        auto shader = pipeline->GetShader();
        if (shader) {
            shader->SetMat4("u_Model",      &transform[0][0]);
            shader->SetMat4("u_View",       &m_ViewMatrix[0][0]);
            shader->SetMat4("u_Projection", &m_ProjectionMatrix[0][0]);
        }

        // Bind buffers and draw
        m_CommandBuffer->BindVertexBuffer(vertexBuffer);
        m_CommandBuffer->BindIndexBuffer(indexBuffer);

        if (indexCount == 0 && indexBuffer) {
            indexCount = static_cast<uint32_t>(indexBuffer->GetSize() / sizeof(uint32_t));
        }

        m_CommandBuffer->DrawIndexed(indexCount);
    }

    void RayRenderer::Draw(const Ref<Buffer>& vertexBuffer,
                            const Ref<Pipeline>& pipeline,
                            const glm::mat4& transform,
                            uint32_t vertexCount) {
        m_Stats.DrawCalls++;

        // Bind pipeline
        m_CommandBuffer->BindPipeline(pipeline);

        // Set uniforms via the abstract Shader interface
        auto shader = pipeline->GetShader();
        if (shader) {
            shader->SetMat4("u_Model",      &transform[0][0]);
            shader->SetMat4("u_View",       &m_ViewMatrix[0][0]);
            shader->SetMat4("u_Projection", &m_ProjectionMatrix[0][0]);
        }

        m_CommandBuffer->BindVertexBuffer(vertexBuffer);
        m_CommandBuffer->Draw(vertexCount);
    }

    // ------------------------------------------------------------------
    // Viewport
    // ------------------------------------------------------------------

    void RayRenderer::OnResize(uint32_t width, uint32_t height) {
        m_ViewportWidth  = width;
        m_ViewportHeight = height;

        if (m_Swapchain)
            m_Swapchain->Resize(width, height);
    }

    // ------------------------------------------------------------------
    // VSync
    // ------------------------------------------------------------------

    void RayRenderer::SetVSync(bool enabled) {
        if (m_Swapchain)
            m_Swapchain->SetVSync(enabled);
    }

    bool RayRenderer::IsVSync() const {
        return m_Swapchain ? m_Swapchain->IsVSync() : true;
    }

    // ------------------------------------------------------------------
    // Queries
    // ------------------------------------------------------------------

    RendererInfo RayRenderer::GetInfo() const {
        return RendererInfo{
            .Name    = "Ray PBR Renderer",
            .Version = "0.1.0",
            .Author  = "Echelon"
        };
    }

    RenderStats RayRenderer::GetStats() const {
        return m_Stats;
    }

} // namespace Echelon
