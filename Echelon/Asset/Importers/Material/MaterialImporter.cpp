#include "Asset/Importers/Material/MaterialImporter.hpp"
#include "Asset/Material/Material.hpp"
#include "Core/Log.hpp"

#include "yaml-cpp/yaml.h"

#include <fstream>
#include <exception>

namespace Echelon {

    // A MaterialParam is stored as { Type: <name>, Value: [floats...] }.
    static void EmitParam(YAML::Emitter& out, const std::string& name, const MaterialParam& p) {
        const uint32_t floats = p.ByteSize() / 4;
        out << YAML::Key << name << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Type"  << YAML::Value << MaterialParamTypeToString(p.Type);
        out << YAML::Key << "Value" << YAML::Value << YAML::Flow << YAML::BeginSeq;
        for (uint32_t i = 0; i < floats; ++i) {
            if (p.Type == MaterialParamType::Int) out << *reinterpret_cast<const int*>(&p.Data[i]);
            else                                  out << p.Data[i];
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    static MaterialParam ParseParam(const YAML::Node& node) {
        MaterialParam p;
        p.Type = MaterialParamTypeFromString(node["Type"].as<std::string>("Float4"));
        const uint32_t floats = p.ByteSize() / 4;
        const YAML::Node values = node["Value"];
        if (values && values.IsSequence()) {
            for (uint32_t i = 0; i < floats && i < values.size(); ++i) {
                if (p.Type == MaterialParamType::Int) {
                    int v = values[i].as<int>(0);
                    std::memcpy(&p.Data[i], &v, sizeof(int));
                } else {
                    p.Data[i] = values[i].as<float>(0.0f);
                }
            }
        }
        return p;
    }

    ImportResult MaterialImporter::Import(const ImportContext& ctx) {
        const std::string path = ctx.GetPathString();
        try {
            YAML::Node root = YAML::LoadFile(path);
            YAML::Node node = root["Material"] ? root["Material"] : root;

            auto mat = CreateRef<Material>();
            mat->ShaderSource = node["Shader"].as<std::string>("");
            mat->ParentSource = node["Parent"].as<std::string>("");

            if (const YAML::Node params = node["Params"]) {
                for (const auto& kv : params)
                    mat->Params[kv.first.as<std::string>()] = ParseParam(kv.second);
            }
            if (const YAML::Node texs = node["Textures"]) {
                for (const auto& kv : texs)
                    mat->Textures[kv.first.as<std::string>()] = kv.second.as<std::string>("");
            }

            ECHELON_LOG_INFO("[MaterialImporter] Loaded '{}' (shader '{}', {} params, {} textures).",
                             path, mat->ShaderSource, mat->Params.size(), mat->Textures.size());
            return ImportResult(mat);
        }
        catch (const std::exception& e) {
            ECHELON_LOG_ERROR("[MaterialImporter] Exception loading '{}': {}", path, e.what());
            return ImportResult(std::string("Exception: ") + e.what());
        }
    }

    bool SaveMaterial(const Ref<Material>& material, const fs::path& path) {
        if (!material) return false;

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Material" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Shader" << YAML::Value << material->ShaderSource;
        if (!material->ParentSource.empty())
            out << YAML::Key << "Parent" << YAML::Value << material->ParentSource;

        out << YAML::Key << "Params" << YAML::Value << YAML::BeginMap;
        for (const auto& [name, p] : material->Params)
            EmitParam(out, name, p);
        out << YAML::EndMap;

        if (!material->Textures.empty()) {
            out << YAML::Key << "Textures" << YAML::Value << YAML::BeginMap;
            for (const auto& [name, tex] : material->Textures)
                out << YAML::Key << name << YAML::Value << tex;
            out << YAML::EndMap;
        }

        out << YAML::EndMap; // Material
        out << YAML::EndMap;

        std::ofstream fout(path);
        if (!fout.is_open()) {
            ECHELON_LOG_ERROR("[MaterialImporter] Could not write '{}'", path.string());
            return false;
        }
        fout << out.c_str();
        return true;
    }

} // namespace Echelon
