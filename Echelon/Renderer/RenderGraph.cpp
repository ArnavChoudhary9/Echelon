#include "RenderGraph.hpp"

#include "Scene/Scene.hpp"
#include "ECS/Components.hpp"
#include "GraphicsAPI/Pipeline.hpp"
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

        auto registry = scene->GetEntityRegistry().lock();
        if (!registry) return version;

        // Hash mesh versions + transform data + material versions
        auto view = registry->view<IDComponent, MeshComponent, TransformComponent>();
        for (auto&& [entity, id, mc, tc] : view.each()) {
            version = HashCombine(version, static_cast<uint64_t>(id.ID));
            version = HashCombine(version, mc.Version);

            // Include transform in version (bit-cast floats)
            uint32_t px, py, pz;
            std::memcpy(&px, &tc.Position.x, 4);
            std::memcpy(&py, &tc.Position.y, 4);
            std::memcpy(&pz, &tc.Position.z, 4);
            version = HashCombine(version, px);
            version = HashCombine(version, py);
            version = HashCombine(version, pz);

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
            if (!mc.IsValid()) continue; // Skip meshes with no GPU data

            DrawCommand cmd;
            cmd.EntityUUID   = static_cast<uint64_t>(id.ID);
            cmd.VertexBuffer = mc.VertexBuffer;
            cmd.IndexBuffer  = mc.IndexBuffer;
            cmd.VertexCount  = mc.VertexCount;
            cmd.IndexCount   = mc.IndexCount;
            cmd.Transform    = ComposeTransform(tc);

            // Material data
            if (registry->all_of<MaterialComponent>(entity)) {
                const auto& mat = registry->get<MaterialComponent>(entity);
                cmd.PipelineRef  = mat.PipelineRef ? mat.PipelineRef : defaultPipeline;
                cmd.AlbedoColor  = mat.AlbedoColor;
                cmd.Roughness    = mat.Roughness;
                cmd.Metallic     = mat.Metallic;
            } else {
                cmd.PipelineRef = defaultPipeline;
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

            // Append instance data
            currentBatch->Transforms.push_back(cmd.Transform);
            currentBatch->AlbedoColors.push_back(cmd.AlbedoColor);
        }
    }

} // namespace Echelon
