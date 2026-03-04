#pragma once

/**
 * @file GraphicsAPI.hpp
 * @brief Top-level graphics API factory and frame lifecycle.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // GraphicsBackend

namespace Echelon {

    // Forward declarations
    class Device;
    class CommandBuffer;

    /**
     * @brief Top-level graphics API factory.
     *
     * GraphicsAPI is the entry point for the graphics abstraction layer.
     * It selects a backend at runtime and creates the Device through which
     * all other GPU resources are allocated.
     *
     * Supports windowed and **headless** modes.  Pass
     * @c GraphicsBackend::Headless for off-screen / compute-only workloads.
     *
     * Usage:
     * @code
     *     // Windowed
     *     auto api    = GraphicsAPI::Create(GraphicsBackend::Vulkan);
     *     auto device = api->CreateDevice();
     *
     *     // Headless (no window required)
     *     auto api    = GraphicsAPI::Create(GraphicsBackend::Headless);
     *     auto device = api->CreateDevice();
     * @endcode
     */
    class GraphicsAPI
    {
    public:
        virtual ~GraphicsAPI() = default;

        /**
         * @brief Create the GPU device for this backend.
         * @return Ref<Device> Shared handle to the created device.
         */
        virtual Ref<Device> CreateDevice() = 0;

        /**
         * @brief Get the active graphics backend.
         * @return GraphicsBackend The backend this instance wraps.
         */
        virtual GraphicsBackend GetBackend() const = 0;

        /**
         * @brief Initialise the backend's function loader (e.g. glad for OpenGL).
         *
         * Must be called after a valid rendering context is current.
         * @return true on success.
         */
        virtual bool InitLoader() = 0;

        /**
         * @brief Whether this API instance is running headless (no window).
         */
        virtual bool IsHeadless() const = 0;

        /**
         * @brief Submit a command buffer to the GPU for execution.
         * @param commandBuffer The recorded command buffer to submit.
         */
        virtual void Submit(const Ref<CommandBuffer>& commandBuffer) = 0;

        /**
         * @brief Begin a new frame.
         *
         * Called once per frame before any command recording.  Handles
         * internal synchronisation (e.g. fence waits, frame-resource cycling).
         */
        virtual void BeginFrame() = 0;

        /**
         * @brief End the current frame.
         *
         * Called once per frame after all command buffers have been submitted.
         */
        virtual void EndFrame() = 0;

        // ---- Factory ----

        /**
         * @brief Create a GraphicsAPI instance for the specified backend.
         *
         * @param backend The backend to initialise.
         * @return Scope<GraphicsAPI> Owning pointer to the API instance.
         */
        static Scope<GraphicsAPI> Create(GraphicsBackend backend);

        /**
         * @brief Get the default graphics backend for this build.
         *
         * Determined by the ECHELON_GRAPHICS_BACKEND compile-time define.
         * Falls back to OpenGL if not specified.
         */
        static GraphicsBackend GetDefaultBackend();
    };

} // namespace Echelon
