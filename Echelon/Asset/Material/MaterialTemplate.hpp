#pragma once

/**
 * @file MaterialTemplate.hpp
 * @brief Material template asset — a BRDF definition. PURE DATA.
 *
 * A MaterialTemplate is the reusable "shader + look policy" that Material
 * *instances* are built on (e.g. the standard PBR template, an Unlit template, or
 * a user-authored custom-shader template). It owns NO GPU objects: it names a
 * shader, declares default render state, and describes the editable parameter
 * schema (name + type + default + a UI hint) and the texture slots. The renderer
 * turns (template + a per-instance blend flag) into a cached pipeline; the shader
 * reflection remains the source of truth for UBO byte offsets at pack time, while
 * the template supplies parameter types, defaults, and UI hints.
 *
 * Saved to disk as a `.ehmaterialtype` YAML file. Add a parameter by editing the
 * template (and the shader's material UBO) — no C++ change required.
 */

#include "Asset/Asset.hpp"
#include "Asset/Material/MaterialParam.hpp"

#include "Echelon/GraphicsAPI/Pipeline.hpp"   // CullMode / FrontFace / PrimitiveTopology

#include "Core/UUID.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    /** @brief How a parameter should be presented in the editor. */
    enum class ParamUi : uint8_t {
        Auto = 0,   ///< Pick a widget from the value type (DragFloat*/DragInt).
        Color       ///< Present a color swatch (ColorEdit3/4) — for Float3/Float4 colors.
    };

    inline const char* ParamUiToString(ParamUi u) { return u == ParamUi::Color ? "Color" : "Auto"; }
    inline ParamUi     ParamUiFromString(const std::string& s) { return s == "Color" ? ParamUi::Color : ParamUi::Auto; }

    class MaterialTemplate : public Asset {
    public:
        /** @brief One editable parameter exposed by this template. */
        struct ParamDesc {
            std::string       Name;                         ///< Matches the shader's material-UBO member name.
            MaterialParamType Type    = MaterialParamType::Float4;
            MaterialParam     Default;                       ///< Value used when an instance leaves it unset.
            ParamUi           Hint    = ParamUi::Auto;
        };

        /** @brief One texture slot exposed by this template. */
        struct TextureSlot {
            std::string Slot;          ///< Reflected sampler name (e.g. "u_Albedo").
            std::string DefaultPath;   ///< Optional default texture asset path.
        };

        // ---- Serialized data ----
        UUID        ShaderHandle = UUID::Null();  ///< Authoritative shader reference (the BRDF).
        std::string ShaderSource;                 ///< Readable hint (path) for the shader, e.g. "shader:PBR.slang".

        // Default render state (per-instance blend is layered on top by the renderer).
        CullMode          Cull       = CullMode::Back;
        FrontFace         Winding    = FrontFace::CounterClockwise;
        bool              DepthTest  = true;
        bool              DepthWrite = true;
        PrimitiveTopology Topology   = PrimitiveTopology::TriangleList;

        std::vector<ParamDesc>   Params;    ///< Editable parameter schema (drives the inspector + defaults).
        std::vector<TextureSlot> Textures;  ///< Texture slots (reflected sampler name → default path).

        MaterialTemplate() = default;
        ~MaterialTemplate() override = default;

        AssetType GetType() const override { return AssetType::MaterialTemplate; }

        /** @brief Valid as data once it names a shader (GPU validity is the renderer's concern). */
        bool IsValid() const override { return !ShaderHandle.IsNull() || !ShaderSource.empty(); }

        /** @brief Absorb freshly re-imported data (hot-reload); renderer rebuilds GPU state on the epoch bump. */
        void ReloadFrom(const Ref<Asset>& fresh) override;

        /** @brief The default value for a parameter (nullptr if the template doesn't declare it). */
        const MaterialParam* ResolveDefault(const std::string& name) const {
            for (const auto& p : Params)
                if (p.Name == name) return &p.Default;
            return nullptr;
        }

        /** @brief Look up a parameter's schema entry (nullptr if absent). */
        const ParamDesc* FindParam(const std::string& name) const {
            for (const auto& p : Params)
                if (p.Name == name) return &p;
            return nullptr;
        }
    };

} // namespace Echelon
