#include "Asset/AssetMetadata.hpp"
#include "Core/Log.hpp"

#include "yaml-cpp/yaml.h"

#include <fstream>
#include <exception>

namespace Echelon {

    bool SaveMeta(const AssetMetadata& meta, const fs::path& metaPath) {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Handle" << YAML::Value << meta.Handle.ToString();
        out << YAML::Key << "Type"   << YAML::Value << AssetTypeToString(meta.Type);
        out << YAML::EndMap;

        std::ofstream fout(metaPath);
        if (!fout.is_open()) {
            ECHELON_LOG_ERROR("[Asset] Could not write meta file: {}", metaPath.string());
            return false;
        }
        fout << out.c_str();
        return true;
    }

    std::optional<AssetMetadata> LoadMeta(const fs::path& metaPath) {
        std::error_code ec;
        if (!fs::exists(metaPath, ec))
            return std::nullopt;

        try {
            YAML::Node node = YAML::LoadFile(metaPath.string());
            AssetMetadata meta;

            std::string handleStr = node["Handle"] ? node["Handle"].as<std::string>("") : "";
            meta.Handle = handleStr.empty() ? UUID::Null() : UUID(handleStr);
            meta.Type   = AssetTypeFromString(node["Type"].as<std::string>("None"));
            return meta;
        } catch (const std::exception& e) {
            ECHELON_LOG_ERROR("[Asset] Failed to parse meta file '{}': {}", metaPath.string(), e.what());
            return std::nullopt;
        }
    }

} // namespace Echelon
