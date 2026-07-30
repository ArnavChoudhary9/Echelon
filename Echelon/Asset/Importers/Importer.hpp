#pragma once

/**
 * @file Importer.hpp
 * @brief Base interface for all asset importers (loader back-ends).
 *
 * An importer converts a file on disk into a Ref<Asset>. The AssetManager keys
 * importers by file extension and picks one automatically, so adding support for
 * a new format (from engine or user code) is just a new AssetImporter subclass +
 * a single RegisterImporter call.
 */

#include "Core/Base.hpp"
#include "Asset/Asset.hpp"
#include "Asset/Importers/ImportContext.hpp"
#include "Asset/Importers/ImportResult.hpp"

#include <string>
#include <vector>

namespace Echelon {

    class AssetImporter {
    public:
        virtual ~AssetImporter() = default;

        /** @brief Lower-case file extensions this importer handles, e.g. {".obj"}. */
        virtual std::vector<std::string> GetSupportedExtensions() const = 0;

        /** @brief The asset type produced by this importer. */
        virtual AssetType GetAssetType() const = 0;

        /** @brief Load the asset. Must never throw — report failure via ImportResult. */
        virtual ImportResult Import(const ImportContext& ctx) = 0;
    };

} // namespace Echelon
