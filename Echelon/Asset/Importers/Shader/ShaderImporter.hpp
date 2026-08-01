#pragma once

/**
 * @file ShaderImporter.hpp
 * @brief Slang → SPIR-V shader loader back-end (produces a ShaderAsset).
 *
 * Compiles a .slang file to SPIR-V for every entry point and extracts reflection
 * (vertex inputs, uniform buffers, samplers) via the Slang reflection API. All
 * Slang includes live in ShaderImporter.cpp (a single TU) so the compiler
 * dependency does not leak into the rest of the engine.
 */

#include "Core/Base.hpp"
#include "Asset/Importers/Importer.hpp"

namespace Echelon {

    class ShaderImporter : public AssetImporter {
    public:
        std::vector<std::string> GetSupportedExtensions() const override { return { ".slang" }; }
        AssetType GetAssetType() const override { return AssetType::Shader; }
        ImportResult Import(const ImportContext& ctx) override;
    };

} // namespace Echelon
