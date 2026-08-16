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
#include "Echelon/Asset/RenderPipeline/RenderPipelineAsset.hpp"
#include "Echelon/Project/Project.hpp"
#include "Echelon/Renderer/RendererLoader.hpp"   // ExecutableDir()
#include "Echelon/Renderer/RendererConstants.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/matrix_inverse.hpp"             // inverseTranspose

#include <algorithm>
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
    // Default pass graph — a single "forward" pass that clears + draws the
    // scene straight to the backbuffer. Reproduces the pre-passgraph behaviour
    // exactly; a project may override this with a RenderPipelineAsset.
    // ------------------------------------------------------------------

    static RenderPipelineDesc DefaultForwardDesc() {
        RenderPipelineDesc d;

        ResourceDesc depth;
        depth.Name       = "depth";
        depth.Format     = TextureFormat::D32_FLOAT;
        depth.SizePolicy = ResourceSizePolicy::SwapchainRelative;
        depth.Scale      = 1.0f;
        d.Resources.push_back(depth);

        PassDesc forward;
        forward.Name = "forward";
        forward.Type = PassType::Graphics;

        AttachmentRef color;
        color.Resource   = kBackbufferResource;
        color.Load       = LoadOp::Clear;
        color.Store      = StoreOp::Store;
        color.ColorClear = ClearColor{ 0.1f, 0.1f, 0.12f, 1.0f };
        forward.ColorOutputs.push_back(color);

        AttachmentRef dep;
        dep.Resource = "depth";
        dep.Load     = LoadOp::Clear;
        dep.Store    = StoreOp::DontCare;
        forward.DepthOutput = dep;

        d.Passes.push_back(forward);
        return d;
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

        SwapchainDesc swapDesc;
        swapDesc.Width        = width;
        swapDesc.Height       = height;
        swapDesc.NativeWindow = windowHandle;
        swapDesc.VSync        = true;

        m_Swapchain = m_Device->CreateSwapchain(swapDesc);

        // Per-pass-type default handlers (persist across pass-graph recompiles):
        //  - Graphics passes record the scene draw list.
        //  - Fullscreen passes run a post-process (bind shader + inputs, draw a triangle).
        m_PassGraph.SetDefaultCallback(PassType::Graphics,
            [this](CommandBuffer&, const PassContext&) { ExecuteDrawList(); });
        m_PassGraph.SetDefaultCallback(PassType::Fullscreen,
            [this](CommandBuffer& cmd, const PassContext& ctx) { ExecuteFullscreenPass(cmd, ctx); });
        m_PassGraph.SetDefaultCallback(PassType::Compute,
            [this](CommandBuffer& cmd, const PassContext& ctx) { ExecuteComputePass(cmd, ctx); });

        // Compile the pass graph: prefer the project's `.ehpipeline` asset, else the
        // built-in single-pass default. (The project may not be active yet during
        // Init — EnsureUpToDate() resolves the asset lazily on the first frame.)
        if (!TryLoadPipelineAsset()) {
            if (!m_PassGraph.CompileFrom(DefaultForwardDesc(), m_Device, width, height)) {
                ECHELON_LOG_ERROR("Ray: failed to compile default pass graph");
                return false;
            }
        }
        // If a project was active we made the definitive resolution attempt; only
        // retry lazily (once) when the project becomes active after Init.
        if (Project::GetActive())
            m_PipelineResolved = true;

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
        m_FullscreenSets.clear();
        m_FullscreenPipelines.clear();
        m_ComputePipelines.clear();
        m_FullscreenShaders.clear();
        m_FullscreenLayout  = nullptr;
        m_LinearSampler     = nullptr;
        m_PipelineAsset     = nullptr;
        m_SystemLayout      = nullptr;
        m_FrameUBO          = nullptr;
        m_ObjectUBO         = nullptr;
        m_FlatPipeline      = nullptr;
        m_FlatShaderAsset   = nullptr;
        m_ErrorPipeline     = nullptr;
        m_ErrorShaderAsset  = nullptr;
        m_Swapchain         = nullptr;
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

        // Linear clamp sampler + a generic 1-sampler layout for fullscreen/post passes.
        // (GL ignores the layout contents; textures bind to reflected sampler bindings.)
        SamplerDesc postSampler;
        postSampler.MinFilter    = FilterMode::Linear;
        postSampler.MagFilter    = FilterMode::Linear;
        postSampler.MipMapFilter = FilterMode::Linear;
        postSampler.AddressU     = AddressMode::ClampToEdge;
        postSampler.AddressV     = AddressMode::ClampToEdge;
        postSampler.AddressW     = AddressMode::ClampToEdge;
        m_LinearSampler = m_Device->CreateSampler(postSampler);

        DescriptorSetLayoutDesc fsLayout;
        fsLayout.Bindings  = { { 0, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment } };
        fsLayout.DebugName = "Ray_FullscreenLayout";
        m_FullscreenLayout = m_Device->CreateDescriptorSetLayout(fsLayout);

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
        const Ref<RenderPass> scenePass = GetScenePass();
        m_FlatPipeline  = BuildPipeline(m_Device, scenePass, m_FlatShaderAsset,  "Ray_FlatPipeline");
        m_ErrorPipeline = BuildPipeline(m_Device, scenePass, m_ErrorShaderAsset, "Ray_ErrorPipeline");
        if (!m_FlatPipeline)  ECHELON_LOG_ERROR("Ray: no flat shader — default pipeline not built");
        if (!m_ErrorPipeline) ECHELON_LOG_ERROR("Ray: no error shader — pink fallback unavailable");
    }

    void RayRenderer::EnsureUpToDate() {
        // Lazily resolve the project's render pipeline asset — the project may not
        // have been active during Init(). Once resolved, rebuild the scene pipelines
        // against the (possibly different) forward pass.
        if (!m_PipelineResolved && Project::GetActive()) {
            m_PipelineResolved = true;
            if (TryLoadPipelineAsset()) {
                BuildDefaultPipeline();
                m_LastAssetEpoch = AssetManager::Get().GetEpoch();
            }
        }

        const uint64_t epoch = AssetManager::Get().GetEpoch();
        if (epoch == m_LastAssetEpoch)
            return;

        // A shader/material/pipeline hot-reload (or renderer swap) invalidated GPU
        // programs / the pass graph. Recompile the graph (a hot-reloaded pipeline
        // asset has already updated its description in place), rebuild GL programs,
        // and drop stale system sets.
        m_SystemSets.clear();
        m_FullscreenPipelines.clear();                               // rebuilt lazily
        m_ComputePipelines.clear();
        for (auto& [name, sh] : m_FullscreenShaders)
            if (sh) sh->UploadGPU(this);                             // rebuild post GL programs if released
        if (m_PipelineAsset) RecompilePassGraph();
        if (m_FlatShaderAsset)  m_FlatShaderAsset->UploadGPU(this);   // rebuild GL program if released
        if (m_ErrorShaderAsset) m_ErrorShaderAsset->UploadGPU(this);
        BuildDefaultPipeline();
        m_LastAssetEpoch = epoch;
    }

    bool RayRenderer::TryLoadPipelineAsset() {
        if (!Project::GetActive()) return false;   // no project → keep the built-in default

        auto& assets = AssetManager::Get();
        const UUID handle = assets.GetHandle(m_PipelinePath);
        if (handle.IsNull()) return false;

        auto asset = assets.GetAssetAs<RenderPipelineAsset>(handle);
        if (!asset || !asset->IsValid()) return false;

        m_PipelineAsset = asset;
        ECHELON_LOG_INFO("Ray: using render pipeline asset '{}'", m_PipelinePath);
        return RecompilePassGraph();
    }

    bool RayRenderer::RecompilePassGraph() {
        // Fullscreen pipelines are built against per-pass RenderPass objects that a
        // recompile replaces, so drop them (rebuilt lazily against the new passes).
        m_FullscreenPipelines.clear();
        m_ComputePipelines.clear();

        if (m_PipelineAsset && m_PipelineAsset->IsValid()) {
            if (m_PassGraph.CompileFrom(m_PipelineAsset->GetDescription(), m_Device,
                                        m_ViewportWidth, m_ViewportHeight))
                return true;
            ECHELON_LOG_ERROR("Ray: pipeline asset failed to compile; using built-in default");
        }
        return m_PassGraph.CompileFrom(DefaultForwardDesc(), m_Device,
                                       m_ViewportWidth, m_ViewportHeight);
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

        // Render-pass scoping and per-pass viewports are handled by the pass graph
        // during RenderScene(). The command buffer is now open but no pass is bound.
    }

    void RayRenderer::EndFrame() {
        // Render passes are opened/closed by the pass graph in RenderScene().
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

        m_RenderGraph.Update(scene, GetDefaultPipeline(), GetErrorPipeline());

        // Execute the pass graph. Each pass opens its own render pass + framebuffer;
        // the "forward" pass calls back into ExecuteDrawList() to record the draws.
        m_PassGraph.Execute(m_CommandBuffer);
    }

    // ------------------------------------------------------------------
    // Forward scene pass — record the sorted draw list into the currently
    // bound render pass (invoked by the pass graph's "forward" callback).
    // ------------------------------------------------------------------

    void RayRenderer::ExecuteDrawList() {
        for (const auto& group : m_RenderGraph.GetPipelineGroups()) {
            const auto& pipeline = group.PipelineRef ? group.PipelineRef : GetErrorPipeline();
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
    // Fullscreen / post-process pass — sample the declared input attachments and
    // draw a single fullscreen triangle (no vertex buffer; the VS uses SV_VertexID).
    // ------------------------------------------------------------------

    Ref<Pipeline> RayRenderer::GetFullscreenPipeline(const std::string& passName,
                                                     const std::string& shaderName) {
        if (auto it = m_FullscreenPipelines.find(passName);
            it != m_FullscreenPipelines.end() && it->second)
            return it->second;

        if (shaderName.empty()) {
            ECHELON_LOG_ERROR("Ray: fullscreen pass '{}' declares no shader", passName);
            return nullptr;
        }

        // Load + cache the post shader asset (ships beside the executable in Shaders/).
        auto sit = m_FullscreenShaders.find(shaderName);
        Ref<ShaderAsset> shaderAsset = (sit != m_FullscreenShaders.end()) ? sit->second : nullptr;
        if (!shaderAsset) {
            shaderAsset = LoadShaderAsset(shaderName);
            if (shaderAsset) m_FullscreenShaders[shaderName] = shaderAsset;
        }
        if (!shaderAsset || !shaderAsset->GetGpuShader()) {
            ECHELON_LOG_ERROR("Ray: fullscreen shader '{}' failed to load", shaderName);
            return nullptr;
        }

        PipelineDesc pd;
        pd.ShaderProgram = shaderAsset->GetGpuShader();
        pd.Topology      = PrimitiveTopology::TriangleList;
        pd.Pass          = m_PassGraph.GetRenderPass(passName);
        // Fullscreen shaders declare no vertex inputs → empty layout (SV_VertexID drives the VS).
        pd.Layout        = StandardVertex::FromReflection(shaderAsset->GetReflection());
        pd.Depth.DepthTestEnable  = false;   // always overwrite the target
        pd.Depth.DepthWriteEnable = false;
        pd.Raster.Cull            = CullMode::None;
        pd.DebugName     = "Ray_Fullscreen_" + passName;

        auto pipe = m_Device->CreatePipeline(pd);
        m_FullscreenPipelines[passName] = pipe;
        return pipe;
    }

    void RayRenderer::ExecuteFullscreenPass(CommandBuffer& cmd, const PassContext& ctx) {
        const PassDesc& pass = *ctx.Pass;

        Ref<Pipeline> pipe = GetFullscreenPipeline(pass.Name, pass.Shader);
        if (!pipe) return;
        cmd.BindPipeline(pipe);

        // Bind each resolved input attachment at the shader's reflected sampler binding.
        if (Ref<Shader> shader = pipe->GetShader(); shader && !ctx.InputTextures.empty()) {
            const ShaderReflection& refl = shader->GetReflection();
            if (!refl.Samplers.empty()) {
                Ref<DescriptorSet>& set = m_FullscreenSets[pass.Name];
                if (!set) set = m_Device->AllocateDescriptorSet(m_FullscreenLayout);

                const size_t n = std::min(ctx.InputTextures.size(), refl.Samplers.size());
                for (size_t i = 0; i < n; ++i) {
                    if (ctx.InputTextures[i])
                        set->SetTexture(refl.Samplers[i].Binding, ctx.InputTextures[i], m_LinearSampler);
                }
                set->Update();
                cmd.BindDescriptorSet(set, 0);
            }
        }

        cmd.Draw(3, 1, 0, 0);   // fullscreen triangle
    }

    // ------------------------------------------------------------------
    // Compute pass — bind a compute pipeline + its declared resources and dispatch.
    // Storage-image inputs (AsStorageImage) bind at their authored binding; sampled
    // inputs bind at the shader's reflected sampler binding. Group counts come from
    // the pass declaration (default 1 on any unset axis).
    // ------------------------------------------------------------------

    Ref<ComputePipeline> RayRenderer::GetComputePipeline(const std::string& passName,
                                                         const std::string& shaderName) {
        if (auto it = m_ComputePipelines.find(passName);
            it != m_ComputePipelines.end() && it->second)
            return it->second;

        if (shaderName.empty()) {
            ECHELON_LOG_ERROR("Ray: compute pass '{}' declares no shader", passName);
            return nullptr;
        }

        auto sit = m_FullscreenShaders.find(shaderName);
        Ref<ShaderAsset> shaderAsset = (sit != m_FullscreenShaders.end()) ? sit->second : nullptr;
        if (!shaderAsset) {
            shaderAsset = LoadShaderAsset(shaderName);
            if (shaderAsset) m_FullscreenShaders[shaderName] = shaderAsset;
        }
        if (!shaderAsset || !shaderAsset->GetGpuShader()) {
            ECHELON_LOG_ERROR("Ray: compute shader '{}' failed to load", shaderName);
            return nullptr;
        }

        ComputePipelineDesc cd;
        cd.ComputeShader = shaderAsset->GetGpuShader();
        cd.DebugName     = "Ray_Compute_" + passName;

        auto pipe = m_Device->CreateComputePipeline(cd);
        m_ComputePipelines[passName] = pipe;
        return pipe;
    }

    void RayRenderer::ExecuteComputePass(CommandBuffer& cmd, const PassContext& ctx) {
        const PassDesc& pass = *ctx.Pass;

        Ref<ComputePipeline> pipe = GetComputePipeline(pass.Name, pass.Shader);
        if (!pipe) return;
        cmd.BindComputePipeline(pipe);

        if (!pass.Inputs.empty() && !ctx.InputTextures.empty()) {
            Ref<DescriptorSet>& set = m_FullscreenSets["compute:" + pass.Name];
            if (!set) set = m_Device->AllocateDescriptorSet(m_FullscreenLayout);

            const size_t n = std::min(pass.Inputs.size(), ctx.InputTextures.size());
            for (size_t i = 0; i < n; ++i) {
                const PassInput& in = pass.Inputs[i];
                if (!ctx.InputTextures[i]) continue;
                if (in.AsStorageImage)
                    set->SetStorageTexture(in.Binding, ctx.InputTextures[i]);
                else
                    set->SetTexture(in.Binding, ctx.InputTextures[i], m_LinearSampler);
            }
            set->Update();
            cmd.BindDescriptorSet(set, 0);
        }

        cmd.Dispatch(std::max(1u, pass.GroupCountX),
                     std::max(1u, pass.GroupCountY),
                     std::max(1u, pass.GroupCountZ));
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

        // Recreate swapchain-relative attachments for the new viewport size.
        m_PassGraph.Resize(width, height);
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
            .Version = "0.2.0",
            .Author  = "Echelon"
        };
    }

    RenderStats RayRenderer::GetStats() const {
        return m_Stats;
    }

} // namespace Echelon
