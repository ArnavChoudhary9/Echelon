#pragma once

/**
 * @file Registry.hpp
 * @brief In-memory table of known assets: UUID -> AssetMetadata (+ a path index).
 *
 * Pure metadata bookkeeping — no loading, no GPU, and fully type-agnostic.
 */

#include "Core/Base.hpp"
#include "Core/UUID.hpp"
#include "Asset/AssetMetadata.hpp"

#include <string>
#include <unordered_map>

namespace Echelon {

    class AssetRegistry {
    public:
        void SetMetadata(const AssetMetadata& meta) {
            m_Assets[meta.Handle] = meta;
            if (!meta.FilePath.empty())
                m_PathIndex[meta.FilePath.string()] = meta.Handle;
        }

        bool Contains(const UUID& handle) const {
            return m_Assets.find(handle) != m_Assets.end();
        }

        const AssetMetadata* GetMetadata(const UUID& handle) const {
            auto it = m_Assets.find(handle);
            return it != m_Assets.end() ? &it->second : nullptr;
        }

        AssetMetadata* GetMetadata(const UUID& handle) {
            auto it = m_Assets.find(handle);
            return it != m_Assets.end() ? &it->second : nullptr;
        }

        /** @brief Handle previously registered for an absolute path, or Null. */
        UUID GetHandleFromPath(const std::string& path) const {
            auto it = m_PathIndex.find(path);
            return it != m_PathIndex.end() ? it->second : UUID::Null();
        }

        const std::unordered_map<UUID, AssetMetadata>& GetAll() const { return m_Assets; }

    private:
        std::unordered_map<UUID, AssetMetadata> m_Assets;
        std::unordered_map<std::string, UUID>   m_PathIndex;
    };

} // namespace Echelon
