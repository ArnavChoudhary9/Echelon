#include "RayRenderer.hpp"

#include "Echelon/Core/Log.hpp"
#include "Echelon/GraphicsAPI/Buffer.hpp"
#include "Echelon/GraphicsAPI/Pipeline.hpp"
#include "Echelon/GraphicsAPI/Shader.hpp"
#include "Echelon/GraphicsAPI/Swapchain.hpp"
#include "Echelon/GraphicsAPI/RenderPass.hpp"
#include "Echelon/GraphicsAPI/CommandBuffer.hpp"
#include "Echelon/GraphicsAPI/DescriptorSet.hpp"

#include "Echelon/Asset/AssetManager.hpp"
#include "Echelon/Asset/Mesh/StandardVertex.hpp"
#include "Echelon/Renderer/RendererLoader.hpp"   // ExecutableDir()

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/matrix_inverse.hpp"             // inverseTranspose

#include <cstring>

namespace Echelon {

    // ------------------------------------------------------------------
    // CPU mirrors of the shader constant system (Echelon.slang). Sizes must
    // match the std140 layout Slang reflects (Frame = 224B, Object = 128B).
    // ------------------------------------------------------------------
    struct FrameConstantsCPU {
        glm::mat4 View{ 1.0f };
        glm::mat4 Projection{ 1.0f };
        glm::mat4 ViewProjection{ 1.0f };
        glm::vec4 CameraPosition{ 0.0f };
        glm::vec4 TimeParams{ 0.0f };
    };
    struct ObjectConstantsCPU {
        glm::mat4 Model{ 1.0f };
        glm::mat4 NormalMatrix{ 1.0f };
    };

    // Find a reflected uniform buffer's binding by name. Returns false if absent.
    static bool FindUBOBinding(const ShaderReflection& refl, const char* name, uint32_t& outBinding) {
        for (const auto& ub : refl.UniformBuffers) {
            if (ub.Name == name) { outBinding = ub.Binding; return true; }
        }
        return false;
    }

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

        m_GraphicsAPI = GraphicsAPI::Create(GraphicsAPI::GetDefaultBackend());
        if (!m_GraphicsAPI) {
            ECHELON_LOG_ERROR("Ray: Failed to create GraphicsAPI");
            return false;
        }

        if (!m_GraphicsAPI->InitLoader()) {
            ECHELON_LOG_ERROR("Ray: Failed to initialise graphics loader");
            return false;
        }

        m_Device = m_GraphicsAPI->CreateDevice();
        if (!m_Device) {
            ECHELON_LOG_ERROR("Ray: Failed to create device");
            return false;
        }

        m_CommandBuffer = m_Device->CreateCommandBuffer();

        // Default render pass (render to default framebuffer)
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

        SwapchainDesc swapDesc;
        swapDesc.Width        = width;
        swapDesc.Height       = height;
        swapDesc.NativeWindow = windowHandle;
        swapDesc.VSync        = true;
        
        m_Swapchain = m_Device->CreateSwapchain(swapDesc);

        CreateDefaultResources();

