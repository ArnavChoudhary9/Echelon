#pragma once

/**
 * @file Material.hpp
 * @brief Material asset — a *material instance*: a MaterialTemplate reference plus
 *        concrete parameter values + texture references. PURE DATA.
 *
 * A Material is the shareable, assignable "look" built on a BRDF: it references a
 * MaterialTemplate (the shader + render-state policy + parameter schema) and stores
 * the parameter values (by name) and texture references (reflected sampler name →
 * texture path) that differ from the template's defaults. Objects reference a
 * Material via MaterialComponent; editing a Material affects every object using it
 * (Unity-like shared material). The set of editable parameters comes from the
 * template's schema — add a parameter by editing the template + its shader, no C++
 * change required. Saved to disk as a `.ehmaterial` YAML file.
 *
 * The Material carries NO GPU objects: turning this data into a GPU pipeline +
 * descriptor set is the renderer's job (it alone knows the shader ABI and the
 * scene pass). See Ray/Material/RayMaterialCache. A `Transparent` flag selects the
 * renderer's blended pipeline variant of the template; `Version` lets edits
 * invalidate the renderer's cache and force a live re-pack.
 */

#include "Asset/Asset.hpp"
#include "Asset/Material/MaterialParam.hpp"

#include "Core/UUID.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Echelon {

    class MaterialTemplate;

    class Material : public Asset {
    public:
        // ---- Serialized data ----
        UUID        TemplateHandle = UUID::Null();  ///< Authoritative template (BRDF) reference.
        std::string TemplateSource;                 ///< Readable hint (path/built-in name) for the template.
        std::unordered_map<std::string, MaterialParam> Params;    ///< name → value (sparse; falls through to template default).
        std::unordered_map<std::string, std::string>   Textures;  ///< reflected sampler name → texture asset path.
        bool        Transparent = false;            ///< Per-instance blend flag → the template's blended pipeline variant.

        // ---- Transient ----
        uint64_t    Version = 0;                     ///< Bumped on edit; invalidates the renderer's per-instance GPU cache.

        Material() = default;
        ~Material() override = default;

        AssetType GetType() const override { return AssetType::Material; }

        /** @brief Valid as data once it names a template (GPU validity is the renderer's concern). */
        bool IsValid() const override { return !TemplateHandle.IsNull() || !TemplateSource.empty(); }

        /** @brief Absorb freshly re-imported data (hot-reload); the renderer rebuilds GPU state on the epoch bump. */
        void ReloadFrom(const Ref<Asset>& fresh) override;

        /** @brief Resolve a parameter value: own value → template default → nullptr (unset). */
        const MaterialParam* Resolve(const std::string& name) const;

        void SetParam(const std::string& name, const MaterialParam& value) { Params[name] = value; }

        /** @brief Bump the version — call after changing any value/texture/flag so the renderer re-packs. */
        void Invalidate() { ++Version; }

        /** @brief Resolve (and memoize) the referenced template asset (pure data, via AssetManager). */
        Ref<MaterialTemplate> GetTemplate() const;

    private:
        mutable Ref<MaterialTemplate> m_Template;   ///< lazily resolved template; pure data.
    };

} // namespace Echelon
