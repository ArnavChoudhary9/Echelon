#pragma once

/**
 * @file MaterialImporter.hpp
 * @brief Loader back-end for `.ehmaterial` files (produces a Material asset).
 */

#include "Core/Base.hpp"
#include "Asset/Importers/Importer.hpp"

namespace Echelon {

    class Material; // fwd

    class MaterialImporter : public AssetImporter {
    public:
        std::vector<std::string> GetSupportedExtensions() const override { return { ".ehmaterial" }; }
        AssetType GetAssetType() const override { return AssetType::Material; }
        ImportResult Import(const ImportContext& ctx) override;
    };

    /** @brief Serialize a Material to a `.ehmaterial` YAML file. */
    bool SaveMaterial(const Ref<Material>& material, const fs::path& path);

} // namespace Echelon
