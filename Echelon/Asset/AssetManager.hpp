#pragma once

/**
 * @file AssetManager.hpp
 * @brief Central asset registry, loader, cache, and GPU-lifecycle owner (singleton).
 *
 * Responsibilities:
 *  - Resolve a source (built-in name or file path) to a stable UUID handle.
 *  - Load an asset on demand, auto-selecting an importer by file extension.
 *  - Cache loaded assets (UUID -> Ref<Asset>).
 *  - Own the GPU lifecycle: upload on load, rebuild on renderer hot-swap, and
 *    rebuild on file hot-reload — all driven through Asset's polymorphic virtuals
 *    so the manager never names a concrete type.
 *
 * Extension points (engine or user code):
 *  - RegisterImporter(...)  — add a loader back-end for new file extensions.
 *  - RegisterPrimitive(...) — add a procedural built-in shape.
 *  A new asset type (e.g. Texture) needs only a new Asset subclass + importer +
 *  one RegisterImporter call — no changes to this class.
 */

#include "Core/Base.hpp"
#include "Core/UUID.hpp"
#include "Asset/Asset.hpp"
#include "Asset/Registry.hpp"

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <cstdint>

namespace Echelon {

    class AssetImporter; // fwd
    class Mesh;          // fwd
    class RendererAPI;   // fwd

    class AssetManager {
    public:
        /** @brief Access the singleton instance. */
        static AssetManager& Get();

        // ---- Engine-internal lifecycle (called by Application) ----

        /** @brief Register engine importers + built-in primitives and hook renderer changes. */
        void Init();

        /** @brief Release cached assets' GPU resources and unhook. Call before renderer shutdown. */
        void Shutdown();

        /** @brief Poll watched file assets for changes and hot-reload them. Call once per frame. */
        void Update();

        /**
         * @brief Seed the registry from `.meta` sidecars found under a directory.
         *
         * A handle persisted in a scene (MeshComponent::MeshHandle, etc.) is only
         * resolvable once its metadata is known. Path-based imports register it
         * lazily, but a scene loaded straight from its handle would otherwise fail.
         * Scanning the project's asset tree on open makes every persisted handle
         * resolvable up front. Recursive; type-agnostic; safe to call repeatedly.
         */
        void RefreshRegistry(const fs::path& directory);

        // ---- Extension points ----

        /** @brief Register a loader back-end (keyed by the extensions it reports). */
        void RegisterImporter(const Ref<AssetImporter>& importer);

        /** @brief Register a procedural built-in asset generator; returns its stable handle. */
        UUID RegisterPrimitive(const std::string& name, std::function<Ref<Asset>()> generator);

        // ---- Resolution / loading ----

        /**
         * @brief Resolve a source string to a handle.
         * @param source A built-in primitive name (e.g. "Cube") or a path relative
         *               to the active project's Assets directory (or absolute).
         * @return A handle, or UUID::Null() if it cannot be resolved (logged).
         */
        UUID GetHandle(const std::string& source);

        /**
         * @brief Get (loading + GPU-uploading on first use) the asset for a handle.
         * @return The cached asset, or nullptr on failure (logged — never throws).
         */
        Ref<Asset> GetAsset(const UUID& handle, bool forceReload = false);

        template<class T>
        Ref<T> GetAssetAs(const UUID& handle, bool forceReload = false) {
            return std::dynamic_pointer_cast<T>(GetAsset(handle, forceReload));
        }

        Ref<Mesh> GetMesh(const UUID& handle);
        Ref<Mesh> GetMesh(const std::string& source);

        /** @brief Re-import a file asset in place (hot-reload); bumps the epoch. */
        bool ReloadAsset(const UUID& handle);

        // ---- Introspection ----

        /** @brief Monotonic counter bumped on every GPU rebuild / hot-reload. */
        uint64_t GetEpoch() const { return m_Epoch; }

        std::vector<UUID> GetAssetsByType(AssetType type) const;

    private:
        AssetManager() = default;
        ~AssetManager() = default;
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        /** @brief Ensure a file asset is known (create/read its .meta); returns its handle. */
        UUID ImportAsset(const fs::path& absolutePath);

        /** @brief Run the extension-matched importer for a file asset. */
        Ref<Asset> LoadFromFile(const AssetMetadata& meta);

        /** @brief Renderer hot-swap handler: rebuild all cached assets' GPU resources. */
        void OnRendererChanged(RendererAPI* renderer);

        AssetRegistry                        m_Registry;
        std::unordered_map<UUID, Ref<Asset>> m_Loaded;
        std::unordered_map<std::string, Ref<AssetImporter>> m_ImportersByExt;

        std::unordered_map<std::string, UUID>                  m_PrimitiveHandles;    // name -> handle
        std::unordered_map<UUID, std::function<Ref<Asset>()>>  m_PrimitiveGenerators; // handle -> generator

        uint64_t m_Epoch            = 0;
        uint32_t m_RendererListener = 0;
    };

} // namespace Echelon
