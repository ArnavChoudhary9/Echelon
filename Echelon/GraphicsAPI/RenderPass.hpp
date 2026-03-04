#pragma once

/**
 * @file RenderPass.hpp
 * @brief Render pass interface and all render-pass / attachment types.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // TextureFormat

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    // ================================================================
    // Load / store operations
    // ================================================================

    /**
     * @brief What to do with an attachment at the start of a render pass.
     */
    enum class LoadOp : uint8_t
    {
        Load = 0,
        Clear,
        DontCare
    };

    /**
     * @brief What to do with an attachment at the end of a render pass.
     */
    enum class StoreOp : uint8_t
    {
        Store = 0,
        DontCare
    };

    // ================================================================
    // Clear values
    // ================================================================

    /**
     * @brief RGBA clear color value.
     */
    struct ClearColor
    {
        float R = 0.0f;
        float G = 0.0f;
        float B = 0.0f;
        float A = 1.0f;
    };

    /**
     * @brief Clear value for a depth/stencil attachment.
     */
    struct ClearDepthStencil
    {
        float    Depth   = 1.0f;
        uint32_t Stencil = 0;
    };

    // ================================================================
    // Attachment descriptors
    // ================================================================

    /**
     * @brief Describes a single color attachment within a render pass.
     */
    struct ColorAttachmentDesc
    {
        TextureFormat Format = TextureFormat::RGBA8_UNORM;
        LoadOp        Load   = LoadOp::Clear;
        StoreOp       Store  = StoreOp::Store;
        ClearColor    Clear  = {};
    };

    /**
     * @brief Describes the depth attachment within a render pass.
     */
    struct DepthAttachmentDesc
    {
        TextureFormat     Format = TextureFormat::D32_FLOAT;
        LoadOp            Load   = LoadOp::Clear;
        StoreOp           Store  = StoreOp::Store;
        ClearDepthStencil Clear  = {};
    };

    /**
     * @brief Descriptor for creating a render pass.
     */
    struct RenderPassDesc
    {
        std::vector<ColorAttachmentDesc> ColorAttachments;
        DepthAttachmentDesc              DepthAttachment    = {};
        bool                             HasDepthAttachment = false;
        std::string                      DebugName          = "";
    };

    // ================================================================
    // RenderPass interface
    // ================================================================

    /**
     * @brief Abstract render pass.
     *
     * A render pass describes a rendering stage: the set of color and depth
     * attachments, their formats, and load/store operations.  Multiple render
     * passes can execute sequentially within a single frame (e.g. shadow,
     * G-buffer, lighting, post-process, UI).
     *
     * Conceptually similar to VkRenderPass.  On backends that don't have a
     * native equivalent (e.g. OpenGL) it is emulated.
     */
    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        /**
         * @brief Get the number of color attachments in this render pass.
         * @return uint32_t Attachment count.
         */
        virtual uint32_t GetColorAttachmentCount() const = 0;

        /**
         * @brief Query whether this render pass includes a depth attachment.
         * @return true if a depth attachment is present.
         */
        virtual bool HasDepthAttachment() const = 0;

        /**
         * @brief Get the debug name assigned to this render pass.
         * @return const std::string&
         */
        virtual const std::string& GetDebugName() const = 0;
    };

} // namespace Echelon
