#include "Asset/Importers/Material/MaterialTemplateImporter.hpp"
#include "Asset/Material/MaterialTemplate.hpp"
#include "Core/Log.hpp"

#include "yaml-cpp/yaml.h"

#include <fstream>
#include <exception>
#include <cstring>

namespace Echelon {

    // ---- render-state enum <-> string --------------------------------------

    static const char* CullToString(CullMode c) {
        switch (c) {
            case CullMode::None:         return "None";
            case CullMode::Front:        return "Front";
            case CullMode::Back:         return "Back";
            case CullMode::FrontAndBack: return "FrontAndBack";
        }
        return "Back";
    }
    static CullMode CullFromString(const std::string& s) {
        if (s == "None")         return CullMode::None;
        if (s == "Front")        return CullMode::Front;
        if (s == "FrontAndBack") return CullMode::FrontAndBack;
        return CullMode::Back;
    }

    static const char* WindingToString(FrontFace f) {
        return f == FrontFace::Clockwise ? "CW" : "CCW";
    }
    static FrontFace WindingFromString(const std::string& s) {
        return (s == "CW" || s == "Clockwise") ? FrontFace::Clockwise : FrontFace::CounterClockwise;
    }

    static const char* TopologyToString(PrimitiveTopology t) {
        switch (t) {
            case PrimitiveTopology::TriangleList:  return "TriangleList";
            case PrimitiveTopology::TriangleStrip: return "TriangleStrip";
            case PrimitiveTopology::LineList:      return "LineList";
            case PrimitiveTopology::LineStrip:     return "LineStrip";
            case PrimitiveTopology::PointList:     return "PointList";
        }
        return "TriangleList";
    }
    static PrimitiveTopology TopologyFromString(const std::string& s) {
        if (s == "TriangleStrip") return PrimitiveTopology::TriangleStrip;
        if (s == "LineList")      return PrimitiveTopology::LineList;
        if (s == "LineStrip")     return PrimitiveTopology::LineStrip;
        if (s == "PointList")     return PrimitiveTopology::PointList;
        return PrimitiveTopology::TriangleList;
    }

    // ---- param value (Type + Value[]) --------------------------------------

    static MaterialParam ParseValue(MaterialParamType type, const YAML::Node& values) {
        MaterialParam p;
        p.Type = type;
        const uint32_t floats = p.ByteSize() / 4;
        if (values && values.IsSequence()) {
            for (uint32_t i = 0; i < floats && i < values.size(); ++i) {
                if (type == MaterialParamType::Int) {
                    int v = values[i].as<int>(0);
                    std::memcpy(&p.Data[i], &v, sizeof(int));
                } else {
                    p.Data[i] = values[i].as<float>(0.0f);
                }
            }
        }
        return p;
    }

    static void EmitValue(YAML::Emitter& out, const MaterialParam& p) {
        const uint32_t floats = p.ByteSize() / 4;
        out << YAML::Flow << YAML::BeginSeq;
        for (uint32_t i = 0; i < floats; ++i) {
            if (p.Type == MaterialParamType::Int) out << *reinterpret_cast<const int*>(&p.Data[i]);
            else                                  out << p.Data[i];
        }
        out << YAML::EndSeq;
    }

