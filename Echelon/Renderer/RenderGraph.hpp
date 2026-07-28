#pragma once

/**
 * @file RenderGraph.hpp
 * @brief Lightweight render graph that converts a SceneGraph into a flat,
 *        sorted draw list suitable for efficient command-buffer recording.
 *
 * Design goals:
 *  - **O(n) rebuild** when the scene changes (n = number of renderable entities).
 *  - **O(1) per-frame skip** when nothing changed (version check).
 *  - Sorted by pipeline first, then by mesh identity for instancing.
 *  - Minimises pipeline switches (most expensive GPU state change).
 *  - Merges identical meshes within a pipeline group into instanced batches.
 *  - The graph owns no GPU resources — it only references Ref<Buffer> and
 *    Ref<Pipeline> already held by components and the renderer.
 *
 * Best Practices:
 *  - Call Update() once per frame.  It early-outs if nothing is dirty.
 *  - Renderers iterate GetPipelineGroups() to issue draw calls with
 *    minimal state changes.
 *  - Adding / removing MeshComponents, MaterialComponents, or changing
 *    transforms automatically bumps the graph's dirty flag via version
 *    tracking.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/ECS/Components.hpp"
#include "Echelon/Scene/SceneGraph.hpp"

#include "glm/glm.hpp"

#include <cstdint>
#include <vector>
#include <unordered_map>

namespace Echelon {

    // Forward declarations
    class Scene;
    class Pipeline;

    // ================================================================
    // A single draw command (one entity, fully resolved)
    // ================================================================

    struct DrawCommand {
        uint64_t     EntityUUID   = 0;
        Ref<Buffer>  VertexBuffer;
        Ref<Buffer>  IndexBuffer;       ///< nullptr for non-indexed draws
        uint32_t     VertexCount  = 0;
        uint32_t     IndexCount   = 0;
        glm::mat4    Transform    = glm::mat4(1.0f);

        // Material data (per-entity uniforms)
        Ref<Pipeline> PipelineRef;
        glm::vec4     AlbedoColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
        float         Roughness    = 0.5f;
        float         Metallic     = 0.0f;

        // Sort key: pipeline pointer in upper 32 bits, mesh pointer (VB)
        // in lower 32 bits.  This sorts by pipeline first, then by mesh
        // identity, enabling minimal pipeline switches AND instancing.
        uint64_t      SortKey      = 0;
    };

    // ================================================================
    // An instanced batch — same pipeline + same mesh
    // ================================================================

    struct DrawBatch {
        Ref<Buffer>  VertexBuffer;
        Ref<Buffer>  IndexBuffer;
        uint32_t     VertexCount  = 0;
        uint32_t     IndexCount   = 0;

        // Per-instance data
        std::vector<glm::mat4> Transforms;
        std::vector<glm::vec4> AlbedoColors;
    };

    // ================================================================
    // Pipeline group — all batches sharing the same pipeline
    // ================================================================

    /**
     * @brief A group of draw batches that use the same Pipeline.
     *
     * The renderer binds the pipeline once, then iterates all batches.
     * This minimises the most expensive GPU state change (pipeline switch).
     */
    struct PipelineGroup {
        Ref<Pipeline>          PipelineRef;
        std::vector<DrawBatch> Batches;
    };

    // ================================================================
    // RenderGraph
    // ================================================================

    class RenderGraph {
    public:
        RenderGraph() = default;
        ~RenderGraph() = default;

        // ---- Per-frame update ----

        /**
         * @brief Rebuild the draw list from the scene if anything changed.
         *
         * Compares mesh + material + transform versions to decide whether
         * a rebuild is needed.  If nothing changed this is a single
         * integer comparison — O(1).
         *
         * @param scene           The active scene.
         * @param defaultPipeline Fallback pipeline for entities without a MaterialComponent.
         */
        void Update(const Ref<Scene>& scene, const Ref<Pipeline>& defaultPipeline);

        /**
         * @brief Force a full rebuild on the next Update().
         */
        void Invalidate() { m_IsDirty = true; }

        // ---- Accessors ----

        /** Flat draw list (sorted by pipeline → mesh). */
        const std::vector<DrawCommand>& GetDrawCommands() const { return m_DrawCommands; }

        /** Draw list grouped by pipeline, then instanced by mesh. */
        const std::vector<PipelineGroup>& GetPipelineGroups() const { return m_PipelineGroups; }

        /** True if the last Update() actually rebuilt the graph. */
        bool WasRebuilt() const { return m_WasRebuilt; }

        /** Number of renderable entities in the current graph. */
        uint32_t GetRenderableCount() const { return static_cast<uint32_t>(m_DrawCommands.size()); }

    private:
        void Rebuild(const Ref<Scene>& scene, const Ref<Pipeline>& defaultPipeline);
        void SortAndBatch();
        uint64_t ComputeSceneVersion(const Ref<Scene>& scene) const;

        bool     m_IsDirty    = true;
        bool     m_WasRebuilt = false;
        uint64_t m_LastSceneVersion = 0;

        std::vector<DrawCommand>   m_DrawCommands;
        std::vector<PipelineGroup> m_PipelineGroups;
    };

} // namespace Echelon
