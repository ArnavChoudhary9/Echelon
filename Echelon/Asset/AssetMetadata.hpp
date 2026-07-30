#pragma once

/**
 * @file AssetMetadata.hpp
 * @brief Per-asset bookkeeping + .meta sidecar (de)serialization.
 *
 * Every file-backed asset gets a stable UUID persisted in a `<file>.meta`
 * sidecar, so references survive renames/moves and re-runs. Procedural assets
 * (built-in primitives) are memory-only and use deterministic handles instead.
 */

#include "Core/Base.hpp"
#include "Core/UUID.hpp"
#include "Asset/Asset.hpp"

#include <optional>

namespace Echelon {

    struct AssetMetadata {
        UUID      Handle       = UUID::Null();
        AssetType Type         = AssetType::None;
        fs::path  FilePath;                       ///< Absolute path on disk; empty for procedural assets.
        bool      IsMemoryOnly = false;           ///< Procedural / built-in (no file, no .meta).
        bool      WatchForChanges = true;         ///< Hot-reload polling opt-out (e.g. scenes).
        fs::file_time_type LastWriteTime{};       ///< Last-seen mtime, for hot-reload detection.

        bool IsValid() const { return !Handle.IsNull(); }
    };

    /** @brief Write a .meta sidecar (Handle + Type). Returns false on I/O error (logged). */
    bool SaveMeta(const AssetMetadata& meta, const fs::path& metaPath);

    /** @brief Read a .meta sidecar. Returns nullopt if missing or unparseable (logged). */
    std::optional<AssetMetadata> LoadMeta(const fs::path& metaPath);

} // namespace Echelon
