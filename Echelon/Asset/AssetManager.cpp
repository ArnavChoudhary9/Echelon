#include "Asset/AssetManager.hpp"

#include "Asset/AssetMetadata.hpp"
#include "Asset/Importers/Importer.hpp"
#include "Asset/Importers/ImportContext.hpp"
#include "Asset/Importers/ImportResult.hpp"
#include "Asset/Importers/OBJ/OBJImporter.hpp"
#include "Asset/Importers/Scene/SceneImporter.hpp"
#include "Asset/Importers/Shader/ShaderImporter.hpp"
#include "Asset/Importers/Material/MaterialImporter.hpp"
#include "Asset/Importers/Texture/TextureImporter.hpp"
#include "Asset/Importers/RenderPipeline/RenderPipelineImporter.hpp"
#include "Asset/Mesh/Mesh.hpp"
#include "Asset/Mesh/Primitives.hpp"
#include "Asset/Material/Material.hpp"

#include "Renderer/RendererService.hpp"
#include "Renderer/RendererLoader.hpp"   // ExecutableDir() for the built-in shader path
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
        RegisterImporter(CreateRef<ShaderImporter>());
        RegisterImporter(CreateRef<MaterialImporter>());
        RegisterImporter(CreateRef<TextureImporter>());
        RegisterImporter(CreateRef<RenderPipelineImporter>());

        // Procedural built-in shapes ("internal shape repository").
        RegisterPrimitive("Cube",   []() -> Ref<Asset> { return MeshPrimitives::CreateCube(); });
        RegisterPrimitive("Plane",  []() -> Ref<Asset> { return MeshPrimitives::CreatePlane(); });
        RegisterPrimitive("Sphere", []() -> Ref<Asset> { return MeshPrimitives::CreateSphere(); });

        // Built-in default material — backed by whatever shader the active renderer
        // declares as its default (now PBR.slang). Resolved lazily so the renderer is
        // guaranteed to be initialised before this lambda first runs. PBR requires its
        // params to be set (unset members zero-fill → black/occluded), so seed sane ones.
        RegisterPrimitive("DefaultMaterial", []() -> Ref<Asset> {
            auto mat = CreateRef<Material>();
            if (auto* r = Renderer::Get().GetActive())
                mat->ShaderSource = (RendererLoader::ExecutableDir() / "Shaders" / r->GetDefaultShaderName()).string();
            mat->Params["BaseColor"] = MaterialParam::Make(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
            mat->Params["Metallic"]  = MaterialParam::Make(0.0f);
            mat->Params["Roughness"] = MaterialParam::Make(0.6f);
            mat->Params["Occlusion"] = MaterialParam::Make(1.0f);
            return mat;
        });

        // Widely-used built-in materials, all backed by the standard PBR shader
        // (shipped next to the executable). Projects reference these by name
        // (MaterialSource: "PBR" / "Albedo" / "Textured") or author .ehmaterial files
        // that use `shader:PBR.slang` with their own params.
        auto makePbr = [](glm::vec4 baseColor, float metallic, float roughness, bool useAlbedoMap) {
            auto mat = CreateRef<Material>();
            mat->ShaderSource = (RendererLoader::ExecutableDir() / "Shaders" / "PBR.slang").string();
            mat->Params["BaseColor"]    = MaterialParam::Make(baseColor);
            mat->Params["Metallic"]     = MaterialParam::Make(metallic);
            mat->Params["Roughness"]    = MaterialParam::Make(roughness);
            mat->Params["Occlusion"]    = MaterialParam::Make(1.0f);
            mat->Params["NormalScale"]  = MaterialParam::Make(1.0f);
            if (useAlbedoMap) mat->Params["UseAlbedoMap"] = MaterialParam::Make(1.0f);
            return mat;
        };
        RegisterPrimitive("PBR",      [makePbr]() -> Ref<Asset> { return makePbr(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.0f, 0.5f, false); });
        RegisterPrimitive("Albedo",   [makePbr]() -> Ref<Asset> { return makePbr(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), 0.0f, 0.5f, false); });
        RegisterPrimitive("Textured", [makePbr]() -> Ref<Asset> { return makePbr(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 0.0f, 0.5f, true);  });

        // Rebuild GPU resources whenever the active renderer (back-end) changes.
        m_RendererListener = Renderer::Get().AddChangeListener(
            [this](RendererAPI* r) { OnRendererChanged(r); });

#ifndef ECHELON_DIST
        m_Watcher.Start();
#endif

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

#ifndef ECHELON_DIST
        m_Watcher.Stop();
#endif
    }

#ifndef ECHELON_DIST
    void AssetManager::Update() {
        for (const auto& path : m_Watcher.Poll()) {
            UUID handle = m_Registry.GetHandleFromPath(path.string());
            if (!handle.IsNull())
                ReloadAsset(handle);
        }
    }
#endif

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

        // 2) "shader:" prefix → renderer shader, resolved against <exe>/Shaders/.
        //    Use this in material files to reference renderer-owned shaders without
        //    coupling them to the project's Assets directory.
        constexpr std::string_view kShaderPrefix = "shader:";
        if (source.size() > kShaderPrefix.size() &&
            source.compare(0, kShaderPrefix.size(), kShaderPrefix) == 0) {
            return ImportAsset(fs::absolute(
                RendererLoader::ExecutableDir() / "Shaders" / source.substr(kShaderPrefix.size())));
        }

        // 3) File path (relative → anchored to the project's Assets dir; absolute → as-is).
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

#ifndef ECHELON_DIST
        if (!meta.IsMemoryOnly && meta.WatchForChanges)
            m_Watcher.Watch(absolutePath);
#endif

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
