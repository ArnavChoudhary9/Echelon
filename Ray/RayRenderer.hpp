#pragma once

/**
 * @file RayRenderer.hpp
 * @brief "Ray" PBR Renderer — the default Echelon renderer plugin.
 *
 * This is a concrete RendererAPI implementation compiled as the "Ray" plugin
 * (libRay.so / Ray.dll / libRay.dylib).
 * It uses the engine's GraphicsAPI abstraction layer — no backend-specific
 * code lives here.
 *
 * Best Practices:
 *  - All GPU work is encapsulated here; the engine only knows about RendererAPI.
 *  - Internal state (GPU context, pipeline cache, frame data) is private.
 *  - The factory functions (CreateRenderer / DestroyRenderer) are the only
 *    symbols exported from this DLL.
 */

#include "Echelon/Renderer/RendererAPI.hpp"
#include "Echelon/Renderer/RenderGraph.hpp"
#include "Echelon/Renderer/RenderPassGraph.hpp"
#include "Echelon/GraphicsAPI/GraphicsAPI.hpp"
#include "Echelon/GraphicsAPI/Device.hpp"
#include "Echelon/Asset/Shader/ShaderAsset.hpp"

#include <string>
#include <unordered_map>

namespace Echelon {

    class RenderPipelineAsset; // fwd — the project's authored pass graph (data)

    class RayRenderer : public RendererAPI {
    public:
        RayRenderer();
        ~RayRenderer() override;

        // ---- Lifecycle ----
        bool Init(void* windowHandle, uint32_t width, uint32_t height) override;
        void Shutdown() override;

        // ---- Frame lifecycle ----
        void BeginFrame(const glm::mat4& viewMatrix,
                        const glm::mat4& projectionMatrix,
                        const ClearValue& clearValue) override;
        void EndFrame() override;

        // ---- Scene scope ----
        void BeginScene(const Ref<Scene>& scene) override;
        void EndScene() override;

        // ---- Scene-driven rendering ----
        void RenderScene(const Ref<Scene>& scene) override;

        // ---- Draw commands ----
        void DrawIndexed(const Ref<Buffer>& vertexBuffer,
                         const Ref<Buffer>& indexBuffer,
                         const Ref<Shader>& shader,
                         const glm::mat4& transform,
                         uint32_t indexCount = 0) override;

        void Draw(const Ref<Buffer>& vertexBuffer,
                  const Ref<Shader>& shader,
                  const glm::mat4& transform,
                  uint32_t vertexCount) override;

        // ---- Viewport ----
        void OnResize(uint32_t width, uint32_t height) override;

        // ---- VSync ----
        void SetVSync(bool enabled) override;
        bool IsVSync() const override;

        // ---- Resource access ----
        Ref<Device>   GetDevice()          const override { return m_Device; }
        Ref<Pipeline> GetDefaultPipeline() const override { return m_FlatPipeline; }
        Ref<Pipeline> GetErrorPipeline()   const override { return m_ErrorPipeline ? m_ErrorPipeline : m_FlatPipeline; }
        Ref<RenderPass> GetScenePass()     const override { return m_PassGraph.GetRenderPass("forward"); }
        const char*   GetDefaultShaderName() const override { return "Flat.slang"; }

        // ---- Queries ----
        RendererInfo GetInfo() const override;
        RenderStats  GetStats() const override;

    private:
        void CreateDefaultResources();

        /** @brief Load a shader asset and ensure its GPU program exists (renderer is not yet active during Init). */
        Ref<ShaderAsset> LoadShaderAsset(const std::string& name);

        /** @brief Build the default (Flat) + error (pink) pipelines from their shaders' reflection. */
        void BuildDefaultPipeline();

        /** @brief Rebuild GPU resources if a shader/material/pipeline was hot-reloaded (epoch bumped). */
        void EnsureUpToDate();

        /** @brief Resolve the project's `.ehpipeline` asset (if any) and compile the pass graph from it. */
        bool TryLoadPipelineAsset();

        /** @brief (Re)compile the pass graph from the pipeline asset, falling back to the built-in default. */
        bool RecompilePassGraph();

        /** @brief Bind g_Frame / g_Object (resolved by name) for the given pipeline's shader. */
        void BindSystemConstants(const Ref<Pipeline>& pipeline);

