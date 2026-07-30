#include "Asset/Importers/Scene/SceneImporter.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneSerializer.hpp"
#include "Core/Log.hpp"

namespace Echelon {

    ImportResult SceneImporter::Import(const ImportContext& ctx) {
        auto scene = CreateRef<Scene>();

        SceneSerializer serializer(scene);
        if (!serializer.Deserialize(ctx.GetAbsolutePath())) {
            ECHELON_LOG_ERROR("[SceneImporter] Failed to deserialize scene: {}", ctx.GetPathString());
            return ImportResult(std::string("Failed to deserialize scene: ") + ctx.GetPathString());
        }

        ECHELON_LOG_INFO("[SceneImporter] Loaded scene: {}", ctx.GetPathString());
        return ImportResult(scene);
    }

} // namespace Echelon
