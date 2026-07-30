#include "Asset/AssetManager.hpp"

#include "Asset/AssetMetadata.hpp"
#include "Asset/Importers/Importer.hpp"
#include "Asset/Importers/ImportContext.hpp"
#include "Asset/Importers/ImportResult.hpp"
#include "Asset/Importers/OBJ/OBJImporter.hpp"
#include "Asset/Importers/Scene/SceneImporter.hpp"
#include "Asset/Mesh/Mesh.hpp"
#include "Asset/Mesh/Primitives.hpp"

#include "Renderer/RendererService.hpp"
#include "Project/Project.hpp"
#include "Core/Log.hpp"

#include <algorithm>
#include <cctype>

namespace Echelon {

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------
    static std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    AssetManager& AssetManager::Get() {
        static AssetManager s_Instance;
        return s_Instance;
    }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------
    void AssetManager::Init() {
        // Engine loader back-ends.
        RegisterImporter(CreateRef<OBJImporter>());
        RegisterImporter(CreateRef<SceneImporter>());

        // Procedural built-in shapes ("internal shape repository").
        RegisterPrimitive("Cube", []() -> Ref<Asset> { return MeshPrimitives::CreateCube(); });

        // Rebuild GPU resources whenever the active renderer (back-end) changes.
        m_RendererListener = Renderer::Get().AddChangeListener(
            [this](RendererAPI* r) { OnRendererChanged(r); });

        ECHELON_LOG_INFO("[Asset] AssetManager initialized ({} extension handlers).",
                         m_ImportersByExt.size());
    }

    void AssetManager::Shutdown() {
        if (m_RendererListener) {
            Renderer::Get().RemoveChangeListener(m_RendererListener);
            m_RendererListener = 0;
        }
        // Release GPU handles while the GL context is still alive.
        for (auto& [handle, asset] : m_Loaded) {
            if (asset) asset->ReleaseGPU();
        }
        m_Loaded.clear();
    }

    void AssetManager::Update() {
        // Collect first, reload after — reloading mutates the registry/cache.
        std::vector<UUID> toReload;
        for (const auto& [handle, meta] : m_Registry.GetAll()) {
            if (meta.IsMemoryOnly || !meta.WatchForChanges) continue;
            if (m_Loaded.find(handle) == m_Loaded.end()) continue; // only watch loaded assets

            std::error_code ec;
            auto now = fs::last_write_time(meta.FilePath, ec);
            if (ec) continue;
            if (now != meta.LastWriteTime) toReload.push_back(handle);
        }

        for (const auto& handle : toReload)
            ReloadAsset(handle);
    }

    void AssetManager::RefreshRegistry(const fs::path& directory) {
        std::error_code ec;
        if (!fs::exists(directory, ec)) return;

        size_t count = 0;
        for (fs::recursive_directory_iterator it(directory, ec), end; it != end; it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file(ec)) continue;

            const fs::path& metaPath = it->path();
            if (ToLower(metaPath.extension().string()) != ".meta") continue;

            // The asset lives beside its sidecar: "<asset>.meta" -> "<asset>".
            fs::path assetPath = metaPath;
            assetPath.replace_extension();
            if (assetPath.empty() || !fs::exists(assetPath, ec)) continue;

            // ImportAsset dedups by path, but skip the extra work when known.
            if (!m_Registry.GetHandleFromPath(assetPath.string()).IsNull()) continue;

            if (!ImportAsset(assetPath).IsNull())
                ++count;
        }

