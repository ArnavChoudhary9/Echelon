#include "RayRenderer.hpp"

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

    bool RayRenderer::Init(void* /*windowHandle*/, uint32_t width, uint32_t height) {
        m_ViewportWidth  = width;
        m_ViewportHeight = height;
        m_Initialized    = true;
        m_Stats          = {};

        // TODO: Initialize GPU context, create swapchain, compile default shaders, etc.

        return true;
    }

    void RayRenderer::Shutdown() {
        // TODO: Release GPU resources, destroy context.
        m_Initialized = false;
    }

    // ------------------------------------------------------------------
    // Frame lifecycle
    // ------------------------------------------------------------------

    void RayRenderer::BeginFrame(const glm::mat4& viewMatrix,
                                  const glm::mat4& projectionMatrix,
                                  const ClearValue& /*clearValue*/) {
        m_ViewMatrix       = viewMatrix;
        m_ProjectionMatrix = projectionMatrix;
        m_Stats            = {};  // Reset per-frame stats

        // TODO: Acquire next swapchain image, begin command buffer recording,
        //       apply clear values to the framebuffer.
    }

    void RayRenderer::EndFrame() {
        // TODO: End command buffer, submit to queue, present swapchain image.
    }

    // ------------------------------------------------------------------
    // Scene scope
    // ------------------------------------------------------------------

    void RayRenderer::BeginScene(const Ref<Scene>& /*scene*/) {
        // TODO: Upload scene-global data (lights, environment maps) to GPU.
    }

    void RayRenderer::EndScene() {
        // TODO: Post-processing passes (bloom, tone-mapping, FXAA, etc.).
    }

    // ------------------------------------------------------------------
    // Draw commands
    // ------------------------------------------------------------------

    void RayRenderer::DrawIndexed(const Ref<Buffer>& /*vertexBuffer*/,
                                   const Ref<Buffer>& /*indexBuffer*/,
                                   const Ref<Pipeline>& /*pipeline*/,
                                   const glm::mat4& /*transform*/,
                                   uint32_t /*indexCount*/) {
        m_Stats.DrawCalls++;
        // TODO: Bind pipeline, vertex/index buffers, push transform, issue draw call.
    }

    void RayRenderer::Draw(const Ref<Buffer>& /*vertexBuffer*/,
                            const Ref<Pipeline>& /*pipeline*/,
                            const glm::mat4& /*transform*/,
                            uint32_t /*vertexCount*/) {
        m_Stats.DrawCalls++;
        // TODO: Bind pipeline, vertex buffer, push transform, issue non-indexed draw call.
    }

    // ------------------------------------------------------------------
    // Viewport
    // ------------------------------------------------------------------

    void RayRenderer::OnResize(uint32_t width, uint32_t height) {
        m_ViewportWidth  = width;
        m_ViewportHeight = height;
        // TODO: Recreate swapchain / framebuffers to match new dimensions.
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
