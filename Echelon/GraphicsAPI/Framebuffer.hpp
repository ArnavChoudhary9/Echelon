#pragma once

/**
 * @file Framebuffer.hpp
 * @brief Framebuffer interface and descriptor.
 *
 * A Framebuffer groups one or more color attachments and an optional depth
 * attachment into a single render target that a RenderPass can draw into.
 * Multiple framebuffers allow rendering the same scene to different targets
 * (shadow maps, G-buffers, post-process targets, headless off-screen, etc.).
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // TextureFormat

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    // Forward declarations
    class Texture;
    class RenderPass;

    // ================================================================
    // Framebuffer types
    // ================================================================

    /**
     * @brief Describes a single attachment slot within a framebuffer.
     *
     * An attachment may reference an existing texture (externally owned) or
     * request the framebuffer to create one automatically based on the
     * provided format and size.
     */
    struct FramebufferAttachment
    {
        Ref<Texture>  ExistingTexture = nullptr;   ///< Use an existing texture (may be null)
        TextureFormat Format          = TextureFormat::RGBA8_UNORM;
    };

    /**
     * @brief Descriptor for creating a framebuffer.
     *
     * Usage:
     * @code
     *     FramebufferDesc desc;
     *     desc.Width  = 1920;
     *     desc.Height = 1080;
     *     desc.ColorAttachments.push_back({ nullptr, TextureFormat::RGBA16_FLOAT });
     *     desc.ColorAttachments.push_back({ nullptr, TextureFormat::RGBA8_UNORM  });
     *     desc.DepthAttachment = { nullptr, TextureFormat::D32_FLOAT };
     *     desc.HasDepthAttachment = true;
     *     desc.CompatiblePass = myRenderPass;
     *     auto fb = device->CreateFramebuffer(desc);
     * @endcode
     */
    struct FramebufferDesc
    {
        uint32_t                            Width              = 0;
        uint32_t                            Height             = 0;
        std::vector<FramebufferAttachment>  ColorAttachments;
        FramebufferAttachment               DepthAttachment    = {};
        bool                                HasDepthAttachment = false;
        Ref<RenderPass>                     CompatiblePass     = nullptr;
        uint32_t                            Layers             = 1; ///< >1 for layered rendering (e.g. cubemap faces)
        std::string                         DebugName          = "";
    };

    // ================================================================
    // Framebuffer interface
    // ================================================================

    /**
     * @brief Abstract framebuffer — a collection of texture attachments that
     *        serve as render targets for a render pass.
     *
     * Framebuffers are created through Device::CreateFramebuffer().
     * CommandBuffer::BeginRenderPass() takes a Framebuffer to define where
     * the GPU will write.
     *
     * Multiple framebuffers enable:
     *   - Multi-pass rendering (shadow, G-buffer, lighting, post-process)
     *   - Headless / off-screen rendering
     *   - Render-to-texture effects
     *   - Resolution-independent passes (e.g. half-res bloom)
     */
    class Framebuffer
    {
    public:
        virtual ~Framebuffer() = default;

        /** @brief Width of the framebuffer in pixels. */
        virtual uint32_t GetWidth() const = 0;

        /** @brief Height of the framebuffer in pixels. */
        virtual uint32_t GetHeight() const = 0;

        /**
         * @brief Get the number of color attachments.
         * @return uint32_t Color attachment count.
         */
        virtual uint32_t GetColorAttachmentCount() const = 0;

        /**
         * @brief Get a color attachment texture by index.
         * @param index Attachment index (0-based).
         * @return Ref<Texture> The texture behind the attachment.
         */
        virtual Ref<Texture> GetColorAttachment(uint32_t index) const = 0;

        /**
         * @brief Get the depth attachment texture, if present.
         * @return Ref<Texture> The depth texture, or nullptr.
         */
        virtual Ref<Texture> GetDepthAttachment() const = 0;

        /**
         * @brief Query whether this framebuffer has a depth attachment.
         */
        virtual bool HasDepthAttachment() const = 0;

        /**
         * @brief Resize all attachments.
         *
         * Internally recreates the backing textures.  Existing texture
         * references obtained through GetColorAttachment / GetDepthAttachment
         * become invalid after a resize.
         *
         * @param width  New width in pixels.
         * @param height New height in pixels.
         */
        virtual void Resize(uint32_t width, uint32_t height) = 0;
    };

} // namespace Echelon
