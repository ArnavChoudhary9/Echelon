#pragma once

/**
 * @file GraphicsTypes.hpp
 * @brief Shared / cross-cutting types used by multiple GraphicsAPI interfaces.
 *
 * Domain-specific types live alongside their interface:
 *   Buffer types      → Buffer.hpp
 *   Texture types     → Texture.hpp
 *   Shader types      → Shader.hpp
 *   Pipeline state    → Pipeline.hpp
 *   RenderPass types  → RenderPass.hpp
 *   Descriptor types  → DescriptorSet.hpp
 *   Swapchain types   → Swapchain.hpp
 *   Framebuffer types → Framebuffer.hpp
 */

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    // ================================================================
    // Backend selection
    // ================================================================

    /**
     * @brief Enumerates the supported graphics backends.
     */
    enum class GraphicsBackend : uint8_t
    {
        None = 0,   ///< No backend — null/stub (useful for unit tests)
        Headless,   ///< Offscreen-only — no window or presentation
        OpenGL,
        Vulkan,
        DirectX12,
        Metal
    };

    // ================================================================
    // Texture format  (shared: Texture, RenderPass, Swapchain, Framebuffer)
    // ================================================================

    /**
     * @brief Pixel / texel format for textures, render targets, and swapchains.
     */
    enum class TextureFormat : uint8_t
    {
        Undefined = 0,

        // Color formats
        R8_UNORM,
        RG8_UNORM,
        RGBA8_UNORM,
        RGBA8_SRGB,
        BGRA8_UNORM,
        BGRA8_SRGB,

        R16_FLOAT,
        RG16_FLOAT,
        RGBA16_FLOAT,

        R32_FLOAT,
        RG32_FLOAT,
        RGBA32_FLOAT,

        // Depth / stencil
        D16_UNORM,
        D24_UNORM_S8_UINT,
        D32_FLOAT,
        D32_FLOAT_S8_UINT
    };

    /**
     * @brief Returns true when the format describes a depth or depth/stencil layout.
     */
    inline bool IsDepthFormat(TextureFormat fmt)
    {
        return fmt == TextureFormat::D16_UNORM
            || fmt == TextureFormat::D24_UNORM_S8_UINT
            || fmt == TextureFormat::D32_FLOAT
            || fmt == TextureFormat::D32_FLOAT_S8_UINT;
    }

    // ================================================================
    // Shader stage  (shared: Shader, DescriptorSet)
    // ================================================================

    /**
     * @brief Stage of a shader within the graphics or compute pipeline.
     */
    enum class ShaderStage : uint8_t
    {
        Vertex = 0,
        Fragment,
        Geometry,
        TessellationControl,
        TessellationEvaluation,
        Compute
    };

    // ================================================================
    // Comparison operator  (shared: Pipeline depth state, sampler, stencil)
    // ================================================================

    /**
     * @brief Comparison function used by depth / stencil tests and samplers.
     */
    enum class CompareOp : uint8_t
    {
        Never = 0,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    // ================================================================
    // Index type  (shared: CommandBuffer)
    // ================================================================

    /**
     * @brief Index data type for indexed draw calls.
     */
    enum class IndexType : uint8_t
    {
        UInt16 = 0,
        UInt32
    };

    // ================================================================
    // Viewport and scissor  (shared: CommandBuffer)
    // ================================================================

    /**
     * @brief Viewport rectangle.
     */
    struct Viewport
    {
        float X        = 0.0f;
        float Y        = 0.0f;
        float Width    = 0.0f;
        float Height   = 0.0f;
        float MinDepth = 0.0f;
        float MaxDepth = 1.0f;
    };

    /**
     * @brief Scissor rectangle.
     */
    struct ScissorRect
    {
        int32_t  X      = 0;
        int32_t  Y      = 0;
        uint32_t Width  = 0;
        uint32_t Height = 0;
    };

} // namespace Echelon