        if (count > 0)
            ECHELON_LOG_INFO("[Asset] Registered {} asset(s) from '{}'.", count, directory.string());
    }

    // ------------------------------------------------------------------
    // Extension points
    // ------------------------------------------------------------------
    void AssetManager::RegisterImporter(const Ref<AssetImporter>& importer) {
        if (!importer) return;
        for (const auto& ext : importer->GetSupportedExtensions())
            m_ImportersByExt[ToLower(ext)] = importer;
    }

    UUID AssetManager::RegisterPrimitive(const std::string& name, std::function<Ref<Asset>()> generator) {
        UUID handle = UUID::FromName("builtin:" + name);
        m_PrimitiveHandles[name]      = handle;
        m_PrimitiveGenerators[handle] = std::move(generator);

        AssetMetadata meta;
        meta.Handle          = handle;
        meta.Type            = AssetType::Mesh; // primitives are meshes for now
        meta.IsMemoryOnly    = true;
        meta.WatchForChanges = false;
        m_Registry.SetMetadata(meta);

        return handle;
    }

    // ------------------------------------------------------------------
    // Resolution
    // ------------------------------------------------------------------
    UUID AssetManager::GetHandle(const std::string& source) {
        if (source.empty()) return UUID::Null();

        // 1) Built-in primitive by name?
        auto pit = m_PrimitiveHandles.find(source);
        if (pit != m_PrimitiveHandles.end())
            return pit->second;

        // 2) Otherwise treat as a file path (relative to the project's Assets dir).
        fs::path path(source);
        if (!path.is_absolute()) {
            if (auto project = Project::GetActive())
                path = project->GetAssetsDirectory() / path;
        }

        std::error_code ec;
        fs::path abs = fs::absolute(path, ec);
        if (ec) abs = path;

        if (!fs::exists(abs, ec)) {
            ECHELON_LOG_ERROR("[Asset] Source not found: '{}' (resolved: {})", source, abs.string());
            return UUID::Null();
        }
        return ImportAsset(abs);
    }

    UUID AssetManager::ImportAsset(const fs::path& absolutePath) {
        const std::string key = absolutePath.string();

        // Already registered for this path?
        UUID existing = m_Registry.GetHandleFromPath(key);
        if (!existing.IsNull())
            return existing;

        fs::path metaPath = absolutePath;
        metaPath += ".meta";

        AssetMetadata meta;
        if (auto loaded = LoadMeta(metaPath))
            meta = *loaded;
        else
            meta.Handle = UUID(); // fresh random handle for a new asset

        meta.FilePath     = absolutePath;
        meta.IsMemoryOnly = false;

        // Type is authoritative from the importer that handles this extension.
        const std::string ext = ToLower(absolutePath.extension().string());
        auto iit = m_ImportersByExt.find(ext);
        if (iit != m_ImportersByExt.end())
            meta.Type = iit->second->GetAssetType();

        // Scenes are not auto-hot-reloaded (would clobber in-editor edits).
        if (meta.Type == AssetType::Scene)
            meta.WatchForChanges = false;

        std::error_code ec;
        meta.LastWriteTime = fs::last_write_time(absolutePath, ec);

        m_Registry.SetMetadata(meta);

        // Persist the sidecar so the handle is stable across runs.
        if (!fs::exists(metaPath, ec))
            SaveMeta(meta, metaPath);

        return meta.Handle;
    }

    // ------------------------------------------------------------------
    // Loading
    // ------------------------------------------------------------------
    Ref<Asset> AssetManager::LoadFromFile(const AssetMetadata& meta) {
        const std::string ext = ToLower(meta.FilePath.extension().string());
        auto it = m_ImportersByExt.find(ext);
        if (it == m_ImportersByExt.end()) {
            ECHELON_LOG_ERROR("[Asset] No importer for extension '{}' ({}).", ext, meta.FilePath.string());
            return nullptr;
        }

        ImportContext ctx(meta.FilePath);
        ImportResult  result = it->second->Import(ctx);
        if (!result.IsSuccess() || !result.GetAsset()) {
            ECHELON_LOG_ERROR("[Asset] Import failed for '{}': {}", meta.FilePath.string(), result.GetMessage());
            return nullptr;
        }
        return result.GetAsset();
    }

    Ref<Asset> AssetManager::GetAsset(const UUID& handle, bool forceReload) {
        if (handle.IsNull()) return nullptr;

        if (!forceReload) {
            auto it = m_Loaded.find(handle);
            if (it != m_Loaded.end())
                return it->second;
        }

        Ref<Asset> asset;

        // Procedural primitive?
        auto git = m_PrimitiveGenerators.find(handle);
        if (git != m_PrimitiveGenerators.end()) {
            asset = git->second ? git->second() : nullptr;
            if (!asset) {
                ECHELON_LOG_ERROR("[Asset] Primitive generator returned null ({}).", handle.ToString());
                return nullptr;
            }
        } else {
            const AssetMetadata* meta = m_Registry.GetMetadata(handle);
            if (!meta) {
                ECHELON_LOG_ERROR("[Asset] Unknown asset handle: {}", handle.ToString());
                return nullptr;
            }
            asset = LoadFromFile(*meta);
            if (!asset) return nullptr; // already logged
        }

        asset->Handle = handle;

        // Realize GPU resources against the active renderer, if any.
        if (auto* renderer = Renderer::Get().GetActive())
            asset->UploadGPU(renderer);

        m_Loaded[handle] = asset;
        return asset;
    }

    Ref<Mesh> AssetManager::GetMesh(const UUID& handle) {
        return std::dynamic_pointer_cast<Mesh>(GetAsset(handle));
    }

    Ref<Mesh> AssetManager::GetMesh(const std::string& source) {
        return GetMesh(GetHandle(source));
    }

    bool AssetManager::ReloadAsset(const UUID& handle) {
        AssetMetadata* meta = m_Registry.GetMetadata(handle);
        if (!meta || meta->IsMemoryOnly) return false;

        Ref<Asset> fresh = LoadFromFile(*meta);
        if (!fresh) return false; // keep the previous asset on failure

        auto it = m_Loaded.find(handle);
        if (it != m_Loaded.end() && it->second) {
            // Update in place so any held Ref stays valid.
            it->second->ReloadFrom(fresh);
            if (auto* renderer = Renderer::Get().GetActive())
                it->second->UploadGPU(renderer);
        } else {
            fresh->Handle = handle;
            if (auto* renderer = Renderer::Get().GetActive())
                fresh->UploadGPU(renderer);
            m_Loaded[handle] = fresh;
        }

        std::error_code ec;
        meta->LastWriteTime = fs::last_write_time(meta->FilePath, ec);

        ++m_Epoch;
        ECHELON_LOG_INFO("[Asset] Hot-reloaded: {}", meta->FilePath.string());
        return true;
    }

    // ------------------------------------------------------------------
    // Renderer hot-swap
    // ------------------------------------------------------------------
    void AssetManager::OnRendererChanged(RendererAPI* renderer) {
        if (!renderer) return;

        for (auto& [handle, asset] : m_Loaded) {
            if (!asset) continue;
            asset->ReleaseGPU();
            asset->UploadGPU(renderer);
        }

        ++m_Epoch;
        if (!m_Loaded.empty())
            ECHELON_LOG_INFO("[Asset] Rebuilt GPU resources for {} assets (renderer changed).",
                             m_Loaded.size());
    }

    // ------------------------------------------------------------------
    // Introspection
    // ------------------------------------------------------------------
    std::vector<UUID> AssetManager::GetAssetsByType(AssetType type) const {
        std::vector<UUID> result;
        for (const auto& [handle, meta] : m_Registry.GetAll())
            if (meta.Type == type) result.push_back(handle);
        return result;
    }

} // namespace Echelon
