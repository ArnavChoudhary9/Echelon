#pragma once

/**
 * @file SceneImporter.hpp
 * @brief Loads a .ehscene file into a Scene asset via the shared asset pipeline.
 *
 * Scene is an Asset, so it flows through the same AssetManager path as meshes.
 * The actual YAML parsing is delegated to the existing SceneSerializer.
 */

#include "Core/Base.hpp"
#include "Asset/Importers/Importer.hpp"

namespace Echelon {

    class SceneImporter : public AssetImporter {
    public:
        std::vector<std::string> GetSupportedExtensions() const override { return { ".ehscene" }; }
        AssetType GetAssetType() const override { return AssetType::Scene; }
        ImportResult Import(const ImportContext& ctx) override;
    };

} // namespace Echelon
