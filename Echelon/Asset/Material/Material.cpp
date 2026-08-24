#include "Asset/Material/Material.hpp"
#include "Asset/AssetManager.hpp"

#include <memory>

namespace Echelon {

    const MaterialParam* Material::Resolve(const std::string& name) const {
        auto it = Params.find(name);
        if (it != Params.end()) return &it->second;

        // Lazily resolve the parent (saved-instance) chain — pure data, via the
        // AssetManager. No GPU work happens here.
        if (!m_Parent && !ParentHandle.IsNull())
            m_Parent = AssetManager::Get().GetAssetAs<Material>(ParentHandle);
        if (m_Parent) return m_Parent->Resolve(name);
        return nullptr;
    }

    void Material::ReloadFrom(const Ref<Asset>& fresh) {
        auto other = std::dynamic_pointer_cast<Material>(fresh);
        if (!other) return;
        ShaderHandle = other->ShaderHandle;
        ShaderSource = other->ShaderSource;
        ParentHandle = other->ParentHandle;
        ParentSource = other->ParentSource;
        Params       = other->Params;
        Textures     = other->Textures;
        m_Parent     = nullptr;   // re-resolved lazily against the new data
    }

} // namespace Echelon
