#pragma once

/**
 * @file OBJImporter.hpp
 * @brief Wavefront .obj loader back-end (produces a Mesh asset).
 *
 * NOTE: TINYOBJLOADER_IMPLEMENTATION lives in OBJImporter.cpp (a single TU), not
 * here — defining it in a header would cause multiple-definition link errors.
 */

#include "Core/Base.hpp"
#include "Asset/Importers/Importer.hpp"

namespace Echelon {

    class OBJImporter : public AssetImporter {
    public:
        std::vector<std::string> GetSupportedExtensions() const override { return { ".obj" }; }
        AssetType GetAssetType() const override { return AssetType::Mesh; }
        ImportResult Import(const ImportContext& ctx) override;
    };

} // namespace Echelon
