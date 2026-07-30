#pragma once

/**
 * @file Asset.hpp
 * @brief Base class for every engine asset + the asset type tag.
 *
 * Best Practices:
 *  - All asset behaviour that differs per type is expressed through the virtuals
 *    below (GetType / IsValid / UploadGPU / ReleaseGPU / ReloadFrom). The
 *    AssetManager never switches on a concrete type, so new asset types
 *    (Texture, Material, ...) are purely additive — see AssetManager.hpp.
 *  - GPU-side work goes through the *active renderer's* Device via UploadGPU, so
 *    assets carry no back-end-specific code and survive renderer hot-swaps.
 */

#include "Core/Base.hpp"
#include "Core/UUID.hpp"

#include <cstdint>
#include <string_view>

namespace Echelon {

    class RendererAPI; // fwd — assets upload through the active renderer's Device

    /**
     * @brief Discriminator stamped onto asset metadata.
     *
     * This is metadata / query sugar only — it is never used to dispatch loading
     * (that is keyed on file extension via AssetImporter). Extend by adding a
     * value when a new engine type is implemented.
     */
    enum class AssetType : uint16_t {
        None = 0,
        Mesh,
        Scene
    };

    inline const char* AssetTypeToString(AssetType type) {
        switch (type) {
            case AssetType::Mesh:  return "Mesh";
            case AssetType::Scene: return "Scene";
            default:               return "None";
        }
    }

    inline AssetType AssetTypeFromString(std::string_view s) {
        if (s == "Mesh")  return AssetType::Mesh;
        if (s == "Scene") return AssetType::Scene;
        return AssetType::None;
    }

    /**
     * @brief Polymorphic base for all assets.
     */
    class Asset {
    public:
        virtual ~Asset() = default;

        /** @brief Stable identity, assigned by the AssetManager. */
        UUID Handle;

        virtual AssetType GetType() const { return AssetType::None; }
        virtual bool IsValid() const { return false; }

        /** @brief (Re)create GPU resources against the active renderer. No-op for CPU-only assets. */
        virtual void UploadGPU(RendererAPI* /*renderer*/) {}

        /** @brief Drop GPU resources (kept CPU-side data allows a later rebuild). */
        virtual void ReleaseGPU() {}

        /**
         * @brief Absorb freshly re-imported data into this instance (hot-reload).
         *
         * Updating in place (rather than swapping the object) keeps every held
         * Ref<Asset> valid. Default is a no-op; GPU types override it.
         */
        virtual void ReloadFrom(const Ref<Asset>& /*fresh*/) {}
    };

} // namespace Echelon
