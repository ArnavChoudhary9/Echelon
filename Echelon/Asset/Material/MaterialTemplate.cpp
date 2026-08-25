#include "Asset/Material/MaterialTemplate.hpp"

#include <memory>

namespace Echelon {

    void MaterialTemplate::ReloadFrom(const Ref<Asset>& fresh) {
        auto other = std::dynamic_pointer_cast<MaterialTemplate>(fresh);
        if (!other) return;
        ShaderHandle = other->ShaderHandle;
        ShaderSource = other->ShaderSource;
        Cull         = other->Cull;
        Winding      = other->Winding;
        DepthTest    = other->DepthTest;
        DepthWrite   = other->DepthWrite;
        Topology     = other->Topology;
        Params       = other->Params;
        Textures     = other->Textures;
    }

} // namespace Echelon
