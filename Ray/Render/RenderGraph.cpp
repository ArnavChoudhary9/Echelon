#include "RenderGraph.hpp"

#include "Scene/Scene.hpp"
#include "ECS/Components.hpp"
#include "Scene/TransformUtils.hpp"
#include "GraphicsAPI/Pipeline.hpp"
#include "Asset/AssetManager.hpp"
#include "Asset/Mesh/Mesh.hpp"
#include "Asset/Material/Material.hpp"
#include "Material/RayMaterialCache.hpp"   // renderer-owned material→GPU translation
#include "Core/Log.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

#include <algorithm>
#include <functional>
#include <cstring>
#include <unordered_map>

namespace Echelon {

    // ------------------------------------------------------------------
    // Version hashing — produces a lightweight fingerprint of all
    // renderable state so we can skip rebuilds when nothing changed.
    // ------------------------------------------------------------------

    static uint64_t HashCombine(uint64_t seed, uint64_t value) {
        // FNV-style combine
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }

    uint64_t RenderGraph::ComputeSceneVersion(const Ref<Scene>& scene) const {
        uint64_t version = 0;

        // Fold in the asset epoch so a renderer hot-swap or asset hot-reload
        // (which rebuilds GPU buffers) forces exactly one graph rebuild.
        version = HashCombine(version, AssetManager::Get().GetEpoch());

        // Fold in the scene identity so swapping to a *different* scene object always
        // rebuilds — even if its contents (UUIDs/transforms) are identical (e.g. an
        // editor play-mode copy). Without this the draw list could keep the previous
        // scene's per-entity data.
        version = HashCombine(version, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(scene.get())));

        auto registry = scene->GetEntityRegistry().lock();
        if (!registry) return version;

        // Hash EVERY entity that carries a transform — not just renderables. A child's
        // world transform depends on its ancestors, and those ancestors may be empty
        // pivot entities with no mesh; folding all transforms in (plus each entity's
        // parent link) guarantees a rebuild whenever an ancestor moves or the hierarchy
        // is re-parented.
        auto view = registry->view<IDComponent, TransformComponent>();
        for (auto&& [entity, id, tc] : view.each()) {
            version = HashCombine(version, id.ID.Hash());

            // Include transform in version (bit-cast floats)
            uint32_t px, py, pz, rx, ry, rz, sx, sy, sz;
            std::memcpy(&px, &tc.Position.x, 4); std::memcpy(&py, &tc.Position.y, 4); std::memcpy(&pz, &tc.Position.z, 4);
            std::memcpy(&rx, &tc.Rotation.x, 4); std::memcpy(&ry, &tc.Rotation.y, 4); std::memcpy(&rz, &tc.Rotation.z, 4);
            std::memcpy(&sx, &tc.Scale.x,    4); std::memcpy(&sy, &tc.Scale.y,    4); std::memcpy(&sz, &tc.Scale.z,    4);
            version = HashCombine(version, px); version = HashCombine(version, py); version = HashCombine(version, pz);
            version = HashCombine(version, rx); version = HashCombine(version, ry); version = HashCombine(version, rz);
            version = HashCombine(version, sx); version = HashCombine(version, sy); version = HashCombine(version, sz);

            // Fold in the parent link so re-parenting forces a rebuild.
            if (const auto* rel = registry->try_get<RelationshipComponent>(entity))
                if (rel->Parent.has_value())
                    version = HashCombine(version, rel->Parent->Hash());

            // Include mesh + material versions for renderables.
            if (const auto* mc = registry->try_get<MeshComponent>(entity))
                version = HashCombine(version, mc->Version);
            if (const auto* mat = registry->try_get<MaterialComponent>(entity))
                version = HashCombine(version, mat->Version);
        }

