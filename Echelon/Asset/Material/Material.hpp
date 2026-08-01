#pragma once

/**
 * @file Material.hpp
 * @brief Material asset — binds a shader + reflection-driven parameters + textures.
 *
 * A Material references a ShaderAsset and stores parameter values (packed into the
 * shader's reflected material UBO) and texture references. It may reference a
 * parent material (a "saved instance"): unset parameters fall through to the
 * parent, then to the shader default. The set of editable parameters is derived
 * entirely from shader reflection — add a parameter by editing the .slang, no C++
 * change required. Saved to disk as a `.ehmaterial` YAML file.
 */

#include "Asset/Asset.hpp"
#include "Asset/Shader/ShaderAsset.hpp"
#include "Asset/Material/MaterialParam.hpp"
#include "Asset/Material/MaterialResources.hpp"

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
        std::unordered_map<std::string, std::string>   Textures;  ///< sampler name → texture path (future).

        Material() = default;
        ~Material() override = default;

        AssetType GetType() const override { return AssetType::Material; }
        bool IsValid() const override { return m_Pipeline != nullptr; }

        void UploadGPU(RendererAPI* renderer) override;
        void ReleaseGPU() override;
        void ReloadFrom(const Ref<Asset>& fresh) override;

        /** @brief Re-pack current parameter values into the GPU UBO (after edits). */
        void Repack();

        // ---- Accessors ----
        const Ref<Pipeline>&      GetPipeline() const      { return m_Pipeline; }
        const Ref<DescriptorSet>& GetDescriptorSet() const { return m_Resources.Set; }
        const Ref<ShaderAsset>&   GetShaderAsset() const   { return m_Shader; }

        /** @brief Resolve a parameter value: own value → parent → nullptr (unset). */
        const MaterialParam* Resolve(const std::string& name) const;

        void SetParam(const std::string& name, const MaterialParam& value) { Params[name] = value; }

    private:
        Ref<ShaderAsset>     m_Shader;
        Ref<Material>        m_Parent;
        Ref<Pipeline>        m_Pipeline;
        Ref<Texture>         m_DefaultTexture;
        MaterialGpuResources m_Resources;
    };

} // namespace Echelon
