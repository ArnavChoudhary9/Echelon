#include "Asset/Material/Material.hpp"
#include "Asset/Material/MaterialTemplate.hpp"
#include "Asset/AssetManager.hpp"

#include <memory>

namespace Echelon {

    Ref<MaterialTemplate> Material::GetTemplate() const {
        if (m_Template) return m_Template;

        auto& assets = AssetManager::Get();
        if (!TemplateHandle.IsNull())
            m_Template = assets.GetAssetAs<MaterialTemplate>(TemplateHandle);
        if (!m_Template && !TemplateSource.empty()) {
            UUID h = assets.GetHandle(TemplateSource);
            if (!h.IsNull()) m_Template = assets.GetAssetAs<MaterialTemplate>(h);
        }
        return m_Template;
    }

    const MaterialParam* Material::Resolve(const std::string& name) const {
        auto it = Params.find(name);
        if (it != Params.end()) return &it->second;

        // Fall through to the template's declared default (pure data, no GPU work).
        if (auto tmpl = GetTemplate())
            return tmpl->ResolveDefault(name);
        return nullptr;
    }

    void Material::ReloadFrom(const Ref<Asset>& fresh) {
        auto other = std::dynamic_pointer_cast<Material>(fresh);
        if (!other) return;
        TemplateHandle = other->TemplateHandle;
        TemplateSource = other->TemplateSource;
        Params         = other->Params;
        Textures       = other->Textures;
        Transparent    = other->Transparent;
        m_Template     = nullptr;   // re-resolved lazily against the new data
        ++Version;                  // force the renderer's per-instance cache to re-pack
    }

} // namespace Echelon
