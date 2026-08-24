#pragma once

/**
 * @file RendererAPI.hpp
 * @brief Pure-virtual renderer interface loaded as a plugin DLL.
 *
 * Best Practices:
 *  - This header is the **contract** between the engine and any renderer
 *    plugin.  Renderer DLLs include this header and implement the interface.
 *  - The engine never links against a specific renderer at compile time.
 *    Instead, the Renderer service dynamically loads the plugin library
 *    (e.g. libRay.so / Ray.dll) at runtime.
 *  - All types exchanged across the DLL boundary are engine types
 *    (Ref, Scope, std::string, glm types) — keep the ABI stable.
 *  - Renderers may maintain internal state (pipeline caches, GPU contexts).
 *    The engine only interacts through this interface.
 *  - To swap renderers, provide another plugin library that exports the same
 *    CreateRenderer factory and load it through the Renderer service.
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

    // Forward declarations — the contract only traffics in the Scene, GPU resource
    // handles the engine may create (Device/Buffer/Texture/Shader), and the neutral
    // pass-graph description used for injectable auxiliary passes. How a scene turns
    // into pipelines/passes is the renderer's own business.
    class Scene;
    class Device;
    class Buffer;
    class Texture;
    class Shader;
    struct RenderPipelineDesc;

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

        // ---- Scene-driven rendering ----

        /**
         * @brief Render all MeshComponent entities in the scene using the
         *        internal RenderGraph.
         *
         * The RenderGraph caches draw commands and only rebuilds when
         * entity meshes or transforms change.  For scenes that are
         * mostly static, this reduces per-frame CPU cost to O(1).
         *
         * @param scene The scene whose entities should be rendered.
         */
        virtual void RenderScene(const Ref<Scene>& scene) = 0;

        // ---- Draw commands ----

        /**
         * @brief Submit an indexed draw call.  The pipeline must already be
         *        bound via BindPipeline() before calling this.
         * @param vertexBuffer Vertex data.
         * @param indexBuffer  Index data.
         * @param shader       Shader used to upload per-draw uniforms (model/view/proj).
         * @param transform    Model matrix.
         * @param indexCount   Number of indices to draw (0 = use full buffer).
         */
        virtual void DrawIndexed(const Ref<Buffer>& vertexBuffer,
                                 const Ref<Buffer>& indexBuffer,
                                 const Ref<Shader>& shader,
                                 const glm::mat4& transform,
                                 uint32_t indexCount = 0) = 0;

        /**
         * @brief Submit a non-indexed draw call.  The pipeline must already be
         *        bound via BindPipeline() before calling this.
         * @param vertexBuffer Vertex data.
         * @param shader       Shader used to upload per-draw uniforms (model/view/proj).
         * @param transform    Model matrix.
         * @param vertexCount  Number of vertices.
         */
        virtual void Draw(const Ref<Buffer>& vertexBuffer,
                          const Ref<Shader>& shader,
                          const glm::mat4& transform,
                          uint32_t vertexCount) = 0;

        // ---- Viewport ----

        /**
         * @brief Notify the renderer that the viewport has been resized.
         * @param width  New width in pixels.
         * @param height New height in pixels.
         */
        virtual void OnResize(uint32_t width, uint32_t height) = 0;

        // ---- Editor viewport (render-to-texture) ----

        /**
         * @brief Route the final rendered image into an offscreen texture instead of
         *        the window's backbuffer.
         *
         * Used by editors that present the scene inside an ImGui viewport panel: with
         * this enabled the renderer draws the whole pass graph into an offscreen target
         * (sized by OnResize) and leaves the window backbuffer free for the UI. The
         * result is retrieved through GetViewportTexture(). Default: no-op (renders to
         * the window), so non-editor applications are unaffected.
         *
         * @param offscreen true to render into the offscreen viewport texture.
         */
        virtual void SetViewportTarget(bool /*offscreen*/) {}

        /**
         * @brief The color texture the scene was rendered into while offscreen viewport
         *        mode is enabled (see SetViewportTarget). null when disabled/unsupported.
         *        Its GetNativeHandle() can be passed to ImGui::Image.
         */
        virtual Ref<Texture> GetViewportTexture() const { return nullptr; }

        // ---- Auxiliary passes (generic pass-graph extension) ----

        /**
         * @brief Contribute extra resources + passes that are merged into the active
         *        pass graph on every (re)compile.
         *
         * A generic hook for a host (e.g. the editor) to add its own render passes —
         * a depth prepass, an object-id pass for picking, a normals/velocity buffer,
         * a debug overlay — without baking them into the project's authored pipeline.
         * The renderer appends @p aux.Resources / @p aux.Passes to whichever pipeline
         * it compiles. Pass an empty description to clear. Default: no-op.
         *
         * The results of an auxiliary pass are read back through ReadTargetPixel() or
         * sampled by later passes like any other resource. Nothing here is specific to
         * any one use case — passes are declared with the same RenderPipelineDesc data
         * as a project pipeline. A Graphics pass that declares a `Shader` draws the
         * whole scene with that single shader (e.g. an id shader), instead of the
         * per-material pipelines.
         */
        virtual void SetAuxiliaryPipeline(const RenderPipelineDesc& /*aux*/) {}

        /**
         * @brief Read back a single texel from a named pass-graph color resource.
         *
         * Generic render-target readback (used e.g. by editor GPU picking to read the
         * object id under the cursor). @p x / @p y are in the resource's pixel space
         * with the backend's native origin (OpenGL: bottom-left). @p out must hold at
         * least one texel. Returns false if the resource is unknown / unsupported.
         */
        virtual bool ReadTargetPixel(const std::string& /*resource*/, uint32_t /*x*/, uint32_t /*y*/,
                                     void* /*out*/, uint32_t /*outSize*/) { return false; }

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

        // NOTE: how a scene turns into GPU pipelines — the default/error/material
        // pipelines and the scene ("forward") pass — is entirely the renderer's
        // internal business and is deliberately NOT part of this contract. The
        // renderer builds and caches those itself (see Ray's material cache).

        /**
         * @brief Filename of the renderer's default shader (e.g. "Flat.slang").
         *
         * The engine uses this to register the "DefaultMaterial" primitive without
         * knowing the renderer's internal shader layout.  The file must be present
         * in the standard shader search path next to the executable.
         */
        virtual const char* GetDefaultShaderName() const = 0;

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
