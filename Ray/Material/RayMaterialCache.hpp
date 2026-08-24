#pragma once

/**
 * @file RayMaterialCache.hpp
 * @brief Renderer-owned cache that turns a data-only Material asset into GPU
 *        objects (a pipeline + descriptor set), plus per-entity override sets.
 *
 * The engine core keeps Material/MaterialParam as pure data; the renderer is the
 * only place that knows the shader ABI (which UBOs/samplers are "system" vs
 * material) and the scene pass a material pipeline must be compatible with. This
 * cache therefore lives with the renderer and is the sole owner of a material's
 * GPU state. It is dropped wholesale on an asset-epoch change (hot-reload /
 * renderer swap) — mirroring the renderer's other epoch-keyed GPU caches.
 */

#include "Echelon/Core/Base.hpp"
#include "Material/RayMaterialResources.hpp"   // MaterialGpuResources, Build/PackMaterialResources

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Echelon {

    class Material;
    class RendererAPI;
    class Pipeline;
    class RenderPass;
    class Texture;
    class Sampler;
    class DescriptorSet;
    struct MaterialParam;

    /**
     * @brief GPU realization of one Material (built once, cached by identity).
     */
    class RayMaterialCache {
    public:
        struct MaterialGpu {
            Ref<Pipeline>        PipelineRef; ///< reflection-driven pipeline, built against the scene pass
            MaterialGpuResources Base;        ///< param UBO + descriptor set (textures + packed params)
        };

        /**
         * @brief Get (building once) the base pipeline + resources for a material.
         *        Keyed by material identity; safe to call every rebuild (O(1) hit).
         */
        const MaterialGpu& GetOrBuild(const Ref<Material>& mat, RendererAPI* renderer,
                                      const Ref<RenderPass>& scenePass);

        /**
         * @brief (Re)build a per-entity override descriptor set from sparse overrides.
         *        Rebuilds unconditionally (called only when the entity's material was
         *        re-resolved — i.e. an override edit or an epoch bump).
         */
        void BuildOverride(uint32_t entityId, const Ref<Material>& mat,
                           const std::unordered_map<std::string, MaterialParam>& overrides,
                           RendererAPI* renderer);

        /** @brief The cached override set for an entity (null if none). */
        Ref<DescriptorSet> GetOverride(uint32_t entityId) const;

        /** @brief Forget an entity's override set (it no longer has overrides). */
        void DropOverride(uint32_t entityId);

        /** @brief Drop all cached GPU state (asset epoch change / hot-reload / renderer swap). */
        void Clear();

    private:
        void EnsureFallbacks(RendererAPI* renderer);

        std::unordered_map<const Material*, MaterialGpu>   m_Materials;
        std::unordered_map<uint32_t, MaterialGpuResources> m_Overrides;

        Ref<Texture> m_WhiteTexture;    ///< shared 1×1 white fallback for unbound samplers
        Ref<Sampler> m_DefaultSampler;  ///< shared LINEAR/REPEAT sampler
    };

} // namespace Echelon