        /** @brief Record the sorted draw list into the currently-bound render pass (the graphics-pass handler). */
        void ExecuteDrawList();

        /** @brief Default handler for Fullscreen passes: bind the post pipeline + inputs, draw a fullscreen triangle. */
        void ExecuteFullscreenPass(CommandBuffer& cmd, const PassContext& ctx);

        /** @brief Get/build+cache the post pipeline for a fullscreen pass (compatible with its RenderPass). */
        Ref<Pipeline> GetFullscreenPipeline(const std::string& passName, const std::string& shaderName);

        /** @brief Default handler for Compute passes: bind compute pipeline + resources, dispatch. */
        void ExecuteComputePass(CommandBuffer& cmd, const PassContext& ctx);

        /** @brief Get/build+cache the compute pipeline for a compute pass. */
        Ref<ComputePipeline> GetComputePipeline(const std::string& passName, const std::string& shaderName);

        bool m_Initialized = false;

        uint32_t m_ViewportWidth  = 0;
        uint32_t m_ViewportHeight = 0;

        glm::mat4 m_ViewMatrix       = glm::mat4(1.0f);
        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);

        RenderStats m_Stats;

        // ---- Graphics API resources ----
        Scope<GraphicsAPI>     m_GraphicsAPI;
        Ref<Device>            m_Device;
        Ref<CommandBuffer>     m_CommandBuffer;
        Ref<Swapchain>         m_Swapchain;

        // ---- Default + error shader assets & pipelines (reflection-driven) ----
        Ref<ShaderAsset> m_FlatShaderAsset;
        Ref<Pipeline>    m_FlatPipeline;
        Ref<ShaderAsset> m_ErrorShaderAsset;  ///< pink "something wrong" shader
        Ref<Pipeline>    m_ErrorPipeline;     ///< renderer's own fallback material

        // ---- System constant buffers (the fixed shader ABI: g_Frame / g_Object / g_Lights) ----
        Ref<Buffer>              m_FrameUBO;    ///< FrameConstants — written once per frame.
        Ref<Buffer>              m_ObjectUBO;   ///< ObjectConstants — rewritten per draw.
        Ref<Buffer>              m_LightUBO;    ///< LightConstants — gathered once per scene (lighting scaffold).
        int                      m_LastLightCount = -1;  ///< diagnostic: log when the gathered light count changes
        Ref<DescriptorSetLayout> m_SystemLayout;

        // One system descriptor set per shader — g_Frame/g_Object bindings are
        // assigned per-shader, so a shared set would leave stale bindings that could
        // clobber a material's own binding. Cleared when the asset epoch changes.
        std::unordered_map<Shader*, Ref<DescriptorSet>> m_SystemSets;

        uint64_t m_LastAssetEpoch = 0;   ///< AssetManager epoch at last pipeline (re)build.

        // ---- Render graph (caches draw commands across frames) ----
        RenderGraph   m_RenderGraph;

        // ---- Multipass pass graph (named passes, attachments, execution order) ----
        RenderPassGraph m_PassGraph;

        // ---- Render pipeline asset (project-authored pass graph; hot-reloadable) ----
        Ref<RenderPipelineAsset> m_PipelineAsset;             ///< null → built-in default graph
        bool                     m_PipelineResolved = false;  ///< true once a load has been attempted with an active project
        std::string              m_PipelinePath = "Pipelines/Forward.ehpipeline"; ///< project-relative

        // ---- Fullscreen / post-process passes ----
        Ref<Sampler>             m_LinearSampler;      ///< clamp+linear sampler for sampling attachments
        Ref<DescriptorSetLayout> m_FullscreenLayout;   ///< generic sampled-texture layout (GL ignores contents)
        std::unordered_map<std::string, Ref<ShaderAsset>>   m_FullscreenShaders;   ///< by shader name (fullscreen + compute)
        std::unordered_map<std::string, Ref<Pipeline>>      m_FullscreenPipelines; ///< by pass name
        std::unordered_map<std::string, Ref<DescriptorSet>> m_FullscreenSets;      ///< by pass name (fullscreen + compute)
        std::unordered_map<std::string, Ref<ComputePipeline>> m_ComputePipelines;  ///< by pass name
    };

} // namespace Echelon
