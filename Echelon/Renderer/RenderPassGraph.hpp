#pragma once

/**
 * @file RenderPassGraph.hpp
 * @brief Runtime multipass render graph — compiles a RenderPipelineDesc into GPU
 *        RenderPass / Framebuffer objects, resolves execution order, manages
 *        attachment lifetime, and executes the passes each frame.
 *
 * This sits ABOVE the draw-list RenderGraph (RenderGraph.hpp): the draw-list
 * graph answers "what to draw and how to batch it", this answers "where to draw,
 * in what order, into which attachments". A renderer supplies the per-pass work
 * through execute callbacks keyed by pass name; the graph owns sequencing,
 * framebuffers, clears, and viewport.
 *
 * Design goals:
 *  - Data-driven: the whole structure comes from a RenderPipelineDesc (usually a
 *    RenderPipelineAsset authored in YAML), so passes are never hardcoded.
 *  - Backend-agnostic: only talks to the GraphicsAPI abstraction (Device,
 *    RenderPass, Framebuffer, CommandBuffer). No GL/Vulkan code here.
 *  - Hot-reloadable: CompileFrom() can be called again with a fresh description;
 *    registered execute callbacks are preserved across recompiles.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/Asset/RenderPipeline/RenderPipelineDesc.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Echelon {

    // Forward declarations — the graph only touches the GraphicsAPI abstraction.
    class Device;
    class RenderPass;
    class Framebuffer;
    class Texture;
    class CommandBuffer;

    /**
     * @brief Everything a per-pass execute callback needs at record time.
     *
     * `InputTextures` is parallel to `Pass.Inputs`: entry i is the resolved
     * texture for `Pass.Inputs[i]` (a color/depth attachment produced by an
     * earlier pass). The render pass scope (BeginRenderPass/EndRenderPass) is
     * already open for Graphics/Fullscreen passes; Compute callbacks run with
     * no render pass bound.
     */
    struct PassContext {
        const PassDesc*                Pass   = nullptr;
        Ref<Framebuffer>               Target;              ///< null => backbuffer
        uint32_t                       Width  = 0;
        uint32_t                       Height = 0;
        std::vector<Ref<Texture>>      InputTextures;
    };

    class RenderPassGraph {
    public:
        using ExecuteFn = std::function<void(CommandBuffer&, const PassContext&)>;

        RenderPassGraph() = default;
        ~RenderPassGraph() = default;

        /**
         * @brief Compile a description into GPU objects and a resolved order.
         *
         * Safe to call again to hot-reload a new description; previously
         * registered execute callbacks are kept. Returns false (and leaves the
         * graph empty) if the description is invalid (missing resource, cycle,
         * duplicate producer) — the caller should fall back to a known-good desc.
         */
        bool CompileFrom(const RenderPipelineDesc& desc,
                         const Ref<Device>& device,
                         uint32_t width, uint32_t height);

        /** @brief Recreate swapchain-relative attachments for a new viewport size. */
        void Resize(uint32_t width, uint32_t height);

        /** @brief Register the work performed by a specific pass, keyed by pass name. */
        void SetExecuteCallback(const std::string& passName, ExecuteFn fn);

        /**
         * @brief Register a fallback handler for every pass of a given type that has
         *        no name-specific callback (e.g. a generic fullscreen/compute handler).
         */
        void SetDefaultCallback(PassType type, ExecuteFn fn);

        /** @brief Run every enabled pass in resolved order into `cmd`. */
        void Execute(const Ref<CommandBuffer>& cmd);

        /** @brief The GPU RenderPass created for a pass (for building compatible pipelines). */
        Ref<RenderPass> GetRenderPass(const std::string& passName) const;

        /** @brief The current texture backing a managed resource (editor / debug). */
        Ref<Texture> GetOutput(const std::string& resourceName) const;

        /**
         * @brief Redirect any pass that writes `$backbuffer` into an offscreen
         *        framebuffer instead of the window's default framebuffer.
         *
         * Used to present the render inside an editor viewport. The offscreen target
         * mirrors the backbuffer pass's attachment layout at the graph's current size
         * and is rebuilt on CompileFrom/Resize. When disabled, backbuffer passes draw
         * to the window default framebuffer as before. No-op if there is no backbuffer
         * pass. Retrieve the result with GetOffscreenColor().
         */
        void SetOffscreenTarget(bool enabled);

        /** @brief Whether offscreen backbuffer redirection is currently enabled. */
        bool IsOffscreenTarget() const { return m_OffscreenEnabled; }

        /** @brief Color texture of the offscreen backbuffer target (null if disabled/none). */
        Ref<Texture> GetOffscreenColor() const;

        /** @brief True if a valid pipeline is currently compiled. */
        bool IsValid() const { return !m_Order.empty(); }

        uint32_t GetWidth()  const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

    private:
        // Where an input resource's texture comes from within a producing pass.
        struct AttachmentLocation {
            size_t   PassIndex = 0;      ///< index into m_Order
            bool     IsDepth   = false;
            uint32_t ColorIndex = 0;     ///< color attachment index when !IsDepth
        };

        struct CompiledPass {
            PassDesc                        Desc;         ///< copy (owns its strings)
            Ref<RenderPass>                 Pass;         ///< null for compute
            Ref<Framebuffer>                FB;           ///< null => backbuffer / compute
            bool                            Backbuffer = false;
            uint32_t                        Width  = 0;
            uint32_t                        Height = 0;
            std::vector<AttachmentLocation> Inputs;       ///< parallel to Desc.Inputs
        };

        void ComputePassSize(const PassDesc& pass, uint32_t& outW, uint32_t& outH) const;
        void ResolveResourceSize(const std::string& resource, uint32_t& outW, uint32_t& outH) const;
        bool CreateFramebuffers();   ///< (re)build framebuffers for all compiled passes at m_Width/m_Height
        void CreateComputeResources(); ///< (re)build standalone storage textures written by compute passes
        void BuildOffscreenTarget(); ///< (re)create the offscreen backbuffer FBO (editor viewport) at m_Width/m_Height

        Ref<Device>                                m_Device;
        RenderPipelineDesc                         m_Desc;
        uint32_t                                   m_Width  = 0;
        uint32_t                                   m_Height = 0;

        std::vector<CompiledPass>                  m_Order;                 ///< resolved execution order
        std::unordered_map<std::string, size_t>    m_PassIndexByName;       ///< name -> index in m_Order
        std::unordered_map<std::string, AttachmentLocation> m_ResourceProducers; ///< resource -> producing attachment
        std::unordered_map<std::string, Ref<Texture>> m_ComputeResources;   ///< standalone storage textures written by compute passes
        std::unordered_map<std::string, ExecuteFn> m_Callbacks;            ///< preserved across recompiles
        ExecuteFn                                  m_DefaultCallbacks[3]; ///< indexed by PassType

        // ---- Offscreen backbuffer redirection (editor viewport) ----
        bool             m_OffscreenEnabled = false;  ///< redirect $backbuffer passes into m_OffscreenFB
        Ref<Framebuffer> m_OffscreenFB;               ///< offscreen target mirroring the backbuffer pass layout
    };

} // namespace Echelon