        return version;
    }

    // ------------------------------------------------------------------
    // Update — main entry point, called once per frame
    // ------------------------------------------------------------------

    void RenderGraph::Update(const Ref<Scene>& scene,
                              RendererAPI* renderer,
                              RayMaterialCache& cache,
                              const Ref<Pipeline>& defaultPipeline,
                              const Ref<Pipeline>& errorPipeline,
                              const Ref<RenderPass>& scenePass) {
        m_WasRebuilt = false;

        if (!scene) {
            m_DrawCommands.clear();
            m_PipelineGroups.clear();
            return;
        }

        uint64_t currentVersion = ComputeSceneVersion(scene);

        if (!m_IsDirty && currentVersion == m_LastSceneVersion) {
            return; // Nothing changed — O(1) early-out
        }

        Rebuild(scene, renderer, cache, defaultPipeline, errorPipeline, scenePass);
        SortAndBatch();

        m_LastSceneVersion = currentVersion;
        m_IsDirty    = false;
        m_WasRebuilt = true;
    }

    // ------------------------------------------------------------------
    // Rebuild — flatten scene graph into draw commands
    // ------------------------------------------------------------------

    // Resolve a MaterialComponent's asset reference (once per epoch) and, when its
    // material was (re)resolved — an override edit or an asset epoch bump — (re)build
    // the per-entity override descriptor set in the renderer's material cache.
    // Mirrors the mesh resolution: lazy, self-healing via source. Returns the resolved
    // base material (or null).
    //
    // The engine does NOT invent a material for meshes that have none — applying a
    // standard or custom material is the renderer's/user's job. An unresolved material
    // returns null, so the caller falls back to the renderer's pink error pipeline
    // (a clear "no material applied / something is wrong" signal).
    static Ref<Material> ResolveMaterial(uint32_t entityId, MaterialComponent& mc,
                                         uint64_t epoch, RayMaterialCache& cache,
                                         RendererAPI* renderer) {
        if (mc.MaterialHandle.IsNull() && mc.MaterialSource.empty())
            return nullptr; // no material applied → renderer's error/pink fallback

        if (mc.ResolveEpoch != epoch) {
            mc.ResolveEpoch = epoch;

            auto& assets = AssetManager::Get();
            UUID handle  = mc.MaterialHandle;
            Ref<Material> material = handle.IsNull() ? nullptr : assets.GetAssetAs<Material>(handle);
            if (!material && !mc.MaterialSource.empty()) {
                handle = assets.GetHandle(mc.MaterialSource);
                if (!handle.IsNull()) material = assets.GetAssetAs<Material>(handle);
            }
            mc.RuntimeMaterial = material;
            if (material) mc.MaterialHandle = handle;

            // (Re)build this entity's override set now that its material was resolved.
            // Sparse per-entity overrides layer over the base material's values.
            if (material && !mc.Overrides.empty())
                cache.BuildOverride(entityId, material, mc.Overrides, renderer);
            else
                cache.DropOverride(entityId);
        }
        return mc.RuntimeMaterial;
    }

    void RenderGraph::Rebuild(const Ref<Scene>& scene,
                               RendererAPI* renderer,
                               RayMaterialCache& cache,
                               const Ref<Pipeline>& defaultPipeline,
                               const Ref<Pipeline>& errorPipeline,
                               const Ref<RenderPass>& scenePass) {
        m_DrawCommands.clear();

        auto registry = scene->GetEntityRegistry().lock();
        if (!registry) return;

        // ---- World-transform resolution (hierarchy composition) ----------------
        // A TransformComponent is LOCAL to its parent, so a renderable's world matrix
        // is the product of its ancestors' local transforms. Ancestors may be empty
        // pivot entities (no mesh), so resolve by walking parent UUIDs. Results are
        // memoized; a provisional identity is written before recursing so a corrupt
        // cyclic hierarchy terminates instead of overflowing the stack.
        std::unordered_map<UUID, entt::entity> byUuid;
        for (auto&& [e, id] : registry->view<IDComponent>().each())
            byUuid[id.ID] = e;

        std::unordered_map<entt::entity, glm::mat4> worldCache;
        std::function<glm::mat4(entt::entity)> worldOf = [&](entt::entity e) -> glm::mat4 {
            if (auto it = worldCache.find(e); it != worldCache.end())
                return it->second;
            worldCache[e] = glm::mat4(1.0f);   // provisional — breaks accidental cycles

            const auto* tc = registry->try_get<TransformComponent>(e);
            glm::mat4 world = tc ? ComposeLocalTransform(*tc) : glm::mat4(1.0f);

            const auto* rel = registry->try_get<RelationshipComponent>(e);
            if (rel && rel->Parent.has_value()) {
                auto pit = byUuid.find(*rel->Parent);
                if (pit != byUuid.end() && pit->second != e)
                    world = worldOf(pit->second) * world;
            }

            worldCache[e] = world;
            return world;
        };

        // Iterate all entities with both a MeshComponent and TransformComponent
        auto view = registry->view<IDComponent, MeshComponent, TransformComponent>();
        for (auto&& [entity, id, mc, tc] : view.each()) {
            // Lazily resolve the asset handle to a GPU-ready mesh (the renderer is
            // active during rendering, so GPU upload succeeds here).
            if (!mc.RuntimeMesh) {
                auto& assets = AssetManager::Get();
                const uint64_t epoch = assets.GetEpoch();

                // Attempt resolution at most once per epoch so a failed lookup does
                // not re-log every frame, while a hot-reload / renderer swap (which
                // bumps the epoch) still triggers a fresh attempt.
                if (mc.ResolveEpoch != epoch) {
                    mc.ResolveEpoch = epoch;

                    UUID      handle = mc.MeshHandle;
                    Ref<Mesh> mesh   = handle.IsNull() ? nullptr : assets.GetMesh(handle);

                    // Self-heal: a persisted handle the registry no longer knows
                    // (its .meta was removed, or the asset was renamed) falls back
                    // to the readable source and adopts the corrected handle.
                    if (!mesh && !mc.MeshSource.empty()) {
                        handle = assets.GetHandle(mc.MeshSource);
                        if (!handle.IsNull())
                            mesh = assets.GetMesh(handle);
                    }

                    if (mesh) {
                        mc.MeshHandle  = handle;
                        mc.RuntimeMesh = mesh;
                    }
                }
            }

            if (!mc.IsValid()) continue; // Skip meshes with no GPU data

            DrawCommand cmd;
            cmd.EntityUUID   = id.ID;
            cmd.EntityID     = static_cast<uint32_t>(entt::to_integral(entity));
            cmd.VertexBuffer = mc.RuntimeMesh->GetVertexBuffer();
            cmd.IndexBuffer  = mc.RuntimeMesh->GetIndexBuffer();
            cmd.VertexCount  = mc.RuntimeMesh->GetVertexCount();
            cmd.IndexCount   = mc.RuntimeMesh->GetIndexCount();
            cmd.Transform    = worldOf(entity);   // parent chain composed → world matrix

            // Material resolution (GPU objects come from the renderer's material cache):
            //  - No MaterialComponent          → renderer's defaultPipeline (draw normally)
            //  - MaterialComponent resolves     → the material's own pipeline + descriptor set
            //  - MaterialComponent fails        → renderer's errorPipeline (pink / obvious signal)
            cmd.PipelineRef = defaultPipeline;
            if (registry->all_of<MaterialComponent>(entity)) {
                auto& mat = registry->get<MaterialComponent>(entity);
                if (!mat.MaterialHandle.IsNull() || !mat.MaterialSource.empty()) {
                    Ref<Material> resolved = ResolveMaterial(cmd.EntityID, mat,
                                                             AssetManager::Get().GetEpoch(),
                                                             cache, renderer);
                    if (resolved) {
                        const auto& gpu = cache.GetOrBuild(resolved, renderer, scenePass);
                        cmd.PipelineRef = gpu.PipelineRef ? gpu.PipelineRef : errorPipeline;
                        // Per-entity override set if any; otherwise the material's base set.
                        Ref<DescriptorSet> ov = mat.Overrides.empty()
                                              ? nullptr : cache.GetOverride(cmd.EntityID);
                        cmd.MaterialSet = ov ? ov : gpu.Base.Set;
                    } else {
                        cmd.PipelineRef = errorPipeline;
                    }
                }
            }

            // Build sort key: pipeline pointer (upper 32) | VB pointer (lower 32)
            // This groups by pipeline first, then by mesh identity.
            uintptr_t pipeKey = reinterpret_cast<uintptr_t>(cmd.PipelineRef.get());
            uintptr_t meshKey = reinterpret_cast<uintptr_t>(cmd.VertexBuffer.get());
            cmd.SortKey = (static_cast<uint64_t>(pipeKey) << 32)
                        | (static_cast<uint64_t>(meshKey) & 0xFFFFFFFFULL);

            m_DrawCommands.push_back(std::move(cmd));
        }
    }

    // ------------------------------------------------------------------
    // SortAndBatch — sort by pipeline → mesh, group into PipelineGroups
    // ------------------------------------------------------------------

    void RenderGraph::SortAndBatch() {
        m_PipelineGroups.clear();

        if (m_DrawCommands.empty()) return;

        // Sort by SortKey (pipeline first, then mesh identity)
        std::sort(m_DrawCommands.begin(), m_DrawCommands.end(),
                  [](const DrawCommand& a, const DrawCommand& b) {
                      return a.SortKey < b.SortKey;
                  });

        // Walk sorted commands and group into pipeline groups → draw batches
        Ref<Pipeline> currentPipeline = nullptr;
        PipelineGroup* currentGroup   = nullptr;

        void* currentVB = nullptr;
        void* currentIB = nullptr;
        DrawBatch* currentBatch = nullptr;

        for (const auto& cmd : m_DrawCommands) {
            // New pipeline group?
            if (cmd.PipelineRef != currentPipeline) {
                m_PipelineGroups.push_back({ cmd.PipelineRef, {} });
                currentGroup    = &m_PipelineGroups.back();
                currentPipeline = cmd.PipelineRef;
                currentVB       = nullptr;  // Force new batch
                currentIB       = nullptr;
                currentBatch    = nullptr;
            }

            // New mesh batch within the current pipeline group?
            void* vbPtr = static_cast<void*>(cmd.VertexBuffer.get());
            void* ibPtr = static_cast<void*>(cmd.IndexBuffer.get());

            if (vbPtr != currentVB || ibPtr != currentIB) {
                currentGroup->Batches.push_back({});
                currentBatch = &currentGroup->Batches.back();
                currentBatch->VertexBuffer = cmd.VertexBuffer;
                currentBatch->IndexBuffer  = cmd.IndexBuffer;
                currentBatch->VertexCount  = cmd.VertexCount;
                currentBatch->IndexCount   = cmd.IndexCount;
                currentVB = vbPtr;
                currentIB = ibPtr;
            }

            // Append instance data (parallel arrays: transform + its material set + entity id)
            currentBatch->Transforms.push_back(cmd.Transform);
            currentBatch->MaterialSets.push_back(cmd.MaterialSet);
            currentBatch->EntityIDs.push_back(cmd.EntityID);
        }
    }

} // namespace Echelon