    ImportResult MaterialTemplateImporter::Import(const ImportContext& ctx) {
        const std::string path = ctx.GetPathString();
        try {
            YAML::Node root = YAML::LoadFile(path);
            YAML::Node node = root["MaterialTemplate"] ? root["MaterialTemplate"] : root;

            auto tmpl = CreateRef<MaterialTemplate>();
            tmpl->ShaderSource = node["Shader"].as<std::string>("");

            if (node["Cull"])       tmpl->Cull       = CullFromString(node["Cull"].as<std::string>("Back"));
            if (node["Winding"])    tmpl->Winding    = WindingFromString(node["Winding"].as<std::string>("CCW"));
            if (node["DepthTest"])  tmpl->DepthTest  = node["DepthTest"].as<bool>(true);
            if (node["DepthWrite"]) tmpl->DepthWrite = node["DepthWrite"].as<bool>(true);
            if (node["Topology"])   tmpl->Topology   = TopologyFromString(node["Topology"].as<std::string>("TriangleList"));

            if (const YAML::Node params = node["Params"]) {
                for (const auto& kv : params) {
                    MaterialTemplate::ParamDesc pd;
                    pd.Name = kv.first.as<std::string>();
                    pd.Type = MaterialParamTypeFromString(kv.second["Type"].as<std::string>("Float4"));
                    pd.Default = ParseValue(pd.Type, kv.second["Default"]);
                    pd.Hint = ParamUiFromString(kv.second["Ui"].as<std::string>("Auto"));
                    tmpl->Params.push_back(std::move(pd));
                }
            }
            if (const YAML::Node texs = node["Textures"]) {
                for (const auto& kv : texs) {
                    MaterialTemplate::TextureSlot ts;
                    ts.Slot = kv.first.as<std::string>();
                    // Accept either `u_Albedo: path` or `u_Albedo: { Default: path }`.
                    if (kv.second.IsMap()) ts.DefaultPath = kv.second["Default"].as<std::string>("");
                    else                   ts.DefaultPath = kv.second.as<std::string>("");
                    tmpl->Textures.push_back(std::move(ts));
                }
            }

            ECHELON_LOG_INFO("[MaterialTemplateImporter] Loaded '{}' (shader '{}', {} params, {} textures).",
                             path, tmpl->ShaderSource, tmpl->Params.size(), tmpl->Textures.size());
            return ImportResult(tmpl);
        }
        catch (const std::exception& e) {
            ECHELON_LOG_ERROR("[MaterialTemplateImporter] Exception loading '{}': {}", path, e.what());
            return ImportResult(std::string("Exception: ") + e.what());
        }
    }

    bool SaveMaterialTemplate(const Ref<MaterialTemplate>& tmpl, const fs::path& path) {
        if (!tmpl) return false;

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "MaterialTemplate" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Shader"     << YAML::Value << tmpl->ShaderSource;
        out << YAML::Key << "Cull"       << YAML::Value << CullToString(tmpl->Cull);
        out << YAML::Key << "Winding"    << YAML::Value << WindingToString(tmpl->Winding);
        out << YAML::Key << "DepthTest"  << YAML::Value << tmpl->DepthTest;
        out << YAML::Key << "DepthWrite" << YAML::Value << tmpl->DepthWrite;
        out << YAML::Key << "Topology"   << YAML::Value << TopologyToString(tmpl->Topology);

        out << YAML::Key << "Params" << YAML::Value << YAML::BeginMap;
        for (const auto& p : tmpl->Params) {
            out << YAML::Key << p.Name << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type"    << YAML::Value << MaterialParamTypeToString(p.Type);
            out << YAML::Key << "Default" << YAML::Value; EmitValue(out, p.Default);
            if (p.Hint != ParamUi::Auto)
                out << YAML::Key << "Ui" << YAML::Value << ParamUiToString(p.Hint);
            out << YAML::EndMap;
        }
        out << YAML::EndMap; // Params

        if (!tmpl->Textures.empty()) {
            out << YAML::Key << "Textures" << YAML::Value << YAML::BeginMap;
            for (const auto& t : tmpl->Textures)
                out << YAML::Key << t.Slot << YAML::Value << t.DefaultPath;
            out << YAML::EndMap;
        }

        out << YAML::EndMap; // MaterialTemplate
        out << YAML::EndMap;

        std::ofstream fout(path);
        if (!fout.is_open()) {
            ECHELON_LOG_ERROR("[MaterialTemplateImporter] Could not write '{}'", path.string());
            return false;
        }
        fout << out.c_str();
        return true;
    }

} // namespace Echelon