        m_Initialized = true;
        ECHELON_LOG_INFO("Ray PBR Renderer initialised ({}x{})", width, height);
        return true;
    }

    void RayRenderer::Shutdown() {
        // Idempotent (see header note).
        if (!m_Initialized)
            return;

        m_SystemSets.clear();
        m_SystemLayout      = nullptr;
        m_FrameUBO          = nullptr;
        m_ObjectUBO         = nullptr;
        m_FlatPipeline      = nullptr;
        m_FlatShaderAsset   = nullptr;
        m_ErrorPipeline     = nullptr;
        m_ErrorShaderAsset  = nullptr;
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

    Ref<ShaderAsset> RayRenderer::LoadShaderAsset(const std::string& name) {
        // Engine/renderer shaders ship next to the executable in Shaders/.
        const fs::path path = RendererLoader::ExecutableDir() / "Shaders" / name;

        auto& assets = AssetManager::Get();
        UUID handle  = assets.GetHandle(path.string());
        if (handle.IsNull()) {
            ECHELON_LOG_ERROR("Ray: could not resolve shader '{}'", path.string());
            return nullptr;
        }

        auto shader = assets.GetAssetAs<ShaderAsset>(handle);
        if (!shader) {
            ECHELON_LOG_ERROR("Ray: '{}' is not a ShaderAsset", path.string());
            return nullptr;
        }

        // The renderer is not "active" during its own Init (RendererService assigns
        // m_Active only after Init returns), so AssetManager cannot auto-upload. Do
        // it here — this renderer *is* the RendererAPI the asset uploads against.
        shader->UploadGPU(this);
        return shader;
    }

    void RayRenderer::CreateDefaultResources() {
        // Per-draw / per-frame system UBOs (the fixed shader ABI).
        BufferDesc frameDesc;
        frameDesc.Size      = sizeof(FrameConstantsCPU);
        frameDesc.Usage     = BufferUsage::UniformBuffer;
        frameDesc.Memory    = MemoryUsage::CPUToGPU;
        frameDesc.DebugName = "Ray_FrameUBO";
        m_FrameUBO = m_Device->CreateBuffer(frameDesc);

        BufferDesc objDesc;
        objDesc.Size      = sizeof(ObjectConstantsCPU);
        objDesc.Usage     = BufferUsage::UniformBuffer;
        objDesc.Memory    = MemoryUsage::CPUToGPU;
        objDesc.DebugName = "Ray_ObjectUBO";
        m_ObjectUBO = m_Device->CreateBuffer(objDesc);

        // A generic 2-binding layout (GL ignores layout at bind time; buffers are
        // assigned to the per-shader reflected bindings in BindSystemConstants).
        DescriptorSetLayoutDesc slDesc;
        slDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer, 1, ShaderStage::Vertex },
            { 1, DescriptorType::UniformBuffer, 1, ShaderStage::Vertex },
        };
        slDesc.DebugName = "Ray_SystemLayout";
        m_SystemLayout = m_Device->CreateDescriptorSetLayout(slDesc);

        m_FlatShaderAsset  = LoadShaderAsset("Flat.slang");
        m_ErrorShaderAsset = LoadShaderAsset("Error.slang");
        BuildDefaultPipeline();

        m_LastAssetEpoch = AssetManager::Get().GetEpoch();
    }

    // Build one reflection-driven pipeline from a shader asset.
    static Ref<Pipeline> BuildPipeline(const Ref<Device>& device, const Ref<RenderPass>& pass,
                                       const Ref<ShaderAsset>& shader, const char* name) {
        if (!shader || !shader->GetGpuShader()) return nullptr;
        PipelineDesc pd;
        pd.ShaderProgram = shader->GetGpuShader();
        pd.Topology      = PrimitiveTopology::TriangleList;
        pd.Pass          = pass;
        pd.DebugName     = name;
        // Vertex layout comes entirely from reflection — no hand-written attributes.
        pd.Layout = StandardVertex::FromReflection(shader->GetReflection());
        pd.Depth.DepthTestEnable  = true;
        pd.Depth.DepthWriteEnable = true;
        pd.Raster.Cull            = CullMode::None;
        return device->CreatePipeline(pd);
    }

    void RayRenderer::BuildDefaultPipeline() {
        m_FlatPipeline  = BuildPipeline(m_Device, m_DefaultRenderPass, m_FlatShaderAsset,  "Ray_FlatPipeline");
        m_ErrorPipeline = BuildPipeline(m_Device, m_DefaultRenderPass, m_ErrorShaderAsset, "Ray_ErrorPipeline");
        if (!m_FlatPipeline)  ECHELON_LOG_ERROR("Ray: no flat shader — default pipeline not built");
        if (!m_ErrorPipeline) ECHELON_LOG_ERROR("Ray: no error shader — pink fallback unavailable");
    }

    void RayRenderer::EnsureUpToDate() {
        const uint64_t epoch = AssetManager::Get().GetEpoch();
        if (epoch == m_LastAssetEpoch)
            return;

        // A shader/material hot-reload (or renderer swap) rebuilt GPU programs; the
        // pipeline holds the *old* GL program, so rebuild it and drop stale sets.
        m_SystemSets.clear();
        if (m_FlatShaderAsset)  m_FlatShaderAsset->UploadGPU(this);   // rebuild GL program if released
        if (m_ErrorShaderAsset) m_ErrorShaderAsset->UploadGPU(this);
        BuildDefaultPipeline();
        m_LastAssetEpoch = epoch;
    }

    // ------------------------------------------------------------------
    // System constant binding (resolve g_Frame / g_Object by name per shader)
    // ------------------------------------------------------------------

    void RayRenderer::BindSystemConstants(const Ref<Pipeline>& pipeline) {
        if (!pipeline) return;
        Ref<Shader> shader = pipeline->GetShader();
        if (!shader) return;

        auto it = m_SystemSets.find(shader.get());
        if (it == m_SystemSets.end()) {
            const ShaderReflection& refl = shader->GetReflection();
            auto set = m_Device->AllocateDescriptorSet(m_SystemLayout);

            uint32_t binding = 0;
            if (FindUBOBinding(refl, "g_Frame", binding))  set->SetBuffer(binding, m_FrameUBO);
            if (FindUBOBinding(refl, "g_Object", binding)) set->SetBuffer(binding, m_ObjectUBO);
            set->Update();

            it = m_SystemSets.emplace(shader.get(), set).first;
        }
        m_CommandBuffer->BindDescriptorSet(it->second, 0);
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

        // Upload per-frame constants once.
        FrameConstantsCPU fc;
        fc.View           = m_ViewMatrix;
        fc.Projection     = m_ProjectionMatrix;
        fc.ViewProjection = m_ProjectionMatrix * m_ViewMatrix;
        fc.CameraPosition = glm::inverse(m_ViewMatrix)[3];
        fc.TimeParams     = glm::vec4(0.0f);
        if (m_FrameUBO) m_FrameUBO->SetData(&fc, sizeof(fc));

        m_GraphicsAPI->BeginFrame();
        m_CommandBuffer->Begin();

        Viewport vp;
        vp.X      = 0.0f;
        vp.Y      = 0.0f;
        vp.Width  = static_cast<float>(m_ViewportWidth);
        vp.Height = static_cast<float>(m_ViewportHeight);
        m_CommandBuffer->SetViewport(vp);

        m_CommandBuffer->BeginRenderPass(m_DefaultRenderPass, nullptr);
    }

    void RayRenderer::EndFrame() {
        m_CommandBuffer->EndRenderPass();
        m_CommandBuffer->End();

        m_GraphicsAPI->Submit(m_CommandBuffer);
        m_GraphicsAPI->EndFrame();
        // Presentation is handled by the Application loop via Window::SwapBuffers().
    }

    // ------------------------------------------------------------------
    // Scene scope
    // ------------------------------------------------------------------

    void RayRenderer::BeginScene(const Ref<Scene>& /*scene*/) {}
    void RayRenderer::EndScene() {}

    // ------------------------------------------------------------------
    // Scene-driven rendering via RenderGraph
    // ------------------------------------------------------------------

    void RayRenderer::RenderScene(const Ref<Scene>& scene) {
        if (!scene) return;

        EnsureUpToDate();

        // The graph's fallback pipeline is the pink error material: entities whose
        // material fails to resolve render magenta so the problem is obvious. Normal
        // meshes get the engine default (Flat) material via the RenderGraph.
        m_RenderGraph.Update(scene, ErrorPipeline());

        for (const auto& group : m_RenderGraph.GetPipelineGroups()) {
            const auto& pipeline = group.PipelineRef ? group.PipelineRef : ErrorPipeline();
            if (!pipeline) continue;

            m_CommandBuffer->BindPipeline(pipeline);
            BindSystemConstants(pipeline);   // g_Frame + g_Object at this shader's bindings
            const auto& shader = pipeline->GetShader();

            for (const auto& batch : group.Batches) {
                for (size_t i = 0; i < batch.Transforms.size(); ++i) {
                    // Per-entity material parameters/textures (null for the default
                    // pipeline). Bound before the draw; its bindings never collide
                    // with the system set within a shader (Slang assigns unique ones).
                    if (i < batch.MaterialSets.size() && batch.MaterialSets[i])
                        m_CommandBuffer->BindDescriptorSet(batch.MaterialSets[i], 1);

                    const auto& transform = batch.Transforms[i];
                    if (batch.IndexBuffer && batch.IndexCount > 0) {
                        DrawIndexed(batch.VertexBuffer, batch.IndexBuffer, shader,
                                    transform, batch.IndexCount);
                    } else {
                        Draw(batch.VertexBuffer, shader, transform, batch.VertexCount);
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // Draw commands — each draw rewrites g_Object, so every index gets its own
    // transform with the SAME bound pipeline (no pipeline change per draw).
    // ------------------------------------------------------------------

    void RayRenderer::DrawIndexed(const Ref<Buffer>& vertexBuffer,
                                   const Ref<Buffer>& indexBuffer,
                                   const Ref<Shader>& /*shader*/,
                                   const glm::mat4& transform,
                                   uint32_t indexCount) {
        m_Stats.DrawCalls++;

        ObjectConstantsCPU oc;
        oc.Model        = transform;
        oc.NormalMatrix = glm::mat4(glm::inverseTranspose(glm::mat3(transform)));
        if (m_ObjectUBO) m_ObjectUBO->SetData(&oc, sizeof(oc));

        m_CommandBuffer->BindVertexBuffer(vertexBuffer);
        m_CommandBuffer->BindIndexBuffer(indexBuffer);

        if (indexCount == 0 && indexBuffer) {
            indexCount = static_cast<uint32_t>(indexBuffer->GetSize() / sizeof(uint32_t));
        }

        m_CommandBuffer->DrawIndexed(indexCount);
    }

    void RayRenderer::Draw(const Ref<Buffer>& vertexBuffer,
                           const Ref<Shader>& /*shader*/,
                           const glm::mat4& transform,
                           uint32_t vertexCount) {
        m_Stats.DrawCalls++;

        ObjectConstantsCPU oc;
        oc.Model        = transform;
        oc.NormalMatrix = glm::mat4(glm::inverseTranspose(glm::mat3(transform)));
        if (m_ObjectUBO) m_ObjectUBO->SetData(&oc, sizeof(oc));

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
