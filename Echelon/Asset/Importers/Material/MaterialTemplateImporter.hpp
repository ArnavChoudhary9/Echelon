#pragma once

/**
 * @file MaterialTemplateImporter.hpp
 * @brief Loader back-end for `.ehmaterialtype` files (produces a MaterialTemplate).
 */

#include "Core/Base.hpp"
#include "Asset/Importers/Importer.hpp"

namespace Echelon {

    class MaterialTemplate; // fwd

    class MaterialTemplateImporter : public AssetImporter {
    public:
        std::vector<std::string> GetSupportedExtensions() const override { return { ".ehmaterialtype" }; }
        AssetType GetAssetType() const override { return AssetType::MaterialTemplate; }
        ImportResult Import(const ImportContext& ctx) override;
    };

    /** @brief Serialize a MaterialTemplate to a `.ehmaterialtype` YAML file. */
    bool SaveMaterialTemplate(const Ref<MaterialTemplate>& tmpl, const fs::path& path);

} // namespace Echelon
