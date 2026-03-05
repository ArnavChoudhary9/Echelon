#pragma once

/**
 * @file RayRenderer.hpp
 * @brief "Ray" PBR Renderer — the default Echelon renderer plugin.
 *
 * This is a concrete RendererAPI implementation compiled as Renderer.dll.
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
#include "Echelon/GraphicsAPI/GraphicsAPI.hpp"
#include "Echelon/GraphicsAPI/Device.hpp"

namespace Echelon {

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
                         const Ref<Pipeline>& pipeline,
                         const glm::mat4& transform,
                         uint32_t indexCount = 0) override;

        void Draw(const Ref<Buffer>& vertexBuffer,
                  const Ref<Pipeline>& pipeline,
                  const glm::mat4& transform,
                  uint32_t vertexCount) override;

        // ---- Viewport ----
        void OnResize(uint32_t width, uint32_t height) override;

        // ---- VSync ----
        void SetVSync(bool enabled) override;
        bool IsVSync() const override;

        // ---- Resource access ----
        Ref<Device>   GetDevice() const override          { return m_Device; }
        Ref<Pipeline> GetDefaultPipeline() const override { return m_FlatPipeline; }

        // ---- Queries ----
        RendererInfo GetInfo() const override;
        RenderStats  GetStats() const override;

    private:
        void CreateDefaultResources();

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
        Ref<RenderPass>        m_DefaultRenderPass;
        Ref<Swapchain>         m_Swapchain;

        // ---- Default shaders & pipeline ----
        Ref<Shader>   m_BasicShader;
        Ref<Shader>   m_FlatShader;
        Ref<Pipeline> m_FlatPipeline;

        // ---- Render graph (caches draw commands across frames) ----
        RenderGraph   m_RenderGraph;
    };

} // namespace Echelon
