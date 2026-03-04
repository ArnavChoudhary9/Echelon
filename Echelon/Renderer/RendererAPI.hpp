#pragma once

/**
 * @file RendererAPI.hpp
 * @brief Pure-virtual renderer interface loaded as a plugin DLL.
 *
 * Best Practices:
 *  - This header is the **contract** between the engine and any renderer
 *    plugin.  Renderer DLLs include this header and implement the interface.
 *  - The engine never links against a specific renderer at compile time.
 *    Instead, RendererLoader dynamically loads Renderer.dll at runtime.
 *  - All types exchanged across the DLL boundary are engine types
 *    (Ref, Scope, std::string, glm types) — keep the ABI stable.
 *  - Renderers may maintain internal state (pipeline caches, GPU contexts).
 *    The engine only interacts through this interface.
 *  - To swap renderers, replace Renderer.dll with another implementation
 *    that exports the same CreateRenderer factory.
 *
 * Plugin authors: implement every pure-virtual below and export the factory:
 * @code
 *     extern "C" RENDERER_EXPORT Echelon::RendererAPI* CreateRenderer();
 *     extern "C" RENDERER_EXPORT void DestroyRenderer(Echelon::RendererAPI*);
 * @endcode
 */

#include "Echelon/Core/Base.hpp"

