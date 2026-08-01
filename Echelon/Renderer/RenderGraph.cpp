#include "RenderGraph.hpp"

#include "Scene/Scene.hpp"
#include "ECS/Components.hpp"
#include "GraphicsAPI/Pipeline.hpp"
#include "Asset/AssetManager.hpp"
#include "Asset/Mesh/Mesh.hpp"
#include "Asset/Material/Material.hpp"
#include "Asset/Material/MaterialInstance.hpp"
#include "Renderer/RendererService.hpp"
#include "Core/Log.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

#include <algorithm>
#include <functional>
#include <cstring>

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

        auto registry = scene->GetEntityRegistry().lock();
        if (!registry) return version;

        // Hash mesh versions + transform data + material versions
        auto view = registry->view<IDComponent, MeshComponent, TransformComponent>();
        for (auto&& [entity, id, mc, tc] : view.each()) {
            version = HashCombine(version, id.ID.Hash());
            version = HashCombine(version, mc.Version);

            // Include transform in version (bit-cast floats)
            uint32_t px, py, pz, rx, ry, rz, sx, sy, sz;
            std::memcpy(&px, &tc.Position.x, 4); std::memcpy(&py, &tc.Position.y, 4); std::memcpy(&pz, &tc.Position.z, 4);
            std::memcpy(&rx, &tc.Rotation.x, 4); std::memcpy(&ry, &tc.Rotation.y, 4); std::memcpy(&rz, &tc.Rotation.z, 4);
            std::memcpy(&sx, &tc.Scale.x,    4); std::memcpy(&sy, &tc.Scale.y,    4); std::memcpy(&sz, &tc.Scale.z,    4);
            version = HashCombine(version, px); version = HashCombine(version, py); version = HashCombine(version, pz);
            version = HashCombine(version, rx); version = HashCombine(version, ry); version = HashCombine(version, rz);
            version = HashCombine(version, sx); version = HashCombine(version, sy); version = HashCombine(version, sz);

            // Include material version if present
            if (registry->all_of<MaterialComponent>(entity)) {
                const auto& mat = registry->get<MaterialComponent>(entity);
                version = HashCombine(version, mat.Version);
                version = HashCombine(version, mat.GetPipelineSortKey());
            }
        }

        return version;
    }

    // ------------------------------------------------------------------
    // Update — main entry point, called once per frame
    // ------------------------------------------------------------------

    void RenderGraph::Update(const Ref<Scene>& scene, const Ref<Pipeline>& defaultPipeline) {
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

        Rebuild(scene, defaultPipeline);
        SortAndBatch();

        m_LastSceneVersion = currentVersion;
        m_IsDirty    = false;
        m_WasRebuilt = true;
    }

    // ------------------------------------------------------------------
    // Rebuild — flatten scene graph into draw commands
    // ------------------------------------------------------------------

    // Resolve a MaterialComponent's asset reference into a pipeline + descriptor set
    // (once per epoch). Mirrors the mesh resolution: lazy, self-healing via source.
    static void ResolveMaterial(MaterialComponent& mc, uint64_t epoch) {
        if (mc.ResolveEpoch == epoch)
            return;
        mc.ResolveEpoch = epoch;

        // Auto-fill: an empty material component adopts the built-in default
        // material so every renderable is backed by a real material object.
        if (mc.MaterialHandle.IsNull() && mc.MaterialSource.empty())
            mc.MaterialSource = "DefaultMaterial";

        auto& assets = AssetManager::Get();
        UUID handle  = mc.MaterialHandle;
        Ref<Material> material = handle.IsNull() ? nullptr : assets.GetAssetAs<Material>(handle);
        if (!material && !mc.MaterialSource.empty()) {
            handle = assets.GetHandle(mc.MaterialSource);
            if (!handle.IsNull()) material = assets.GetAssetAs<Material>(handle);
        }
        if (!material)
            return;

        mc.MaterialHandle  = handle;
        mc.RuntimeMaterial = material;
        mc.PipelineRef     = material->GetPipeline();

        // Sparse per-entity overrides form a runtime MaterialInstance.
        if (!mc.Overrides.empty()) {
            auto inst = CreateRef<MaterialInstance>(material);
            for (const auto& [name, value] : mc.Overrides) inst->SetOverride(name, value);
            inst->Build(Renderer::Get().GetActive());
            mc.RuntimeInstance = inst;
        } else {
            mc.RuntimeInstance = nullptr;
        }
    }

    static glm::mat4 ComposeTransform(const TransformComponent& tc) {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), tc.Position);
        glm::mat4 r = glm::toMat4(glm::quat(glm::radians(tc.Rotation)));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), tc.Scale);
        return t * r * s;
    }

    void RenderGraph::Rebuild(const Ref<Scene>& scene, const Ref<Pipeline>& defaultPipeline) {
        m_DrawCommands.clear();

        auto registry = scene->GetEntityRegistry().lock();
        if (!registry) return;

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
            cmd.VertexBuffer = mc.RuntimeMesh->GetVertexBuffer();
            cmd.IndexBuffer  = mc.RuntimeMesh->GetIndexBuffer();
            cmd.VertexCount  = mc.RuntimeMesh->GetVertexCount();
            cmd.IndexCount   = mc.RuntimeMesh->GetIndexCount();
            cmd.Transform    = ComposeTransform(tc);

            // Material: resolve the asset reference → pipeline + per-entity set.
            cmd.PipelineRef = defaultPipeline;
            if (registry->all_of<MaterialComponent>(entity)) {
                auto& mat = registry->get<MaterialComponent>(entity);
                ResolveMaterial(mat, AssetManager::Get().GetEpoch());
                if (mat.PipelineRef) cmd.PipelineRef = mat.PipelineRef;
                cmd.MaterialSet = mat.GetDescriptorSet();
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

            // Append instance data (parallel arrays: transform + its material set)
            currentBatch->Transforms.push_back(cmd.Transform);
            currentBatch->MaterialSets.push_back(cmd.MaterialSet);
        }
    }

} // namespace Echelon
