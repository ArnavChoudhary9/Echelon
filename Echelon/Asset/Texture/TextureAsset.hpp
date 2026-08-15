#pragma once

/**
 * @file TextureAsset.hpp
 * @brief Image texture asset — owns CPU pixels and the GPU texture built from them.
 *
 * Authored as an image file (.png/.jpg/.tga/...), decoded to raw pixels at import
 * time by the TextureImporter (stb_image). The CPU pixel buffer is retained so the
 * GPU texture can be rebuilt on renderer hot-swap and asset hot-reload — mirroring
 * Mesh and ShaderAsset. Uploads go through the active renderer's Device, so this
 * class carries no back-end-specific code.
 *
 * Named TextureAsset (not Texture) to avoid clashing with the GraphicsAPI's
 * abstract `class Texture` (the GPU object this asset produces).
 */

#include "GraphicsAPI/Texture.hpp"
#include "GraphicsAPI/Device.hpp"

#include "Renderer/RendererAPI.hpp"

#include "Asset/Asset.hpp"
#include "Core/Log.hpp"

#include <vector>
#include <cstdint>
#include <utility>

namespace Echelon {

    class TextureAsset : public Asset {
    public:
        TextureAsset() = default;
        ~TextureAsset() override = default;

        /**
         * @brief Replace the CPU-side pixel data. Does not touch the GPU.
         * @param pixels   Tightly-packed pixels matching @p format (row-major, no padding).
         * @param width    Width in texels.
         * @param height   Height in texels.
         * @param format   GPU texel format the pixels are stored in.
         * @param generateMips  Request a full mip chain on upload.
         */
        void SetData(std::vector<uint8_t> pixels, uint32_t width, uint32_t height,
                     TextureFormat format, bool generateMips = true) {
            m_Pixels       = std::move(pixels);
            m_Width        = width;
            m_Height       = height;
            m_Format       = format;
            m_GenerateMips = generateMips;
        }

        AssetType GetType() const override { return AssetType::Texture; }

        /** @brief (Re)create the GPU texture from the retained CPU pixels. */
        void UploadGPU(RendererAPI* renderer) override {
            if (!renderer) return;
            auto device = renderer->GetDevice();
            if (!device) return;
            if (m_GpuTexture || m_Pixels.empty() || m_Width == 0 || m_Height == 0)
                return;

            const uint32_t mipLevels = m_GenerateMips ? ComputeMipLevels(m_Width, m_Height) : 1;

            TextureDesc td;
            td.Type      = TextureType::Texture2D;
            td.Format    = m_Format;
            td.Usage     = TextureUsage::Sampled;
            td.Width     = m_Width;
            td.Height    = m_Height;
            td.MipLevels = mipLevels;
            td.DebugName = "TextureAsset";

            m_GpuTexture = device->CreateTexture(td);
            m_GpuTexture->SetData(m_Pixels.data(), m_Pixels.size());  // mip 0; SetData auto-generates the chain

            ECHELON_LOG_DEBUG("[Texture] Uploaded GPU texture {}x{} ({} mips, {} KB).",
                              m_Width, m_Height, mipLevels,
                              static_cast<uint32_t>(m_Pixels.size() / 1024));
        }

        /** @brief Drop the GPU texture; CPU pixels are retained for a later rebuild. */
        void ReleaseGPU() override { m_GpuTexture = nullptr; }

        /** @brief Absorb re-imported pixels in place so held Refs stay valid (hot-reload). */
        void ReloadFrom(const Ref<Asset>& fresh) override {
            auto other = std::dynamic_pointer_cast<TextureAsset>(fresh);
            if (!other) return;
            ReleaseGPU();
            SetData(other->m_Pixels, other->m_Width, other->m_Height,
                    other->m_Format, other->m_GenerateMips);
        }

        [[nodiscard]] bool IsValid() const override { return m_GpuTexture != nullptr; }

        [[nodiscard]] const Ref<Texture>& GetGpuTexture() const { return m_GpuTexture; }
        uint32_t      GetWidth()  const { return m_Width; }
        uint32_t      GetHeight() const { return m_Height; }
        TextureFormat GetFormat() const { return m_Format; }

    private:
        /** @brief floor(log2(max(w,h))) + 1 — the full mip count down to 1×1. */
        static uint32_t ComputeMipLevels(uint32_t width, uint32_t height) {
            uint32_t maxDim = width > height ? width : height;
            uint32_t levels = 1;
            while (maxDim > 1) { maxDim >>= 1; ++levels; }
            return levels;
        }

        std::vector<uint8_t> m_Pixels;
        uint32_t             m_Width        = 0;
        uint32_t             m_Height       = 0;
        TextureFormat        m_Format       = TextureFormat::RGBA8_UNORM;
        bool                 m_GenerateMips = true;

        Ref<Texture>         m_GpuTexture   = nullptr;
    };

} // namespace Echelon
