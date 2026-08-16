#pragma once

/**
 * @file RenderPipelineDesc.hpp
 * @brief Plain-data description of a multipass render pipeline (the "pass graph").
 *
 * This is the payload of a RenderPipelineAsset (authored in YAML, see
 * RenderPipelineImporter) and the input to the runtime RenderPassGraph
 * (Echelon/Renderer/RenderPassGraph.hpp), which compiles it into GPU
 * RenderPass / Framebuffer objects.
 *
 * It carries NO GPU objects and depends only on the graphics enums — so it can
 * be constructed in code (for the fallback graph / unit tests) or parsed from an
 * asset without pulling in the renderer.
 */

#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // TextureFormat
#include "Echelon/GraphicsAPI/RenderPass.hpp"      // LoadOp, StoreOp, ClearColor, ClearDepthStencil

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Echelon {

    /**
     * @brief Reserved resource name that maps to the swapchain's default
     *        framebuffer (a null Framebuffer at BeginRenderPass time).
     */
    inline constexpr const char* kBackbufferResource = "$backbuffer";

    /**
     * @brief What kind of work a pass performs.
     *
     * Passes fall into two categories:
     *  - **Render passes** rasterize into a framebuffer: `Graphics` (the scene draw
     *    list) and `Fullscreen` (a single fullscreen triangle, for post-process).
     *    These open a RenderPass + Framebuffer scope; `Samples > 1` makes them MSAA.
     *  - **Compute passes** (`Compute`) run a compute shader with no framebuffer,
     *    reading/writing framebuffer attachments as storage images (see PassInput
     *    `AsStorageImage`) and dispatching. No render-pass scope, no MSAA.
     */
    enum class PassType : uint8_t {
        Graphics = 0,   ///< Render pass: draws the scene draw-list into color/depth attachments.
        Fullscreen,     ///< Render pass: a single fullscreen triangle (post-process).
        Compute         ///< Compute pass: a compute dispatch operating on attachments (no framebuffer).
    };

    /**
     * @brief How a managed resource's size is derived.
     */
    enum class ResourceSizePolicy : uint8_t {
        SwapchainRelative = 0,   ///< round(viewport * Scale)
        Fixed                    ///< explicit Width x Height (e.g. shadow maps)
    };

    /**
     * @brief A graph-managed logical texture (render target / sampleable attachment).
     *
     * The physical texture is auto-created by the owning pass's Framebuffer and
     * is sampleable in later passes.  The reserved name "$backbuffer" is implicit
     * and must NOT be declared here.
     */
    struct ResourceDesc {
        std::string        Name;
        TextureFormat      Format     = TextureFormat::RGBA8_UNORM;
        ResourceSizePolicy SizePolicy = ResourceSizePolicy::SwapchainRelative;
        float              Scale      = 1.0f;   ///< used when SwapchainRelative
        uint32_t           Width      = 0;      ///< used when Fixed
        uint32_t           Height     = 0;      ///< used when Fixed
    };

    /**
     * @brief A resource written by a pass, with its load/store/clear behaviour.
     */
    struct AttachmentRef {
        std::string       Resource;
        LoadOp            Load       = LoadOp::Clear;
        StoreOp           Store      = StoreOp::Store;
        ClearColor        ColorClear = {};   ///< used for color attachments
        ClearDepthStencil DepthClear = {};   ///< used for the depth attachment
    };

    /**
     * @brief A resource read by a pass (sampled texture, or storage image for compute).
     */
    struct PassInput {
        std::string Resource;
        uint32_t    Binding        = 0;
        bool        AsStorageImage = false;   ///< compute image load/store rather than sampling
    };

    /**
     * @brief A single pass declaration.
     */
    struct PassDesc {
        std::string                  Name;
        PassType                     Type = PassType::Graphics;

        std::vector<AttachmentRef>   ColorOutputs;   ///< color targets (may reference $backbuffer)
        std::optional<AttachmentRef> DepthOutput;    ///< optional depth/stencil target

        uint32_t                     Samples = 1;    ///< MSAA sample count for render passes (>1 renders multisampled + auto-resolves)

        std::vector<PassInput>       Inputs;         ///< resources produced by earlier passes

        std::string                  Shader;         ///< fullscreen / compute shader asset name
        uint32_t                     GroupCountX = 0;///< compute dispatch group counts (0 => derive from target)
        uint32_t                     GroupCountY = 0;
        uint32_t                     GroupCountZ = 0;

        bool                         Enabled = true; ///< disabled passes are dropped at compile
    };

    /**
     * @brief A complete render pipeline: managed resources + passes.
     *
     * Execution order is NOT authored here — the RenderPassGraph resolves it from
     * each pass's input/output resource dependencies (declaration order only breaks
     * ties between independent passes).
     */
    struct RenderPipelineDesc {
        std::vector<ResourceDesc> Resources;
        std::vector<PassDesc>     Passes;
    };

} // namespace Echelon
