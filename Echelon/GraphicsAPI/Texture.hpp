#pragma once

/**
 * @file Texture.hpp
 * @brief GPU texture interface and all texture / sampler types.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // TextureFormat

#include <cstdint>
#include <string>

namespace Echelon {

    // ================================================================
    // Texture types
    // ================================================================

    /**
     * @brief Dimensionality / type of a texture resource.
     */
    enum class TextureType : uint8_t
    {
        Texture2D = 0,
        Texture3D,
        TextureCube,
        TextureArray,
        Depth,
        RenderTarget,
        StorageTexture
    };

    /**
     * @brief How a texture may be used by the GPU.  Flags can be combined.
     */
    enum class TextureUsage : uint32_t
    {
        Sampled       = 1 << 0,
        Storage       = 1 << 1,
        RenderTarget  = 1 << 2,
        DepthStencil  = 1 << 3,
        TransferSrc   = 1 << 4,
        TransferDst   = 1 << 5
    };

    inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline TextureUsage operator&(TextureUsage a, TextureUsage b) {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }
    inline bool HasFlag(TextureUsage value, TextureUsage flag) {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    /**
     * @brief Descriptor for creating a texture.
     */
    struct TextureDesc
    {
        TextureType   Type      = TextureType::Texture2D;
        TextureFormat Format    = TextureFormat::RGBA8_UNORM;
        TextureUsage  Usage     = TextureUsage::Sampled;
        uint32_t      Width     = 1;
        uint32_t      Height    = 1;
        uint32_t      Depth     = 1;
        uint32_t      MipLevels = 1;
        uint32_t      ArraySize = 1;
        std::string   DebugName = "";
    };

    // ================================================================
    // Sampler types
    // ================================================================

    /**
     * @brief Texture filtering mode.
     */
    enum class FilterMode : uint8_t
    {
        Nearest = 0,
        Linear
    };

    /**
     * @brief Texture address / wrap mode.
     */
    enum class AddressMode : uint8_t
    {
        Repeat = 0,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };

    /**
     * @brief Descriptor for creating a sampler.
     */
    struct SamplerDesc
    {
        FilterMode  MinFilter     = FilterMode::Linear;
        FilterMode  MagFilter     = FilterMode::Linear;
        FilterMode  MipMapFilter  = FilterMode::Linear;
        AddressMode AddressU      = AddressMode::Repeat;
        AddressMode AddressV      = AddressMode::Repeat;
        AddressMode AddressW      = AddressMode::Repeat;
        float       MaxAnisotropy = 1.0f;
    };

    // ================================================================
    // Texture interface
    // ================================================================

    /**
     * @brief Abstract GPU texture resource.
     *
     * Represents a 2D, 3D, cube-map, array, depth, render target, or
     * storage texture.  Concrete backends implement the actual GPU allocation.
     */
    class Texture
    {
    public:
        virtual ~Texture() = default;

        /** @brief Width in texels. */
        virtual uint32_t GetWidth() const = 0;

        /** @brief Height in texels. */
        virtual uint32_t GetHeight() const = 0;

        /** @brief Depth (1 for non-3D textures). */
        virtual uint32_t GetDepth() const = 0;

        /** @brief Mip level count. */
        virtual uint32_t GetMipLevels() const = 0;

        /** @brief Texture type. */
        virtual TextureType GetType() const = 0;

        /** @brief Texel format. */
        virtual TextureFormat GetFormat() const = 0;

        /**
         * @brief Upload pixel data to the texture.
         *
         * @param data       Pointer to source pixel data.
         * @param size       Size of data in bytes.
         * @param mipLevel   Target mip level.
         * @param arrayLayer Target array layer (0 for non-array textures).
         */
        virtual void SetData(const void* data, uint64_t size,
                             uint32_t mipLevel = 0, uint32_t arrayLayer = 0) = 0;
    };

} // namespace Echelon
