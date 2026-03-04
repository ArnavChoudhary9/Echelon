#pragma once

/**
 * @file RayRenderer.hpp
 * @brief "Ray" PBR Renderer — the default Echelon renderer plugin.
 *
 * This is a concrete RendererAPI implementation compiled as Renderer.dll.
 * It provides PBR (physically-based rendering) capabilities.
 *
 * Best Practices:
 *  - All GPU work is encapsulated here; the engine only knows about RendererAPI.
 *  - Internal state (GPU context, pipeline cache, frame data) is private.
 *  - The factory functions (CreateRenderer / DestroyRenderer) are the only
 *    symbols exported from this DLL.
 */

#include "Echelon/Renderer/RendererAPI.hpp"

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

        // ---- Queries ----
        RendererInfo GetInfo() const override;
        RenderStats  GetStats() const override;

    private:
        bool m_Initialized = false;

        uint32_t m_ViewportWidth  = 0;
        uint32_t m_ViewportHeight = 0;

        glm::mat4 m_ViewMatrix       = glm::mat4(1.0f);
        glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);

        RenderStats m_Stats;
    };

} // namespace Echelon
