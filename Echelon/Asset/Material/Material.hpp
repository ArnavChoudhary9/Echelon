#pragma once

/**
 * @file Material.hpp
 * @brief Material asset — a shader reference + reflection-driven parameter values
 *        + texture references. PURE DATA.
 *
 * A Material references a ShaderAsset and stores parameter values (by name) and
 * texture references (reflected sampler name → texture path). It may reference a
 * parent material (a "saved instance"): unset parameters fall through to the
 * parent, then to the shader default. The set of editable parameters is derived
 * entirely from shader reflection — add a parameter by editing the .slang, no C++
 * change required. Saved to disk as a `.ehmaterial` YAML file.
 *
 * The Material carries NO GPU objects: turning this data into a GPU pipeline +
 * descriptor set is the renderer's job (it alone knows the shader ABI and the
 * scene pass). See Ray/Material/RayMaterialCache.
 */

#include "Asset/Asset.hpp"
#include "Asset/Material/MaterialParam.hpp"

#include "Core/UUID.hpp"

#include <string>
#include <unordered_map>

namespace Echelon {

    class Material : public Asset {
    public:
        // ---- Serialized data ----
        UUID        ShaderHandle = UUID::Null();  ///< Authoritative shader reference.
        std::string ShaderSource;                 ///< Readable hint (path) for the shader.
        UUID        ParentHandle = UUID::Null();  ///< Optional parent material (saved instance).
        std::string ParentSource;
        std::unordered_map<std::string, MaterialParam> Params;    ///< name → value (sparse).
        std::unordered_map<std::string, std::string>   Textures;  ///< reflected sampler name → texture asset path.

        Material() = default;
        ~Material() override = default;

        AssetType GetType() const override { return AssetType::Material; }

        /** @brief Valid as data once it names a shader (GPU validity is the renderer's concern). */
        bool IsValid() const override { return !ShaderHandle.IsNull() || !ShaderSource.empty(); }

        /** @brief Absorb freshly re-imported data (hot-reload); the renderer rebuilds GPU state on the epoch bump. */
        void ReloadFrom(const Ref<Asset>& fresh) override;

        /** @brief Resolve a parameter value: own value → parent → nullptr (unset). */
        const MaterialParam* Resolve(const std::string& name) const;

        void SetParam(const std::string& name, const MaterialParam& value) { Params[name] = value; }

    private:
        mutable Ref<Material> m_Parent;   ///< lazily resolved parent (saved-instance chain); pure data.
    };

} // namespace Echelon
