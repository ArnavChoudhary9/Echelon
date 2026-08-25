#include "RayRenderer.hpp"

#include "Echelon/Core/Log.hpp"
#include "Echelon/GraphicsAPI/Buffer.hpp"
#include "Echelon/GraphicsAPI/Pipeline.hpp"
#include "Echelon/GraphicsAPI/Shader.hpp"
#include "Echelon/GraphicsAPI/Swapchain.hpp"
#include "Echelon/GraphicsAPI/RenderPass.hpp"
#include "Echelon/GraphicsAPI/Framebuffer.hpp"
#include "Echelon/GraphicsAPI/Texture.hpp"
#include "Echelon/GraphicsAPI/CommandBuffer.hpp"
#include "Echelon/GraphicsAPI/DescriptorSet.hpp"

#include "Echelon/Asset/AssetManager.hpp"
#include "Echelon/Asset/Mesh/StandardVertex.hpp"
#include "Echelon/Asset/RenderPipeline/RenderPipelineAsset.hpp"
#include "Echelon/Project/Project.hpp"
#include "Echelon/Scene/Scene.hpp"
#include "Echelon/ECS/Components.hpp"
#include "Echelon/Renderer/RendererLoader.hpp"   // ExecutableDir()
#include "ABI/RayConstants.hpp"                   // renderer-owned shader ABI (CPU mirrors)

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/matrix_inverse.hpp"             // inverseTranspose
#include "glm/gtc/matrix_transform.hpp"          // lookAt / ortho / perspective

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
            [this](CommandBuffer&, const PassContext& ctx) { ExecuteDrawList(ctx); });
        m_PassGraph.SetDefaultCallback(PassType::Fullscreen,
            [this](CommandBuffer& cmd, const PassContext& ctx) { ExecuteFullscreenPass(cmd, ctx); });
        m_PassGraph.SetDefaultCallback(PassType::Compute,
            [this](CommandBuffer& cmd, const PassContext& ctx) { ExecuteComputePass(cmd, ctx); });

        // Compile the pass graph: prefer the project's `.ehpipeline` asset, else the
        // built-in single-pass default. (The project may not be active yet during
        // Init — EnsureUpToDate() resolves the asset lazily on the first frame.)
        if (!TryLoadPipelineAsset()) {
            if (!m_PassGraph.CompileFrom(WithAuxiliary(DefaultForwardDesc()), m_Device, width, height)) {
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

        // Release the editor viewport target (its GL texture) while the context is alive.
        m_PassGraph.SetOffscreenTarget(false);

        m_SystemSets.clear();
        m_MaterialCache.Clear();
        m_FullscreenSets.clear();
        m_FullscreenPipelines.clear();
        m_ComputePipelines.clear();
        m_OverridePipelines.clear();
        m_FullscreenShaders.clear();
        m_FullscreenLayout  = nullptr;
        m_LinearSampler     = nullptr;
        m_FullscreenVBO     = nullptr;
        m_PipelineAsset     = nullptr;
        m_SystemLayout      = nullptr;
        m_FrameUBO          = nullptr;
        m_ObjectUBO         = nullptr;
        m_LightUBO          = nullptr;
        m_ShadowUBO         = nullptr;
        m_ShadowPassUBO     = nullptr;
        m_IblUBO            = nullptr;
        m_ShadowDirMap      = nullptr;
        m_ShadowSpotMap     = nullptr;
        m_ShadowPointMap    = nullptr;
        m_ShadowPointDepth  = nullptr;
        m_EnvCube           = nullptr;
        m_IrradianceMap     = nullptr;
        m_PrefilterMap      = nullptr;
        m_BrdfLUT           = nullptr;
        m_ShadowSampler     = nullptr;
        m_ShadowDepthPass   = nullptr;
        m_ShadowCubePass    = nullptr;
        m_ShadowDirFB       = nullptr;
        m_ShadowSpotFB      = nullptr;
        for (auto& fb : m_ShadowPointFB) fb = nullptr;
        m_ShadowDepthShader = nullptr;
        m_ShadowCubeShader  = nullptr;
        m_ShadowDepthPipeline = nullptr;
        m_ShadowCubePipeline  = nullptr;
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
        auto& assets = AssetManager::Get();

        // Resolve a shader that a material or `.ehpipeline` pass references. Order:
        //  - built-in engine/renderer shaders ship next to the executable (bare names
        //    like "Flat.slang" / "Tonemap.slang");
        //  - otherwise a PROJECT shader — an explicit "shader:" handle, or a path
        //    resolved against the active project's Assets dir (custom project shaders).
        // The ShaderImporter already puts <exe>/Shaders on Slang's search path, so a
        // project shader can still `import Echelon`.
        UUID handle;
        const fs::path builtin = RendererLoader::ExecutableDir() / "Shaders" / name;
        if (name.rfind("shader:", 0) != 0 && fs::exists(builtin))
            handle = assets.GetHandle(builtin.string());   // built-in beside the exe
        else
            handle = assets.GetHandle(name);               // project-relative or "shader:" handle

        if (handle.IsNull()) {
            ECHELON_LOG_ERROR("Ray: could not resolve shader '{}'", name);
            return nullptr;
        }

        auto shader = assets.GetAssetAs<ShaderAsset>(handle);
        if (!shader) {
            ECHELON_LOG_ERROR("Ray: '{}' is not a ShaderAsset", name);
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

        BufferDesc lightDesc;
        lightDesc.Size      = sizeof(LightConstantsCPU);
        lightDesc.Usage     = BufferUsage::UniformBuffer;
        lightDesc.Memory    = MemoryUsage::CPUToGPU;
        lightDesc.DebugName = "Ray_LightUBO";
        m_LightUBO = m_Device->CreateBuffer(lightDesc);

        // Shadow + IBL system UBOs. Seeded with "no casters / IBL off" defaults so a PBR
        // shader renders correctly before the shadow/IBL systems fill them (Phases 5/6).
        BufferDesc shadowDesc;
        shadowDesc.Size      = sizeof(ShadowConstantsCPU);
        shadowDesc.Usage     = BufferUsage::UniformBuffer;
        shadowDesc.Memory    = MemoryUsage::CPUToGPU;
        shadowDesc.DebugName = "Ray_ShadowUBO";
        m_ShadowUBO = m_Device->CreateBuffer(shadowDesc);
        { ShadowConstantsCPU sc; m_ShadowUBO->SetData(&sc, sizeof(sc)); }

        BufferDesc shadowPassDesc;
        shadowPassDesc.Size      = sizeof(ShadowPassConstantsCPU);
        shadowPassDesc.Usage     = BufferUsage::UniformBuffer;
        shadowPassDesc.Memory    = MemoryUsage::CPUToGPU;
        shadowPassDesc.DebugName = "Ray_ShadowPassUBO";
        m_ShadowPassUBO = m_Device->CreateBuffer(shadowPassDesc);

        BufferDesc iblDesc;
        iblDesc.Size      = sizeof(IblConstantsCPU);
        iblDesc.Usage     = BufferUsage::UniformBuffer;
        iblDesc.Memory    = MemoryUsage::CPUToGPU;
        iblDesc.DebugName = "Ray_IblUBO";
        m_IblUBO = m_Device->CreateBuffer(iblDesc);
        { IblConstantsCPU ic; m_IblUBO->SetData(&ic, sizeof(ic)); }

        // A generic 3-binding layout (GL ignores layout at bind time; buffers are
        // assigned to the per-shader reflected bindings in BindSystemConstants).
        DescriptorSetLayoutDesc slDesc;
        slDesc.Bindings = {
            { 0, DescriptorType::UniformBuffer, 1, ShaderStage::Vertex },
            { 1, DescriptorType::UniformBuffer, 1, ShaderStage::Vertex },
            { 2, DescriptorType::UniformBuffer, 1, ShaderStage::Fragment },  // g_Lights
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

        // Fullscreen-triangle VBO. Position lives at MeshVertex offset 0 with the canonical
        // 32-byte stride, so the reflection-driven pipeline layout (StandardVertex) binds it
        // correctly. Fullscreen passes draw this instead of gl_VertexID — attributeless
        // draws (empty VAO) don't rasterize on NVIDIA GL.
        {
            const float fsTri[] = {
                // position            normal      uv
                -1.0f, -1.0f, 0.0f,   0,0,0,     0.0f, 0.0f,
                 3.0f, -1.0f, 0.0f,   0,0,0,     0.0f, 0.0f,
                -1.0f,  3.0f, 0.0f,   0,0,0,     0.0f, 0.0f,
            };
            BufferDesc vb;
            vb.Size      = sizeof(fsTri);
            vb.Usage     = BufferUsage::VertexBuffer;
            vb.Memory    = MemoryUsage::CPUToGPU;
            vb.DebugName = "Ray_FullscreenVBO";
            m_FullscreenVBO = m_Device->CreateBuffer(vb);
            m_FullscreenVBO->SetData(fsTri, sizeof(fsTri));
        }

        DescriptorSetLayoutDesc fsLayout;
        fsLayout.Bindings  = { { 0, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment } };
        fsLayout.DebugName = "Ray_FullscreenLayout";
        m_FullscreenLayout = m_Device->CreateDescriptorSetLayout(fsLayout);

        m_FlatShaderAsset  = LoadShaderAsset("Flat.slang");
        m_ErrorShaderAsset = LoadShaderAsset("Error.slang");
        BuildDefaultPipeline();

        CreateShadowResources();
        PrecomputeIBL();

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

    // A fullscreen-triangle pipeline (depth off) for offscreen precompute passes.
    static Ref<Pipeline> BuildFullscreenPipeline(const Ref<Device>& device, const Ref<RenderPass>& pass,
                                                 const Ref<ShaderAsset>& shader, const char* name) {
        if (!shader || !shader->GetGpuShader()) return nullptr;
        PipelineDesc pd;
        pd.ShaderProgram = shader->GetGpuShader();
        pd.Topology      = PrimitiveTopology::TriangleList;
        pd.Pass          = pass;
        pd.Layout        = StandardVertex::FromReflection(shader->GetReflection());   // empty (SV_VertexID)
        pd.Depth.DepthTestEnable  = false;
        pd.Depth.DepthWriteEnable = false;
        pd.Raster.Cull            = CullMode::None;
        pd.DebugName     = name;
        return device->CreatePipeline(pd);
    }

    void RayRenderer::BuildDefaultPipeline() {
        const Ref<RenderPass> scenePass = GetScenePass();
        m_FlatPipeline  = BuildPipeline(m_Device, scenePass, m_FlatShaderAsset,  "Ray_FlatPipeline");
        m_ErrorPipeline = BuildPipeline(m_Device, scenePass, m_ErrorShaderAsset, "Ray_ErrorPipeline");
        if (!m_FlatPipeline)  ECHELON_LOG_ERROR("Ray: no flat shader — default pipeline not built");
        if (!m_ErrorPipeline) ECHELON_LOG_ERROR("Ray: no error shader — pink fallback unavailable");
    }

    // ------------------------------------------------------------------
    // Shadow system (renderer-owned; one caster per light type)
    // ------------------------------------------------------------------

    void RayRenderer::CreateShadowResources() {
        if (!m_Device) return;

        // Nearest/clamp sampler for shadow-map reads (manual PCF; no hardware compare).
        SamplerDesc ss;
        ss.MinFilter    = FilterMode::Nearest;
        ss.MagFilter    = FilterMode::Nearest;
        ss.MipMapFilter = FilterMode::Nearest;
        ss.AddressU     = AddressMode::ClampToEdge;
        ss.AddressV     = AddressMode::ClampToEdge;
        ss.AddressW     = AddressMode::ClampToEdge;
        m_ShadowSampler = m_Device->CreateSampler(ss);

        // Depth maps (directional + spot) — sampleable D32F 2D textures.
        TextureDesc dird;
        dird.Type   = TextureType::Depth;
        dird.Format = TextureFormat::D32_FLOAT;
        dird.Usage  = TextureUsage::Sampled | TextureUsage::DepthStencil;
        dird.Width  = dird.Height = m_ShadowRes;
        dird.DebugName = "ShadowDirMap";
        m_ShadowDirMap = m_Device->CreateTexture(dird);
        dird.DebugName = "ShadowSpotMap";
        m_ShadowSpotMap = m_Device->CreateTexture(dird);

        // Point light distance cubemap (R32F) + scratch depth for z-testing each face.
        TextureDesc cubed;
        cubed.Type   = TextureType::TextureCube;
        cubed.Format = TextureFormat::R32_FLOAT;
        cubed.Usage  = TextureUsage::Sampled | TextureUsage::RenderTarget;
        cubed.Width  = cubed.Height = m_PointShadowRes;
        cubed.DebugName = "ShadowPointMap";
        m_ShadowPointMap = m_Device->CreateTexture(cubed);

        TextureDesc pdep;
        pdep.Type   = TextureType::Depth;
        pdep.Format = TextureFormat::D32_FLOAT;
        pdep.Usage  = TextureUsage::DepthStencil;
        pdep.Width  = pdep.Height = m_PointShadowRes;
        pdep.DebugName = "ShadowPointDepth";
        m_ShadowPointDepth = m_Device->CreateTexture(pdep);

        // Render passes.
        RenderPassDesc depthPass;
        depthPass.HasDepthAttachment       = true;
        depthPass.DepthAttachment.Format   = TextureFormat::D32_FLOAT;
        depthPass.DepthAttachment.Load     = LoadOp::Clear;
        depthPass.DepthAttachment.Store    = StoreOp::Store;
        depthPass.DepthAttachment.Clear.Depth = 1.0f;
        depthPass.DebugName = "ShadowDepthPass";
        m_ShadowDepthPass = m_Device->CreateRenderPass(depthPass);

        RenderPassDesc cubePass;
        ColorAttachmentDesc cc;
        cc.Format = TextureFormat::R32_FLOAT;
        cc.Load   = LoadOp::Clear;
        cc.Store  = StoreOp::Store;
        cc.Clear  = ClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };   // cleared "far" = lit
        cubePass.ColorAttachments.push_back(cc);
        cubePass.HasDepthAttachment       = true;
        cubePass.DepthAttachment.Format   = TextureFormat::D32_FLOAT;
        cubePass.DepthAttachment.Load     = LoadOp::Clear;
        cubePass.DepthAttachment.Store    = StoreOp::DontCare;
        cubePass.DepthAttachment.Clear.Depth = 1.0f;
        cubePass.DebugName = "ShadowCubePass";
        m_ShadowCubePass = m_Device->CreateRenderPass(cubePass);

        // Framebuffers.
        FramebufferDesc dfb;
        dfb.Width  = dfb.Height = m_ShadowRes;
        dfb.HasDepthAttachment              = true;
        dfb.DepthAttachment.ExistingTexture = m_ShadowDirMap;
        dfb.DepthAttachment.Format          = TextureFormat::D32_FLOAT;
        dfb.CompatiblePass = m_ShadowDepthPass;
        dfb.DebugName      = "ShadowDirFB";
        m_ShadowDirFB = m_Device->CreateFramebuffer(dfb);

        dfb.DepthAttachment.ExistingTexture = m_ShadowSpotMap;
        dfb.DebugName = "ShadowSpotFB";
        m_ShadowSpotFB = m_Device->CreateFramebuffer(dfb);

        for (uint32_t f = 0; f < 6; ++f) {
            FramebufferDesc cfb;
            cfb.Width = cfb.Height = m_PointShadowRes;
            FramebufferAttachment color;
            color.ExistingTexture = m_ShadowPointMap;
            color.Format          = TextureFormat::R32_FLOAT;
            color.Layer           = f;                         // cube face
            cfb.ColorAttachments.push_back(color);
            cfb.HasDepthAttachment              = true;
            cfb.DepthAttachment.ExistingTexture = m_ShadowPointDepth;
            cfb.DepthAttachment.Format          = TextureFormat::D32_FLOAT;
            cfb.CompatiblePass = m_ShadowCubePass;
            cfb.DebugName      = "ShadowPointFB";
            m_ShadowPointFB[f] = m_Device->CreateFramebuffer(cfb);
        }

        // Depth-only shaders + pipelines (built against the shadow passes).
        m_ShadowDepthShader = LoadShaderAsset("ShadowDepth.slang");
        m_ShadowCubeShader  = LoadShaderAsset("ShadowCube.slang");
        m_ShadowDepthPipeline = BuildPipeline(m_Device, m_ShadowDepthPass, m_ShadowDepthShader, "Ray_ShadowDepthPipeline");
        m_ShadowCubePipeline  = BuildPipeline(m_Device, m_ShadowCubePass,  m_ShadowCubeShader,  "Ray_ShadowCubePipeline");
        if (!m_ShadowDepthPipeline) ECHELON_LOG_ERROR("Ray: shadow depth pipeline unavailable (missing ShadowDepth.slang?)");
        if (!m_ShadowCubePipeline)  ECHELON_LOG_ERROR("Ray: shadow cube pipeline unavailable (missing ShadowCube.slang?)");
    }

    // Draw all scene geometry through a depth/distance pipeline (used by every shadow view).
    void RayRenderer::RenderSceneDepth(const Ref<Pipeline>& pipeline) {
        if (!pipeline) return;
        m_CommandBuffer->BindPipeline(pipeline);
        BindSystemConstants(pipeline);   // binds g_Object + g_ShadowPass by name
        const auto& shader = pipeline->GetShader();

        // Opaque geometry only casts into the depth/distance map (transparent draws don't
        // write depth). Iterate pipeline groups → instance groups → mesh batches.
        for (const auto& group : m_RenderGraph.GetPipelineGroups()) {
            for (const auto& inst : group.Instances) {
                for (const auto& batch : inst.Batches) {
                    for (size_t i = 0; i < batch.Transforms.size(); ++i) {
                        const auto& transform = batch.Transforms[i];
                        if (batch.IndexBuffer && batch.IndexCount > 0)
                            DrawIndexed(batch.VertexBuffer, batch.IndexBuffer, shader, transform, batch.IndexCount);
                        else
                            Draw(batch.VertexBuffer, shader, transform, batch.VertexCount);
                    }
                }
            }
        }
    }

    void RayRenderer::RenderShadowMaps() {
        ShadowConstantsCPU sc;   // defaults: all caster indices = -1 (no shadows)

        // Scene bounds (from the draw list) → fit the directional ortho frustum.
        glm::vec3 bmin(1e9f), bmax(-1e9f);
        bool hasGeo = false;
        for (const auto& group : m_RenderGraph.GetPipelineGroups())
            for (const auto& inst : group.Instances)
                for (const auto& batch : inst.Batches)
                    for (const auto& t : batch.Transforms) {
                        const glm::vec3 p = glm::vec3(t[3]);
                        const float r = 0.87f * glm::max(glm::length(glm::vec3(t[0])),
                                              glm::max(glm::length(glm::vec3(t[1])), glm::length(glm::vec3(t[2]))));
                        bmin = glm::min(bmin, p - glm::vec3(r));
                        bmax = glm::max(bmax, p + glm::vec3(r));
                        hasGeo = true;
                    }
        const glm::vec3 center = hasGeo ? (bmin + bmax) * 0.5f : glm::vec3(0.0f);
        float radius = hasGeo ? glm::length(bmax - center) + 1.0f : 15.0f;
        radius = glm::max(radius, 1.0f);

        const float normalBias = 0.05f;

        auto setPass = [&](const glm::mat4& vp, const glm::vec3& pos, float farp) {
            ShadowPassConstantsCPU spc;
            spc.LightViewProj = vp;
            spc.LightPosFar   = glm::vec4(pos, farp);
            if (m_ShadowPassUBO) m_ShadowPassUBO->SetData(&spc, sizeof(spc));
        };
        Viewport vp; vp.X = 0.0f; vp.Y = 0.0f; vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;

        // ---- Directional (orthographic) ----
        if (m_DirCaster.Index >= 0 && m_ShadowDirFB && m_ShadowDepthPipeline) {
            const glm::vec3 dir = glm::normalize(m_DirCaster.Direction);
            const glm::vec3 up  = (glm::abs(dir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
            const glm::vec3 eye = center - dir * (radius * 2.0f + 1.0f);
            const glm::mat4 view = glm::lookAt(eye, center, up);
            const float ext = radius * 1.15f;
            const glm::mat4 proj = glm::ortho(-ext, ext, -ext, ext, 0.05f, radius * 4.0f + 2.0f);
            const glm::mat4 lvp  = proj * view;
            sc.DirViewProj = lvp;
            sc.DirParams   = glm::vec4(static_cast<float>(m_DirCaster.Index), m_DirCaster.Bias, normalBias, 1.0f);

            setPass(lvp, glm::vec3(0.0f), 1.0f);
            vp.Width = vp.Height = static_cast<float>(m_ShadowRes);
            m_CommandBuffer->SetViewport(vp);
            m_CommandBuffer->BeginRenderPass(m_ShadowDepthPass, m_ShadowDirFB);
            RenderSceneDepth(m_ShadowDepthPipeline);
            m_CommandBuffer->EndRenderPass();
        }

        // ---- Spot (perspective) ----
        if (m_SpotCaster.Index >= 0 && m_ShadowSpotFB && m_ShadowDepthPipeline) {
            const glm::vec3 pos = m_SpotCaster.Position;
            const glm::vec3 dir = glm::normalize(m_SpotCaster.Direction);
            const glm::vec3 up  = (glm::abs(dir.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
            const float outerHalf = glm::acos(glm::clamp(m_SpotCaster.CosOuter, -1.0f, 1.0f));
            const float fov  = glm::min(glm::radians(179.0f), 2.0f * outerHalf * 1.1f);
            const float farp = glm::max(m_SpotCaster.Range, 1.0f);
            const glm::mat4 view = glm::lookAt(pos, pos + dir, up);
            const glm::mat4 proj = glm::perspective(fov, 1.0f, 0.05f, farp);
            const glm::mat4 lvp  = proj * view;
            sc.SpotViewProj = lvp;
            sc.SpotParams   = glm::vec4(static_cast<float>(m_SpotCaster.Index), m_SpotCaster.Bias, normalBias, 1.0f);

            setPass(lvp, glm::vec3(0.0f), 1.0f);
            vp.Width = vp.Height = static_cast<float>(m_ShadowRes);
            m_CommandBuffer->SetViewport(vp);
            m_CommandBuffer->BeginRenderPass(m_ShadowDepthPass, m_ShadowSpotFB);
            RenderSceneDepth(m_ShadowDepthPipeline);
            m_CommandBuffer->EndRenderPass();
        }

        // ---- Point (depth cubemap, 6 faces) ----
        if (m_PointCaster.Index >= 0 && m_ShadowCubePipeline) {
            const glm::vec3 pos  = m_PointCaster.Position;
            const float     farp = glm::max(m_PointCaster.Range, 1.0f);
            const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, farp);
            const glm::vec3 dirs[6] = { { 1,0,0 }, { -1,0,0 }, { 0,1,0 }, { 0,-1,0 }, { 0,0,1 }, { 0,0,-1 } };
            const glm::vec3 ups[6]  = { { 0,-1,0 }, { 0,-1,0 }, { 0,0,1 }, { 0,0,-1 }, { 0,-1,0 }, { 0,-1,0 } };
            sc.PointPosIndex = glm::vec4(pos, static_cast<float>(m_PointCaster.Index));
            sc.PointParams   = glm::vec4(farp, glm::max(m_PointCaster.Bias, 0.05f), 1.0f, 0.0f);

            vp.Width = vp.Height = static_cast<float>(m_PointShadowRes);
            for (uint32_t f = 0; f < 6; ++f) {
                if (!m_ShadowPointFB[f]) continue;
                const glm::mat4 lvp = proj * glm::lookAt(pos, pos + dirs[f], ups[f]);
                setPass(lvp, pos, farp);
                m_CommandBuffer->SetViewport(vp);
                m_CommandBuffer->BeginRenderPass(m_ShadowCubePass, m_ShadowPointFB[f]);
                RenderSceneDepth(m_ShadowCubePipeline);
                m_CommandBuffer->EndRenderPass();
            }
        }

        sc.MapParams = glm::vec4(1.0f / static_cast<float>(m_ShadowRes),
                                 1.0f / static_cast<float>(m_ShadowRes),
                                 1.0f / static_cast<float>(m_PointShadowRes), 0.0f);
        if (m_ShadowUBO) m_ShadowUBO->SetData(&sc, sizeof(sc));
    }

    // ------------------------------------------------------------------
    // IBL precompute (procedural sky → env cube → irradiance + prefilter + BRDF LUT)
    // Runs once at init. On any failure IBL stays disabled (PBR falls back to constant
    // ambient), so a broken precompute never breaks the main render.
    // ------------------------------------------------------------------
    void RayRenderer::PrecomputeIBL() {
        if (!m_Device || !m_CommandBuffer) return;

        constexpr uint32_t envSize = 128, irrSize = 32, preSize = 128, lutSize = 512;
        constexpr uint32_t preMips = 5;

        auto makeCube = [&](uint32_t size, uint32_t mips, const char* dbg) {
            TextureDesc d;
            d.Type   = TextureType::TextureCube;
            d.Format = TextureFormat::RGBA16_FLOAT;
            d.Usage  = TextureUsage::Sampled | TextureUsage::RenderTarget;
            d.Width  = d.Height = size;
            d.MipLevels = mips;
            d.DebugName = dbg;
            return m_Device->CreateTexture(d);
        };
        m_EnvCube       = makeCube(envSize, 1, "IblEnvCube");
        m_IrradianceMap = makeCube(irrSize, 1, "IblIrradiance");
        m_PrefilterMap  = makeCube(preSize, preMips, "IblPrefilter");

        TextureDesc lutd;
        lutd.Type   = TextureType::Texture2D;
        lutd.Format = TextureFormat::RG16_FLOAT;
        lutd.Usage  = TextureUsage::Sampled | TextureUsage::RenderTarget;
        lutd.Width  = lutd.Height = lutSize;
        lutd.DebugName = "IblBrdfLUT";
        m_BrdfLUT = m_Device->CreateTexture(lutd);

        RenderPassDesc cubePassDesc;
        { ColorAttachmentDesc c; c.Format = TextureFormat::RGBA16_FLOAT; c.Load = LoadOp::DontCare; c.Store = StoreOp::Store; cubePassDesc.ColorAttachments.push_back(c); }
        cubePassDesc.DebugName = "IblCubePass";
        auto iblCubePass = m_Device->CreateRenderPass(cubePassDesc);

        RenderPassDesc lutPassDesc;
        { ColorAttachmentDesc c; c.Format = TextureFormat::RG16_FLOAT; c.Load = LoadOp::DontCare; c.Store = StoreOp::Store; lutPassDesc.ColorAttachments.push_back(c); }
        lutPassDesc.DebugName = "IblLutPass";
        auto iblLutPass = m_Device->CreateRenderPass(lutPassDesc);

        BufferDesc gd;
        gd.Size = sizeof(IblGenParamsCPU);
        gd.Usage = BufferUsage::UniformBuffer;
        gd.Memory = MemoryUsage::CPUToGPU;
        gd.DebugName = "IblGenUBO";
        auto genUBO = m_Device->CreateBuffer(gd);

        auto skySh = LoadShaderAsset("Sky.slang");
        auto irrSh = LoadShaderAsset("IrradianceConv.slang");
        auto preSh = LoadShaderAsset("Prefilter.slang");
        auto lutSh = LoadShaderAsset("BrdfLUT.slang");
        auto skyPipe = BuildFullscreenPipeline(m_Device, iblCubePass, skySh, "Ray_IblSky");
        auto irrPipe = BuildFullscreenPipeline(m_Device, iblCubePass, irrSh, "Ray_IblIrr");
        auto prePipe = BuildFullscreenPipeline(m_Device, iblCubePass, preSh, "Ray_IblPre");
        auto lutPipe = BuildFullscreenPipeline(m_Device, iblLutPass, lutSh, "Ray_IblLut");
        if (!skyPipe || !irrPipe || !prePipe || !lutPipe) {
            ECHELON_LOG_ERROR("Ray: IBL shaders unavailable — IBL disabled (constant ambient fallback)");
            return;
        }

        // Cube-face basis (GL cubemap texel→direction convention).
        struct Face { glm::vec4 ma, sc, tc; };
        const Face faces[6] = {
            { { 1, 0, 0, 0 }, {  0, 0, -1, 0 }, { 0, -1,  0, 0 } }, // +X
            { { -1, 0, 0, 0 }, { 0, 0,  1, 0 }, { 0, -1,  0, 0 } }, // -X
            { { 0, 1, 0, 0 }, {  1, 0,  0, 0 }, { 0,  0,  1, 0 } }, // +Y
            { { 0, -1, 0, 0 }, { 1, 0,  0, 0 }, { 0,  0, -1, 0 } }, // -Y
            { { 0, 0, 1, 0 }, {  1, 0,  0, 0 }, { 0, -1,  0, 0 } }, // +Z
            { { 0, 0, -1, 0 }, { -1, 0, 0, 0 }, { 0, -1,  0, 0 } }, // -Z
        };
        glm::vec3 sunDir = (m_DirCaster.Index >= 0)
            ? glm::normalize(-m_DirCaster.Direction)
            : glm::normalize(glm::vec3(0.3f, 0.8f, 0.4f));

        auto bindGen = [&](const Ref<Pipeline>& pipe, const Ref<Texture>& env) {
            const ShaderReflection& refl = pipe->GetShader()->GetReflection();
            auto set = m_Device->AllocateDescriptorSet(m_FullscreenLayout);
            uint32_t b = 0;
            if (FindUBOBinding(refl, "g_IblGen", b))    set->SetBuffer(b, genUBO);
            if (env && FindSamplerBinding(refl, "g_EnvCube", b)) set->SetTexture(b, env, m_LinearSampler);
            set->Update();
            m_CommandBuffer->BindDescriptorSet(set, 0);
        };

        Viewport vp; vp.X = 0.0f; vp.Y = 0.0f; vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;

        auto renderCube = [&](const Ref<Texture>& target, uint32_t size, uint32_t mip,
                              const Ref<Pipeline>& pipe, const Ref<Texture>& env, float roughness) {
            for (uint32_t f = 0; f < 6; ++f) {
                IblGenParamsCPU g;
                g.MaAxis = faces[f].ma; g.ScVec = faces[f].sc; g.TcVec = faces[f].tc;
                g.Params = glm::vec4(roughness, static_cast<float>(preMips), 0.0f, 0.0f);
                g.Sun    = glm::vec4(sunDir, 6.0f);
                genUBO->SetData(&g, sizeof(g));

                FramebufferDesc fb;
                fb.Width = fb.Height = size;
                FramebufferAttachment att;
                att.ExistingTexture = target;
                att.Format   = TextureFormat::RGBA16_FLOAT;
                att.Layer    = f;
                att.MipLevel = mip;
                fb.ColorAttachments.push_back(att);
                fb.CompatiblePass = iblCubePass;
                auto fbo = m_Device->CreateFramebuffer(fb);

                vp.Width = vp.Height = static_cast<float>(size);
                m_CommandBuffer->SetViewport(vp);
                m_CommandBuffer->BeginRenderPass(iblCubePass, fbo);
                m_CommandBuffer->BindPipeline(pipe);
                bindGen(pipe, env);
                m_CommandBuffer->BindVertexBuffer(m_FullscreenVBO);
                m_CommandBuffer->Draw(3, 1, 0, 0);
                m_CommandBuffer->EndRenderPass();
            }
        };

        // 1) Environment cube from the procedural sky.
        renderCube(m_EnvCube, envSize, 0, skyPipe, nullptr, 0.0f);
        // 2) Diffuse irradiance from the env cube.
        renderCube(m_IrradianceMap, irrSize, 0, irrPipe, m_EnvCube, 0.0f);
        // 3) Prefiltered specular per roughness mip.
        for (uint32_t mip = 0; mip < preMips; ++mip) {
            uint32_t msize = std::max(1u, preSize >> mip);
            float rough = (preMips > 1) ? static_cast<float>(mip) / static_cast<float>(preMips - 1) : 0.0f;
            renderCube(m_PrefilterMap, msize, mip, prePipe, m_EnvCube, rough);
        }
        // 4) BRDF LUT (fullscreen 2D).
        {
            FramebufferDesc fb;
            fb.Width = fb.Height = lutSize;
            FramebufferAttachment att;
            att.ExistingTexture = m_BrdfLUT;
            att.Format = TextureFormat::RG16_FLOAT;
            fb.ColorAttachments.push_back(att);
            fb.CompatiblePass = iblLutPass;
            auto fbo = m_Device->CreateFramebuffer(fb);
            vp.Width = vp.Height = static_cast<float>(lutSize);
            m_CommandBuffer->SetViewport(vp);
            m_CommandBuffer->BeginRenderPass(iblLutPass, fbo);
            m_CommandBuffer->BindPipeline(lutPipe);
            m_CommandBuffer->BindVertexBuffer(m_FullscreenVBO);
            m_CommandBuffer->Draw(3, 1, 0, 0);
            m_CommandBuffer->EndRenderPass();
        }

        // Enable IBL for PBR.
        IblConstantsCPU ic;
        ic.Params = glm::vec4(static_cast<float>(preMips - 1), 1.0f, 1.0f, 0.0f);
        if (m_IblUBO) m_IblUBO->SetData(&ic, sizeof(ic));

        ECHELON_LOG_INFO("Ray: IBL precomputed (env {} / irr {} / prefilter {} x{} mips / BRDF {})",
                         envSize, irrSize, preSize, preMips, lutSize);
    }

    // ------------------------------------------------------------------
    // Auxiliary passes (generic pass-graph extension)
    //
    // A host (e.g. the editor) contributes extra resources + passes that are
    // merged into whichever pipeline the renderer compiles. Nothing here knows
    // about "picking" — an auxiliary Graphics pass that declares a Shader simply
    // draws all scene geometry with that one pipeline (see ExecuteDrawList), and
    // its output is read back with ReadTargetPixel().
    // ------------------------------------------------------------------

    RenderPipelineDesc RayRenderer::WithAuxiliary(const RenderPipelineDesc& base) const {
        if (!m_HasAux) return base;
        RenderPipelineDesc d = base;
        d.Resources.insert(d.Resources.end(), m_AuxDesc.Resources.begin(), m_AuxDesc.Resources.end());
        d.Passes.insert(d.Passes.end(), m_AuxDesc.Passes.begin(), m_AuxDesc.Passes.end());
        return d;
    }

    void RayRenderer::SetAuxiliaryPipeline(const RenderPipelineDesc& aux) {
        m_AuxDesc = aux;
        m_HasAux  = !aux.Passes.empty() || !aux.Resources.empty();
        if (m_Initialized) {
            RecompilePassGraph();     // rebuild the graph with (or without) the aux passes
            BuildDefaultPipeline();   // scene pipelines follow the (possibly rebuilt) forward pass
        }
    }

    // Build/cache the single pipeline an override-shader Graphics pass draws all
    // geometry with. Built against that pass's RenderPass with depth test on so the
    // front-most surface wins (depth prepass / id pass / normals pass / …).
    Ref<Pipeline> RayRenderer::GetOverridePipeline(const std::string& passName,
                                                   const std::string& shaderName) {
        if (auto it = m_OverridePipelines.find(passName);
            it != m_OverridePipelines.end() && it->second)
            return it->second;
        if (shaderName.empty()) return nullptr;

        auto sit = m_FullscreenShaders.find(shaderName);
        Ref<ShaderAsset> shaderAsset = (sit != m_FullscreenShaders.end()) ? sit->second : nullptr;
        if (!shaderAsset) {
            shaderAsset = LoadShaderAsset(shaderName);
            if (shaderAsset) m_FullscreenShaders[shaderName] = shaderAsset;
        }
        if (!shaderAsset || !shaderAsset->GetGpuShader()) {
            ECHELON_LOG_ERROR("Ray: override shader '{}' (pass '{}') failed to load", shaderName, passName);
            return nullptr;
        }

        auto pipe = BuildPipeline(m_Device, m_PassGraph.GetRenderPass(passName), shaderAsset,
                                  ("Ray_Override_" + passName).c_str());
        m_OverridePipelines[passName] = pipe;
        return pipe;
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
        m_MaterialCache.Clear();                                     // material pipelines/sets rebuilt lazily
        m_FullscreenPipelines.clear();                               // rebuilt lazily
        m_ComputePipelines.clear();
        m_OverridePipelines.clear();                                 // rebuilt lazily against new passes
        for (auto& [name, sh] : m_FullscreenShaders)
            if (sh) sh->UploadGPU(this);                             // rebuild post GL programs if released
        if (m_PipelineAsset) RecompilePassGraph();
        if (m_FlatShaderAsset)  m_FlatShaderAsset->UploadGPU(this);   // rebuild GL program if released
        if (m_ErrorShaderAsset) m_ErrorShaderAsset->UploadGPU(this);
        BuildDefaultPipeline();

        // Rebuild shadow shaders/pipelines too (their GL programs may have been released).
        if (m_ShadowDepthShader) { m_ShadowDepthShader->UploadGPU(this); m_ShadowDepthPipeline = BuildPipeline(m_Device, m_ShadowDepthPass, m_ShadowDepthShader, "Ray_ShadowDepthPipeline"); }
        if (m_ShadowCubeShader)  { m_ShadowCubeShader->UploadGPU(this);  m_ShadowCubePipeline  = BuildPipeline(m_Device, m_ShadowCubePass,  m_ShadowCubeShader,  "Ray_ShadowCubePipeline"); }

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
        // Fullscreen/override pipelines are built against per-pass RenderPass objects that
        // a recompile replaces, so drop them (rebuilt lazily against the new passes).
        m_FullscreenPipelines.clear();
        m_ComputePipelines.clear();
        m_OverridePipelines.clear();

        // Auxiliary resources/passes (SetAuxiliaryPipeline) are merged into whichever
        // base pipeline compiles.
        if (m_PipelineAsset && m_PipelineAsset->IsValid()) {
            if (m_PassGraph.CompileFrom(WithAuxiliary(m_PipelineAsset->GetDescription()), m_Device,
                                        m_ViewportWidth, m_ViewportHeight))
                return true;
            ECHELON_LOG_ERROR("Ray: pipeline asset failed to compile; using built-in default");
        }
        return m_PassGraph.CompileFrom(WithAuxiliary(DefaultForwardDesc()), m_Device,
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

            // System UBOs — resolved by name and bound only where the shader references
            // them (Slang strips unused ones, so FindUBOBinding fails → skipped).
            const std::pair<const char*, Ref<Buffer>> sysUBOs[] = {
                { "g_Frame",      m_FrameUBO      },
                { "g_Object",     m_ObjectUBO     },
                { "g_Lights",     m_LightUBO      },
                { "g_Shadows",    m_ShadowUBO     },
                { "g_ShadowPass", m_ShadowPassUBO },
                { "g_Ibl",        m_IblUBO        },
            };
            uint32_t binding = 0;
            for (const auto& [name, buf] : sysUBOs)
                if (buf && FindUBOBinding(refl, name, binding))
                    set->SetBuffer(binding, buf);

            // System samplers — shadow maps (nearest/clamp) + IBL maps (linear/mip),
            // bound at the reflected binding. Null until the shadow/IBL systems create
            // them (Phases 5/6); a PBR shader guards its samples until then.
            const Ref<Sampler> shadowSampler = m_ShadowSampler ? m_ShadowSampler : m_LinearSampler;
            const std::tuple<const char*, Ref<Texture>, Ref<Sampler>> sysTextures[] = {
                { "g_ShadowDir",     m_ShadowDirMap,   shadowSampler   },
                { "g_ShadowSpot",    m_ShadowSpotMap,  shadowSampler   },
                { "g_ShadowPoint",   m_ShadowPointMap, shadowSampler   },
                { "g_IrradianceMap", m_IrradianceMap,  m_LinearSampler },
                { "g_PrefilterMap",  m_PrefilterMap,   m_LinearSampler },
                { "g_BrdfLUT",       m_BrdfLUT,        m_LinearSampler },
            };
            for (const auto& [name, tex, samp] : sysTextures)
                if (tex && FindSamplerBinding(refl, name, binding))
                    set->SetTexture(binding, tex, samp);

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

    // Gather scene lights into the g_Lights UBO (lighting scaffold). Existing shaders
    // ignore g_Lights; a future PBR shader consumes it. Direction/position come from
    // each light entity's TransformComponent.
    void RayRenderer::BeginScene(const Ref<Scene>& scene) {
        if (!scene || !m_LightUBO) return;

        LightConstantsCPU lc;
        lc.Ambient = glm::vec4(0.03f, 0.03f, 0.03f, 1.0f);

        // Reset shadow casters each scene; the first CastsShadows light of each type wins.
        m_DirCaster = {}; m_SpotCaster = {}; m_PointCaster = {};

        if (auto registry = scene->GetEntityRegistry().lock()) {
            int count = 0;
            auto view = registry->view<LightComponent, TransformComponent>();
            for (auto entity : view) {
                if (count >= ECHELON_MAX_LIGHTS) break;
                const auto& light = view.get<LightComponent>(entity);
                if (!light.Enabled) continue;   // soft-disabled lights contribute nothing
                const auto& tc    = view.get<TransformComponent>(entity);

                // Rotation (euler radians) → forward direction; default facing -Z. This
                // is the light's LOCAL facing (relative to its parent).
                const glm::vec3 euler = glm::radians(tc.Rotation);
                glm::vec3 localDir = glm::normalize(glm::vec3(
                    -glm::sin(euler.y) * glm::cos(euler.x),
                     glm::sin(euler.x),
                    -glm::cos(euler.y) * glm::cos(euler.x)));

                // Hierarchy-aware world placement: TransformComponent is LOCAL, so the
                // light's world position is the world-matrix translation, and its world
                // facing is the parent's world rotation applied to the local forward.
                // Root (unparented) lights are unaffected (parentWorld == identity).
                Entity    lightEntity = scene->FindEntityByUUID(registry->get<IDComponent>(entity).ID);
                glm::vec3 pos = glm::vec3(scene->GetWorldTransform(lightEntity)[3]);
                glm::vec3 dir = localDir;
                if (const auto* rel = registry->try_get<RelationshipComponent>(entity);
                        rel && rel->Parent.has_value()) {
                    Entity parentEntity = scene->FindEntityByUUID(*rel->Parent);
                    if (parentEntity)
                        dir = glm::normalize(glm::mat3(scene->GetWorldTransform(parentEntity)) * localDir);
                }

                GpuLightCPU& g = lc.Lights[count];
                g.Position   = glm::vec4(pos, static_cast<float>(light.Type));
                g.Direction  = glm::vec4(dir, light.Range);
                g.Color      = glm::vec4(light.Color, light.Intensity);
                g.SpotParams = glm::vec4(light.CosInner(), light.CosOuter(), 0.0f, 0.0f);

                // Record one shadow caster per light type (index into this g_Lights array).
                if (light.CastsShadows) {
                    if (light.Type == LightType::Directional && m_DirCaster.Index < 0) {
                        m_DirCaster.Index = count; m_DirCaster.Direction = dir; m_DirCaster.Bias = light.ShadowBias;
                    } else if (light.Type == LightType::Spot && m_SpotCaster.Index < 0) {
                        m_SpotCaster.Index = count; m_SpotCaster.Position = pos; m_SpotCaster.Direction = dir;
                        m_SpotCaster.Range = light.Range; m_SpotCaster.CosOuter = light.CosOuter(); m_SpotCaster.Bias = light.ShadowBias;
                    } else if (light.Type == LightType::Point && m_PointCaster.Index < 0) {
                        m_PointCaster.Index = count; m_PointCaster.Position = pos;
                        m_PointCaster.Range = light.Range; m_PointCaster.Bias = light.ShadowBias;
                    }
                }
                ++count;
            }
            lc.Count.x = count;
        }

        if (lc.Count.x != m_LastLightCount) {
            m_LastLightCount = lc.Count.x;
            ECHELON_LOG_INFO("Ray: gathered {} light(s) into g_Lights", lc.Count.x);
        }

        m_LightUBO->SetData(&lc, sizeof(lc));
    }

    void RayRenderer::EndScene() {}

    // ------------------------------------------------------------------
    // Scene-driven rendering via RenderGraph
    // ------------------------------------------------------------------

    void RayRenderer::RenderScene(const Ref<Scene>& scene) {
        if (!scene) return;

        EnsureUpToDate();

        m_RenderGraph.Update(scene, this, m_MaterialCache,
                             GetDefaultPipeline(), GetErrorPipeline(), GetScenePass());

        // Render this frame's shadow maps (dir/spot/point) into renderer-owned targets
        // before the main graph, and upload g_Shadows. Runs outside the pass graph since
        // the shadow-caster set is dynamic. No-op when no lights cast shadows.
        RenderShadowMaps();

        // Execute the pass graph. Each pass opens its own render pass + framebuffer;
        // the "forward" pass calls back into ExecuteDrawList() to record the draws.
        m_PassGraph.Execute(m_CommandBuffer);
    }

    // ------------------------------------------------------------------
    // Forward scene pass — record the sorted draw list into the currently
    // bound render pass (invoked by the pass graph's "forward" callback).
    // ------------------------------------------------------------------

    void RayRenderer::ExecuteDrawList(const PassContext& ctx) {
        // Keep the transparent bucket ordered back-to-front for the current camera (cheap
        // per-frame sort of an already-built list; the camera can move without a rebuild).
        const glm::vec3 camPos = glm::vec3(glm::inverse(m_ViewMatrix)[3]);
        m_RenderGraph.SortTransparent(camPos);

        // Override-shader pass: draw ALL scene geometry (opaque + transparent) with one
        // pipeline (a generic capability — depth prepass, object-id pass for picking,
        // normals, wireframe…), instead of each entity's material pipeline. The shader
        // reads g_Object.ObjectId (set per draw below) for any per-object work.
        const bool useOverride = ctx.Pass && !ctx.Pass->Shader.empty();
        if (useOverride) {
            Ref<Pipeline> pipe = GetOverridePipeline(ctx.Pass->Name, ctx.Pass->Shader);
            if (!pipe) return;
            m_CommandBuffer->BindPipeline(pipe);
            BindSystemConstants(pipe);   // g_Frame + g_Object at this shader's bindings
            const auto& shader = pipe->GetShader();

            auto drawOne = [&](const Ref<Buffer>& vb, const Ref<Buffer>& ib,
                               uint32_t vc, uint32_t ic, const glm::mat4& transform, uint32_t entityId) {
                m_CurrentObjectId = entityId;
                if (ib && ic > 0) DrawIndexed(vb, ib, shader, transform, ic);
                else              Draw(vb, shader, transform, vc);
            };

            for (const auto& group : m_RenderGraph.GetPipelineGroups())
                for (const auto& inst : group.Instances)
                    for (const auto& batch : inst.Batches)
                        for (size_t i = 0; i < batch.Transforms.size(); ++i)
                            drawOne(batch.VertexBuffer, batch.IndexBuffer, batch.VertexCount,
                                    batch.IndexCount, batch.Transforms[i],
                                    (i < batch.EntityIDs.size()) ? batch.EntityIDs[i] : 0u);

            for (const auto& td : m_RenderGraph.GetTransparentDraws())
                drawOne(td.VertexBuffer, td.IndexBuffer, td.VertexCount, td.IndexCount,
                        td.Transform, td.EntityID);
            return;
        }

        // Normal path (opaque): pipeline + system set bound ONCE per pipeline group; the
        // material descriptor set bound ONCE per instance group; only the per-object
        // transform (g_Object) updates per draw.
        for (const auto& group : m_RenderGraph.GetPipelineGroups()) {
            const auto& pipeline = group.PipelineRef ? group.PipelineRef : GetErrorPipeline();
            if (!pipeline) continue;

            m_CommandBuffer->BindPipeline(pipeline);
            BindSystemConstants(pipeline);   // g_Frame + g_Object at this shader's bindings
            const auto& shader = pipeline->GetShader();

            for (const auto& inst : group.Instances) {
                // This instance's material params/textures (null for the default pipeline).
                // Its bindings never collide with the system set (Slang assigns unique ones).
                if (inst.MaterialSet)
                    m_CommandBuffer->BindDescriptorSet(inst.MaterialSet, 1);

                for (const auto& batch : inst.Batches) {
                    for (size_t i = 0; i < batch.Transforms.size(); ++i) {
                        m_CurrentObjectId = (i < batch.EntityIDs.size()) ? batch.EntityIDs[i] : 0u;
                        const auto& transform = batch.Transforms[i];
                        if (batch.IndexBuffer && batch.IndexCount > 0)
                            DrawIndexed(batch.VertexBuffer, batch.IndexBuffer, shader, transform, batch.IndexCount);
                        else
                            Draw(batch.VertexBuffer, shader, transform, batch.VertexCount);
                    }
                }
            }
        }

        // Transparent bucket: drawn after opaque, back-to-front. Not grouped (depth order
        // wins), so bind pipeline/system + material set as they change along the sorted list.
        Pipeline*      lastPipe = nullptr;
        DescriptorSet* lastSet  = nullptr;
        Ref<Shader>    curShader;
        for (const auto& td : m_RenderGraph.GetTransparentDraws()) {
            const auto& pipeline = td.PipelineRef ? td.PipelineRef : GetErrorPipeline();
            if (!pipeline) continue;

            if (pipeline.get() != lastPipe) {
                m_CommandBuffer->BindPipeline(pipeline);
                BindSystemConstants(pipeline);
                curShader = pipeline->GetShader();
                lastPipe  = pipeline.get();
                lastSet   = nullptr;   // a new pipeline needs its set rebound
            }
            if (td.MaterialSet && td.MaterialSet.get() != lastSet) {
                m_CommandBuffer->BindDescriptorSet(td.MaterialSet, 1);
                lastSet = td.MaterialSet.get();
            }

            m_CurrentObjectId = td.EntityID;
            if (td.IndexBuffer && td.IndexCount > 0)
                DrawIndexed(td.VertexBuffer, td.IndexBuffer, curShader, td.Transform, td.IndexCount);
            else
                Draw(td.VertexBuffer, curShader, td.Transform, td.VertexCount);
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

        cmd.BindVertexBuffer(m_FullscreenVBO);   // POSITION-attributed fullscreen triangle
        cmd.Draw(3, 1, 0, 0);
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
        oc.ObjectId     = glm::uvec4(m_CurrentObjectId, 0u, 0u, 0u);
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
        oc.ObjectId     = glm::uvec4(m_CurrentObjectId, 0u, 0u, 0u);
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