#include "glm/glm.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    // Forward declarations — renderer works with engine graphics abstractions
    class Scene;
    class Device;
    class Buffer;
    class Texture;
    class Shader;
    class Pipeline;
    class RenderPass;
    class Framebuffer;
    class CommandBuffer;

    // ================================================================
    // Renderer capability / info
    // ================================================================

    /**
     * @brief Metadata returned by a renderer plugin so the engine can
     *        identify and log it.
     */
    struct RendererInfo {
        std::string Name;       ///< Human-readable name (e.g. "Ray PBR Renderer")
        std::string Version;    ///< Semantic version (e.g. "1.0.0")
        std::string Author;     ///< Plugin author / organisation
    };

    // ================================================================
    // Clear values
    // ================================================================

    struct ClearValue {
        glm::vec4 Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        float     Depth   = 1.0f;
        uint32_t  Stencil = 0;
    };

    // ================================================================
    // Render statistics (optional, for profiling / debug overlays)
    // ================================================================

    struct RenderStats {
        uint32_t DrawCalls      = 0;
        uint32_t TriangleCount  = 0;
        uint32_t TextureBinds   = 0;
        uint32_t ShaderSwitches = 0;
        float    FrameTimeMs    = 0.0f;
    };

    // ================================================================
    // RendererAPI — the plugin interface
    // ================================================================

    /**
     * @brief Abstract renderer interface implemented by plugin DLLs.
     *
     * The engine calls these methods in a well-defined order each frame:
     * @code
     *     renderer->BeginFrame(camera, clearValue);
     *       renderer->BeginScene(scene);
     *         // engine issues draw commands
     *         renderer->DrawMesh(vb, ib, pipeline, transform);
     *       renderer->EndScene();
     *     renderer->EndFrame();
     * @endcode
     */
    class RendererAPI {
    public:
        virtual ~RendererAPI() = default;

        // ---- Lifecycle ----

        /**
         * @brief One-time initialisation.  Called after the DLL is loaded
         *        and the renderer instance is created.
         * @param windowHandle Opaque native window handle (HWND on Windows).
         * @param width  Initial framebuffer width.
         * @param height Initial framebuffer height.
         * @return true on success.
         */
        virtual bool Init(void* windowHandle, uint32_t width, uint32_t height) = 0;

        /**
         * @brief One-time shutdown.  Called before the DLL is unloaded.
         */
        virtual void Shutdown() = 0;

        // ---- Frame lifecycle ----

        /**
         * @brief Begin a new frame.
         * @param viewMatrix       Camera view matrix.
         * @param projectionMatrix Camera projection matrix.
         * @param clearValue       Clear colour / depth / stencil.
         */
        virtual void BeginFrame(const glm::mat4& viewMatrix,
                                const glm::mat4& projectionMatrix,
                                const ClearValue& clearValue) = 0;

        /**
         * @brief End and present the current frame.
         */
        virtual void EndFrame() = 0;

        // ---- Scene scope ----

        /**
         * @brief Prepare to render a scene (bind global UBOs, lights, etc.).
         * @param scene The scene to render.
         */
        virtual void BeginScene(const Ref<Scene>& scene) = 0;

        /**
         * @brief Finalise scene rendering (post-process, resolve MSAA, etc.).
         */
        virtual void EndScene() = 0;

        // ---- Draw commands ----

        /**
         * @brief Submit an indexed draw call.
         * @param vertexBuffer Vertex data.
         * @param indexBuffer  Index data.
         * @param pipeline     Graphics pipeline (shader + state).
         * @param transform    Model matrix.
         * @param indexCount   Number of indices to draw (0 = use full buffer).
         */
        virtual void DrawIndexed(const Ref<Buffer>& vertexBuffer,
                                 const Ref<Buffer>& indexBuffer,
                                 const Ref<Pipeline>& pipeline,
                                 const glm::mat4& transform,
                                 uint32_t indexCount = 0) = 0;

        /**
         * @brief Submit a non-indexed draw call.
         * @param vertexBuffer Vertex data.
         * @param pipeline     Graphics pipeline (shader + state).
         * @param transform    Model matrix.
         * @param vertexCount  Number of vertices.
         */
        virtual void Draw(const Ref<Buffer>& vertexBuffer,
                          const Ref<Pipeline>& pipeline,
                          const glm::mat4& transform,
                          uint32_t vertexCount) = 0;

        // ---- Viewport ----

        /**
         * @brief Notify the renderer that the viewport has been resized.
         * @param width  New width in pixels.
         * @param height New height in pixels.
         */
        virtual void OnResize(uint32_t width, uint32_t height) = 0;

        // ---- VSync ----

        /**
         * @brief Enable or disable vertical synchronisation.
         * @param enabled true to enable VSync.
         */
        virtual void SetVSync(bool enabled) = 0;

        /**
         * @brief Query whether VSync is currently enabled.
         */
        virtual bool IsVSync() const = 0;

        // ---- Resource access ----

        /**
         * @brief Get the GPU device owned by this renderer, so callers
         *        can create buffers, textures, shaders, etc.
         * @return Ref<Device> or nullptr if not initialised.
         */
        virtual Ref<Device> GetDevice() const = 0;

        /**
         * @brief Get the renderer's default (flat colour) pipeline.
         *
         * Convenience accessor so simple geometry can be drawn without
         * the caller creating a pipeline from scratch.
         * @return Ref<Pipeline> or nullptr.
         */
        virtual Ref<Pipeline> GetDefaultPipeline() const = 0;

        // ---- Queries ----

        /**
         * @brief Get descriptive information about this renderer.
         */
        virtual RendererInfo GetInfo() const = 0;

        /**
         * @brief Get statistics for the last completed frame.
         */
        virtual RenderStats GetStats() const = 0;
    };

} // namespace Echelon

// ====================================================================
// DLL export / import macros for renderer plugins
// ====================================================================

#if defined(_WIN32) || defined(_WIN64)
    #define RENDERER_EXPORT __declspec(dllexport)
    #define RENDERER_IMPORT __declspec(dllimport)
#else
    #define RENDERER_EXPORT __attribute__((visibility("default")))
    #define RENDERER_IMPORT
#endif

// ====================================================================
// Factory function signature expected by the engine
// ====================================================================
// Every renderer plugin DLL must export these two symbols:
//
//   extern "C" RENDERER_EXPORT Echelon::RendererAPI* CreateRenderer();
//   extern "C" RENDERER_EXPORT void DestroyRenderer(Echelon::RendererAPI*);
//
// ====================================================================
