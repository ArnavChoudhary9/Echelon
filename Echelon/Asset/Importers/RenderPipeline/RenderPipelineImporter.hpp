#pragma once

/**
 * @file RenderPipelineImporter.hpp
 * @brief Loader back-end for `.ehpipeline` files (produces a RenderPipelineAsset).
 */

#include "Core/Base.hpp"
#include "Asset/Importers/Importer.hpp"

namespace Echelon {

    class RenderPipelineImporter : public AssetImporter {
    public:
        std::vector<std::string> GetSupportedExtensions() const override { return { ".ehpipeline" }; }
        AssetType GetAssetType() const override { return AssetType::RenderPipeline; }
        ImportResult Import(const ImportContext& ctx) override;
    };

} // namespace Echelon
