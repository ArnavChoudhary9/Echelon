#pragma once

/**
 * @file TextureImporter.hpp
 * @brief Image loader back-end (produces a TextureAsset) built on stb_image.
 *
 * NOTE: STB_IMAGE_IMPLEMENTATION lives in TextureImporter.cpp (a single TU), not
 * here — defining it in a header would cause multiple-definition link errors. This
 * mirrors the OBJImporter / TINYOBJLOADER_IMPLEMENTATION arrangement.
 */

#include "Core/Base.hpp"
#include "Asset/Importers/Importer.hpp"

namespace Echelon {

    class TextureImporter : public AssetImporter {
    public:
        std::vector<std::string> GetSupportedExtensions() const override {
            return { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".psd", ".gif", ".hdr" };
        }
        AssetType GetAssetType() const override { return AssetType::Texture; }
        ImportResult Import(const ImportContext& ctx) override;
    };

} // namespace Echelon
