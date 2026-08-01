#pragma once

/**
 * @file MaterialInstance.hpp
 * @brief Lightweight runtime override layer over a base Material.
 *
 * A MaterialInstance stores only the parameters it overrides; everything else
 * falls through to the base Material (then the shader default). It shares the
 * base's pipeline (same shader/state) but owns its own parameter UBO + descriptor
 * set, so per-entity variation is cheap. Instances are not files — their sparse
 * overrides are serialized inline wherever they are used (e.g. MaterialComponent).
 */

#include "Asset/Material/Material.hpp"
#include "Asset/Material/MaterialParam.hpp"
#include "Asset/Material/MaterialResources.hpp"

#include <string>
#include <unordered_map>

namespace Echelon {

    class MaterialInstance {
    public:
        MaterialInstance() = default;
        explicit MaterialInstance(const Ref<Material>& base) : m_Base(base) {}

        void SetBase(const Ref<Material>& base) { m_Base = base; }
        const Ref<Material>& GetBase() const { return m_Base; }

        void SetOverride(const std::string& name, const MaterialParam& value) { m_Overrides[name] = value; }
        void ClearOverride(const std::string& name) { m_Overrides.erase(name); }
        const std::unordered_map<std::string, MaterialParam>& GetOverrides() const { return m_Overrides; }

        /** @brief Build this instance's own param UBO + descriptor set from the base's shader. */
        void Build(RendererAPI* renderer) {
            if (!m_Base) return;
            m_Base->UploadGPU(renderer);
            auto shader = m_Base->GetShaderAsset();
            if (!shader) return;
            m_Resources = BuildMaterialResources(renderer, shader->GetReflection(),
                                                 /*fallbackTexture*/ nullptr);
            Repack();
        }

        /** @brief Re-pack resolved values (override → base → default) into the UBO. */
        void Repack() {
            PackMaterialResources(m_Resources, [this](const std::string& name) -> const MaterialParam* {
                auto it = m_Overrides.find(name);
                if (it != m_Overrides.end()) return &it->second;
                return m_Base ? m_Base->Resolve(name) : nullptr;
            });
        }

        Ref<Pipeline> GetPipeline() const { return m_Base ? m_Base->GetPipeline() : nullptr; }

        /** @brief This instance's own set if it has params; otherwise the base's set. */
        Ref<DescriptorSet> GetDescriptorSet() const {
            if (m_Resources.Set) return m_Resources.Set;
            return m_Base ? m_Base->GetDescriptorSet() : nullptr;
        }

        bool IsValid() const { return m_Base && m_Base->IsValid(); }

    private:
        Ref<Material>        m_Base;
        std::unordered_map<std::string, MaterialParam> m_Overrides;
        MaterialGpuResources m_Resources;
    };

} // namespace Echelon
