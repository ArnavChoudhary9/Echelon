#pragma once

/**
 * @file Swapchain.hpp
 * @brief Swapchain interface and swapchain-related types including headless
 *        rendering support.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // TextureFormat

#include <cstdint>

namespace Echelon {

    // Forward declarations
    class Texture;
    class Framebuffer;

    // ================================================================
    // Swapchain types
    // ================================================================

    /**
     * @brief Descriptor for creating a swapchain.
     *
     * Set @c Headless to true for off-screen rendering (compute-only,
     * server-side rendering, tests).  When headless, @c NativeWindow may be
     * nullptr and no on-screen presentation will occur.
     */
    struct SwapchainDesc
    {
        uint32_t      Width        = 1280;
        uint32_t      Height       = 720;
        TextureFormat Format       = TextureFormat::BGRA8_SRGB;
        bool          VSync        = true;
        void*         NativeWindow = nullptr;   ///< May be null for headless
        bool          Headless     = false;     ///< True = off-screen only
        uint32_t      BufferCount  = 2;         ///< Double / triple buffering
    };

    // ================================================================
    // Swapchain interface
    // ================================================================

    /**
     * @brief Abstract swapchain for presenting rendered images to a window.
     *
     * The swapchain manages a set of presentable images and synchronises
     * GPU rendering with the display refresh cycle.
     *
     * In **headless** mode the swapchain still produces renderable images
     * but @c Present() is a no-op (no window surface exists).  The rendered
     * result can be read back through the image texture for CPU-side use.
     */
    class Swapchain
    {
    public:
        virtual ~Swapchain() = default;

        /**
         * @brief Acquire the next presentable image for rendering.
         *
         * Call at the beginning of each frame before recording commands that
         * target the swapchain.
         *
         * @return Ref<Texture> The back-buffer texture, or nullptr if the
         *         swapchain is out of date and needs resizing.
         */
        virtual Ref<Texture> AcquireNextImage() = 0;

        /**
         * @brief Present the current image to the window surface.
         *
         * Call after submitting all command buffers for the frame.
         * In headless mode this is a no-op.
         */
        virtual void Present() = 0;

        /**
         * @brief Resize the swapchain to match a new window size.
         * @param width  New width in pixels.
         * @param height New height in pixels.
         */
        virtual void Resize(uint32_t width, uint32_t height) = 0;

        /** @brief Current swapchain image format. */
        virtual TextureFormat GetFormat() const = 0;

        /** @brief Current swapchain extent width in pixels. */
        virtual uint32_t GetWidth() const = 0;

        /** @brief Current swapchain extent height in pixels. */
        virtual uint32_t GetHeight() const = 0;

        /** @brief Whether this swapchain runs in headless (off-screen) mode. */
        virtual bool IsHeadless() const = 0;

        /** @brief Enable or disable vertical synchronisation at runtime. */
        virtual void SetVSync(bool enabled) = 0;

        /** @brief Query whether VSync is currently enabled. */
        virtual bool IsVSync() const = 0;

        /**
         * @brief Get the current active index within the swapchain image ring.
         * @return uint32_t Index of the current image (0-based).
         */
        virtual uint32_t GetCurrentImageIndex() const = 0;

        /**
         * @brief Get the number of images managed by this swapchain.
         * @return uint32_t Image count.
         */
        virtual uint32_t GetImageCount() const = 0;

        /**
         * @brief Get a framebuffer that wraps the current swapchain image.
         *
         * Convenience accessor so callers can pass the swapchain directly
         * into CommandBuffer::BeginRenderPass without manually constructing
         * a Framebuffer each frame.
         *
         * @return Ref<Framebuffer> Framebuffer for the current back-buffer.
         */
        virtual Ref<Framebuffer> GetCurrentFramebuffer() = 0;
    };

} // namespace Echelon
