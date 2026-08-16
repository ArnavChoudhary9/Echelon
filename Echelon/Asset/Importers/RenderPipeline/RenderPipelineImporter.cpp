#include "Asset/Importers/RenderPipeline/RenderPipelineImporter.hpp"
#include "Asset/RenderPipeline/RenderPipelineAsset.hpp"
#include "Core/Log.hpp"

#include "yaml-cpp/yaml.h"

#include <cctype>
#include <exception>
#include <string>
#include <utility>

namespace Echelon {

    // ------------------------------------------------------------------
    // String -> enum helpers (case-insensitive)
    // ------------------------------------------------------------------

    static std::string Lower(std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    static TextureFormat ParseFormat(const std::string& raw, TextureFormat def) {
        const std::string s = Lower(raw);
        if (s == "rgba8_unorm"   || s == "rgba8")  return TextureFormat::RGBA8_UNORM;
        if (s == "rgba8_srgb"    || s == "srgb8")  return TextureFormat::RGBA8_SRGB;
        if (s == "bgra8_unorm"   || s == "bgra8")  return TextureFormat::BGRA8_UNORM;
        if (s == "bgra8_srgb")                     return TextureFormat::BGRA8_SRGB;
        if (s == "r8_unorm"      || s == "r8")     return TextureFormat::R8_UNORM;
        if (s == "rg8_unorm"     || s == "rg8")    return TextureFormat::RG8_UNORM;
        if (s == "r16_float"     || s == "r16f")   return TextureFormat::R16_FLOAT;
        if (s == "rg16_float"    || s == "rg16f")  return TextureFormat::RG16_FLOAT;
        if (s == "rgba16_float"  || s == "rgba16f")return TextureFormat::RGBA16_FLOAT;
        if (s == "r32_float"     || s == "r32f")   return TextureFormat::R32_FLOAT;
        if (s == "rg32_float"    || s == "rg32f")  return TextureFormat::RG32_FLOAT;
        if (s == "rgba32_float"  || s == "rgba32f")return TextureFormat::RGBA32_FLOAT;
        if (s == "d16_unorm"     || s == "d16")    return TextureFormat::D16_UNORM;
        if (s == "d24_unorm_s8_uint" || s == "d24s8") return TextureFormat::D24_UNORM_S8_UINT;
        if (s == "d32_float"     || s == "d32")    return TextureFormat::D32_FLOAT;
        if (s == "d32_float_s8_uint" || s == "d32s8") return TextureFormat::D32_FLOAT_S8_UINT;
        return def;
    }

    static LoadOp ParseLoad(const std::string& raw) {
        const std::string s = Lower(raw);
        if (s == "clear") return LoadOp::Clear;
        if (s == "load")  return LoadOp::Load;
        return LoadOp::DontCare;
    }

    static StoreOp ParseStore(const std::string& raw) {
        return Lower(raw) == "store" ? StoreOp::Store : StoreOp::DontCare;
    }

    static PassType ParseType(const std::string& raw) {
        const std::string s = Lower(raw);
        if (s == "fullscreen") return PassType::Fullscreen;
        if (s == "compute")    return PassType::Compute;
        return PassType::Graphics;
    }

    // ------------------------------------------------------------------
    // Node parsers
    // ------------------------------------------------------------------

    static AttachmentRef ParseColor(const YAML::Node& n) {
        AttachmentRef a;
        a.Resource = n["resource"].as<std::string>("");
        a.Load     = ParseLoad(n["load"].as<std::string>("clear"));
        a.Store    = ParseStore(n["store"].as<std::string>("store"));
        if (const YAML::Node cl = n["clear"]; cl && cl.IsSequence() && cl.size() >= 4) {
            a.ColorClear = ClearColor{ cl[0].as<float>(0.0f), cl[1].as<float>(0.0f),
                                       cl[2].as<float>(0.0f), cl[3].as<float>(1.0f) };
        }
        return a;
    }

    static AttachmentRef ParseDepth(const YAML::Node& n) {
        AttachmentRef a;
        a.Resource = n["resource"].as<std::string>("");
        a.Load     = ParseLoad(n["load"].as<std::string>("clear"));
        a.Store    = ParseStore(n["store"].as<std::string>("store"));
        if (const YAML::Node cl = n["clear"]) {
            a.DepthClear.Depth   = cl["depth"].as<float>(1.0f);
            a.DepthClear.Stencil = cl["stencil"].as<uint32_t>(0u);
        }
        return a;
    }

    // ------------------------------------------------------------------
    // Import
    // ------------------------------------------------------------------

    ImportResult RenderPipelineImporter::Import(const ImportContext& ctx) {
        const std::string path = ctx.GetPathString();
        try {
            YAML::Node root = YAML::LoadFile(path);
            YAML::Node node = root["RenderPipeline"] ? root["RenderPipeline"] : root;

            RenderPipelineDesc d;

            if (const YAML::Node resources = node["resources"]) {
                for (const auto& r : resources) {
                    ResourceDesc rd;
                    rd.Name   = r["name"].as<std::string>("");
                    rd.Format = ParseFormat(r["format"].as<std::string>("RGBA8_UNORM"),
                                            TextureFormat::RGBA8_UNORM);
                    if (Lower(r["size"].as<std::string>("swapchain")) == "fixed") {
                        rd.SizePolicy = ResourceSizePolicy::Fixed;
                        rd.Width  = r["width"].as<uint32_t>(0u);
                        rd.Height = r["height"].as<uint32_t>(0u);
                    } else {
                        rd.SizePolicy = ResourceSizePolicy::SwapchainRelative;
                        rd.Scale      = r["scale"].as<float>(1.0f);
                    }
                    d.Resources.push_back(std::move(rd));
                }
            }

            if (const YAML::Node passes = node["passes"]) {
                for (const auto& p : passes) {
                    PassDesc pd;
                    pd.Name    = p["name"].as<std::string>("");
                    pd.Type    = ParseType(p["type"].as<std::string>("graphics"));
                    pd.Enabled = p["enabled"].as<bool>(true);
                    pd.Shader  = p["shader"].as<std::string>("");

                    if (const YAML::Node cols = p["color"]) {
                        for (const auto& c : cols) pd.ColorOutputs.push_back(ParseColor(c));
                    }
                    if (const YAML::Node dep = p["depth"]) {
                        pd.DepthOutput = ParseDepth(dep);
                    }
                    if (const YAML::Node ins = p["inputs"]) {
                        for (const auto& in : ins) {
                            PassInput pi;
                            pi.Resource       = in["resource"].as<std::string>("");
                            pi.Binding        = in["binding"].as<uint32_t>(0u);
                            pi.AsStorageImage  = in["storage"].as<bool>(false);
                            pd.Inputs.push_back(std::move(pi));
                        }
                    }
                    if (const YAML::Node g = p["groups"]; g && g.IsSequence() && g.size() >= 3) {
                        pd.GroupCountX = g[0].as<uint32_t>(0u);
                        pd.GroupCountY = g[1].as<uint32_t>(0u);
                        pd.GroupCountZ = g[2].as<uint32_t>(0u);
                    }

                    d.Passes.push_back(std::move(pd));
                }
            }

            auto asset = CreateRef<RenderPipelineAsset>(std::move(d));
            ECHELON_LOG_INFO("[RenderPipelineImporter] Loaded '{}' ({} resource(s), {} pass(es)).",
                             path, asset->GetDescription().Resources.size(),
                             asset->GetDescription().Passes.size());
            return ImportResult(asset);
        }
        catch (const std::exception& e) {
            ECHELON_LOG_ERROR("[RenderPipelineImporter] Exception loading '{}': {}", path, e.what());
            return ImportResult(std::string("Exception: ") + e.what());
        }
    }

} // namespace Echelon
