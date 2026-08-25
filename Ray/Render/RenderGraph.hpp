#pragma once

/**
 * @file RenderGraph.hpp
 * @brief Lightweight render graph that converts a SceneGraph into a sorted,
 *        two-level-grouped draw list suitable for efficient command recording.
 *
 * Design goals:
 *  - **O(n) rebuild** when the scene changes (n = number of renderable entities).
 *  - **O(1) per-frame skip** when nothing changed (version check).
 *  - Opaque draws grouped **pipeline → instance (descriptor set) → mesh**, so the
 *    renderer binds the pipeline + system set once per pipeline group, the material
 *    set once per instance group, and only updates the per-object transform per draw.
 *  - Transparent draws collected separately and sorted **back-to-front by camera
 *    distance every frame** (correct alpha blending), independent of the rebuild.
 *  - The graph owns no GPU resources — it only references Ref<Buffer>/Ref<Pipeline>/
 *    Ref<DescriptorSet> already held by the renderer's material cache.
 *
 * Best Practices:
 *  - Call Update() once per frame.  It early-outs if nothing is dirty.
 *  - Call SortTransparent(cameraPos) each frame before recording (cheap; keeps the
 *    transparent order correct as the camera moves without forcing a rebuild).
 *  - Renderers iterate GetPipelineGroups() (opaque) then GetTransparentDraws().
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
    class RenderPass;
    class DescriptorSet;
    class RendererAPI;
    class RayMaterialCache;   // renderer-owned material→GPU cache (Ray/Material/)

    // ================================================================
    // A single draw command (one entity, fully resolved)
    // ================================================================

    struct DrawCommand {
        UUID         EntityUUID;
        uint32_t     EntityID     = 0;   ///< entt entity id (integral) — used by the editor picking pass
        Ref<Buffer>  VertexBuffer;
        Ref<Buffer>  IndexBuffer;       ///< nullptr for non-indexed draws
        uint32_t     VertexCount  = 0;
        uint32_t     IndexCount   = 0;
        glm::mat4    Transform    = glm::mat4(1.0f);

        // Material: the resolved pipeline + this instance's parameter/texture set.
        // MaterialSet may be null (default pipeline / no material params).
        Ref<Pipeline>      PipelineRef;
        Ref<DescriptorSet> MaterialSet;
        bool               Transparent = false;   ///< routed to the back-to-front transparent bucket
    };

    // ================================================================
    // An instanced batch — same pipeline + same instance set + same mesh
    // ================================================================

    struct DrawBatch {
        Ref<Buffer>  VertexBuffer;
        Ref<Buffer>  IndexBuffer;
        uint32_t     VertexCount  = 0;
        uint32_t     IndexCount   = 0;

        // Per-instance data (parallel arrays: one entry per draw in this batch).
        std::vector<glm::mat4> Transforms;
        std::vector<uint32_t>  EntityIDs;    ///< entt entity id per instance (editor picking)
    };

    // ================================================================
    // Instance group — all batches sharing one material descriptor set
    // ================================================================

    /**
     * @brief A group of draw batches that use the same material set (one instance).
     *        The renderer binds the set once, then iterates all batches.
     */
    struct InstanceGroup {
        Ref<DescriptorSet>     MaterialSet;   ///< bound once at set 1 (may be null for the default pipeline)
        std::vector<DrawBatch> Batches;
    };

    // ================================================================
    // Pipeline group — all instance groups sharing the same pipeline
    // ================================================================

    /**
     * @brief A group of instance groups that use the same Pipeline.
     *
     * The renderer binds the pipeline + system set once, then iterates the instance
     * groups (binding each material set once). This minimises the two most expensive
     * state changes — pipeline switches and descriptor-set binds.
     */
    struct PipelineGroup {
        Ref<Pipeline>              PipelineRef;
        std::vector<InstanceGroup> Instances;
    };

    // ================================================================
    // A transparent draw (sorted back-to-front every frame)
    // ================================================================

    struct TransparentDraw {
        Ref<Pipeline>      PipelineRef;
        Ref<DescriptorSet> MaterialSet;
        Ref<Buffer>        VertexBuffer;
        Ref<Buffer>        IndexBuffer;
        uint32_t           VertexCount = 0;
        uint32_t           IndexCount  = 0;
        glm::mat4          Transform   = glm::mat4(1.0f);
        uint32_t           EntityID    = 0;
        glm::vec3          Centroid    = glm::vec3(0.0f);   ///< world position proxy for the depth sort
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
         * Compares mesh + material (reference + instance) + transform versions to decide
         * whether a rebuild is needed. If nothing changed this is a single integer
         * comparison — O(1).
         *
         * @param scene           The active scene.
         * @param renderer        The owning renderer (material cache builds GPU objects against it).
         * @param cache           Renderer-owned material→GPU cache (pipelines + descriptor sets).
         * @param defaultPipeline Pipeline used for entities that have no MaterialComponent.
         * @param errorPipeline   Pipeline used when a material is configured but fails to resolve.
         * @param scenePass       The scene ("forward") pass material pipelines must be compatible with.
         */
        void Update(const Ref<Scene>& scene,
                    RendererAPI* renderer,
                    RayMaterialCache& cache,
                    const Ref<Pipeline>& defaultPipeline,
                    const Ref<Pipeline>& errorPipeline,
                    const Ref<RenderPass>& scenePass);

        /** @brief Sort the transparent bucket back-to-front for the given camera position. Cheap; call per frame. */
        void SortTransparent(const glm::vec3& cameraPos);

        /** @brief Force a full rebuild on the next Update(). */
        void Invalidate() { m_IsDirty = true; }

        // ---- Accessors ----

        /** Opaque draws grouped by pipeline → instance set → mesh. */
        const std::vector<PipelineGroup>& GetPipelineGroups() const { return m_PipelineGroups; }

        /** Transparent draws (sorted back-to-front by the last SortTransparent call). */
        const std::vector<TransparentDraw>& GetTransparentDraws() const { return m_TransparentDraws; }

        /** True if the last Update() actually rebuilt the graph. */
        bool WasRebuilt() const { return m_WasRebuilt; }

        /** Number of renderable entities in the current graph. */
        uint32_t GetRenderableCount() const { return m_RenderableCount; }

    private:
        void Rebuild(const Ref<Scene>& scene,
                     RendererAPI* renderer,
                     RayMaterialCache& cache,
                     const Ref<Pipeline>& defaultPipeline,
                     const Ref<Pipeline>& errorPipeline,
                     const Ref<RenderPass>& scenePass);
        void SortAndBatch(std::vector<DrawCommand>& opaque);
        uint64_t ComputeSceneVersion(const Ref<Scene>& scene) const;

        bool     m_IsDirty    = true;
        bool     m_WasRebuilt = false;
        uint64_t m_LastSceneVersion = 0;
        uint32_t m_RenderableCount  = 0;

        std::vector<PipelineGroup>   m_PipelineGroups;    ///< opaque, grouped
        std::vector<TransparentDraw> m_TransparentDraws;  ///< transparent, depth-sorted per frame
    };

} // namespace Echelon
