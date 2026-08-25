#pragma once

/**
 * @file RayMaterialCache.hpp
 * @brief Renderer-owned cache that turns data-only material assets into GPU objects.
 *
 * The engine core keeps MaterialTemplate/Material/MaterialParam as pure data; the
 * renderer is the only place that knows the shader ABI (which UBOs/samplers are
 * "system" vs material) and the scene pass a material pipeline must be compatible
 * with. This cache therefore lives with the renderer and is the sole owner of a
 * material's GPU state. It is dropped wholesale on an asset-epoch change (hot-reload
 * / renderer swap) — mirroring the renderer's other epoch-keyed GPU caches.
 *
 * Two levels are cached separately so draws can be grouped pipeline→instance:
 *   - **Pipelines** keyed by (MaterialTemplate*, transparent) — every instance of a
 *     template+blend shares ONE pipeline (the group key).
 *   - **Instance resources** (param UBO + descriptor set) keyed by Material* — one per
 *     instance, re-packed when the instance's Version changes (an editor edit).
 */

#include "Echelon/Core/Base.hpp"
#include "Material/RayMaterialResources.hpp"   // MaterialGpuResources, Build/PackMaterialResources

#include <cstdint>
#include <unordered_map>

namespace Echelon {

    class Material;
    class MaterialTemplate;
    class ShaderAsset;
    class RendererAPI;
    class Pipeline;
    class RenderPass;
    class Texture;
    class Sampler;

    class RayMaterialCache {
    public:
        /** @brief What a draw needs: the shared template+blend pipeline + this instance's set. */
        struct MaterialGpu {
            Ref<Pipeline>        PipelineRef; ///< shared per (template, transparent), built against the scene pass
            MaterialGpuResources Base;        ///< per-instance param UBO + descriptor set
        };

        /**
         * @brief Get the pipeline + instance resources for a material instance.
         *        Builds the pipeline once per (template, transparent); (re)builds the
         *        instance's resources on first use and whenever its Version changes.
         *        Safe to call every rebuild (O(1) hit when nothing changed).
         */
        const MaterialGpu& GetOrBuild(const Ref<Material>& mat, RendererAPI* renderer,
                                      const Ref<RenderPass>& scenePass);

        /** @brief Drop all cached GPU state (asset epoch change / hot-reload / renderer swap). */
        void Clear();

    private:
        void EnsureFallbacks(RendererAPI* renderer);
        Ref<Pipeline> GetOrBuildPipeline(const MaterialTemplate* tmpl, bool transparent,
                                         const Ref<ShaderAsset>& shader, RendererAPI* renderer,
                                         const Ref<RenderPass>& scenePass);

        /** @brief A material instance's cached GPU state + change-tracking. */
        struct InstanceEntry {
            MaterialGpu             Gpu;
            uint64_t                BuiltVersion     = UINT64_MAX;  ///< Material::Version at last resource pack
            bool                    BuiltTransparent = false;       ///< to detect a blend-flag flip
            const MaterialTemplate* BuiltTemplate    = nullptr;     ///< to detect a template swap
        };

        std::unordered_map<uint64_t, Ref<Pipeline>>        m_Pipelines;  ///< key = (template ptr & ~1) | transparent
        std::unordered_map<const Material*, InstanceEntry> m_Materials;

        Ref<Texture> m_WhiteTexture;    ///< shared 1×1 white fallback for unbound samplers
        Ref<Sampler> m_DefaultSampler;  ///< shared LINEAR/REPEAT sampler
    };

} // namespace Echelon
