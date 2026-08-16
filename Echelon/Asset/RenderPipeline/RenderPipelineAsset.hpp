#pragma once

/**
 * @file RenderPipelineAsset.hpp
 * @brief A render pipeline (multipass "pass graph") authored as a project asset.
 *
 * The asset is pure CPU-side data (a RenderPipelineDesc) — it owns no GPU objects.
 * A renderer compiles the description into a runtime RenderPassGraph
 * (Echelon/Renderer/RenderPassGraph.hpp). Because it flows through the normal
 * AssetManager pipeline, projects can ship custom `.ehpipeline` files and edits
 * hot-reload for free (ReloadFrom updates the description in place + bumps the
 * asset epoch, which the renderer observes to recompile).
 */

#include "Asset/Asset.hpp"
#include "Asset/RenderPipeline/RenderPipelineDesc.hpp"

#include <memory>
#include <utility>

namespace Echelon {

    class RenderPipelineAsset : public Asset {
    public:
        RenderPipelineAsset() = default;
        explicit RenderPipelineAsset(RenderPipelineDesc desc) : m_Desc(std::move(desc)) {}

        AssetType GetType() const override { return AssetType::RenderPipeline; }
        bool      IsValid() const override { return !m_Desc.Passes.empty(); }

        /** @brief Absorb a freshly re-imported description in place (hot-reload). */
        void ReloadFrom(const Ref<Asset>& fresh) override {
            if (auto p = std::dynamic_pointer_cast<RenderPipelineAsset>(fresh))
                m_Desc = p->m_Desc;
        }

        const RenderPipelineDesc& GetDescription() const { return m_Desc; }

    private:
        RenderPipelineDesc m_Desc;
    };

} // namespace Echelon
