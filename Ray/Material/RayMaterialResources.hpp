#pragma once

/**
 * @file RayMaterialResources.hpp
 * @brief Renderer-owned helpers to build & pack a material's reflection-driven
 *        GPU resources (parameter UBO + descriptor set). Used by the Ray material
 *        cache to realise both a material's base set and its per-entity override set.
 *
 * The "material block" is the first reflected uniform buffer that is NOT part of
 * Ray's shader ABI (g_Frame / g_Object / …). Its members are the material's
 * editable parameters; reflected samplers are material textures. Which resources
 * are "system" is decided by Ray's ABI (RayConstants.hpp) — so the split lives
 * with the renderer, not the engine core.
 */

#include "Echelon/GraphicsAPI/Device.hpp"
#include "Echelon/GraphicsAPI/ShaderReflection.hpp"
#include "Echelon/Renderer/RendererAPI.hpp"
#include "ABI/RayConstants.hpp"   // IsSystemUniformName / IsSystemSamplerName (Ray ABI policy)
#include "Echelon/Asset/Material/MaterialParam.hpp"

#include <functional>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

namespace Echelon {

    /** @brief True for the engine-provided system constant buffers (not material params). */
    inline bool IsSystemUBO(const std::string& name) { return IsSystemUniformName(name); }

    /**
     * @brief True for the engine-provided system samplers (shadow maps + IBL), which the
     *        renderer binds by name on the system descriptor set. They must NOT be treated
     *        as material textures, or the material set would clobber the system bindings.
     */
    inline bool IsSystemSampler(const std::string& name) { return IsSystemSamplerName(name); }

    /** @brief GPU resources realizing a material's parameters for one shader. */
    struct MaterialGpuResources {
        Ref<Buffer>        ParamUBO;
        Ref<DescriptorSet> Set;
        const ReflectedUniformBuffer* Block = nullptr;   ///< the reflected material UBO (or null)
        bool IsValid() const { return Set != nullptr; }
    };

    /** @brief Locate the material parameter block (first non-system UBO) in a reflection. */
    inline const ReflectedUniformBuffer* FindMaterialBlock(const ShaderReflection& refl) {
        for (const auto& ub : refl.UniformBuffers)
            if (!IsSystemUBO(ub.Name)) return &ub;
        return nullptr;
    }

    /** @brief Resolve a reflected sampler name → a texture to bind (or nullptr to fall back). */
    using TextureResolver = std::function<Ref<Texture>(const std::string& samplerName)>;

    /**
     * @brief Build the material's param UBO + descriptor set from a shader's reflection.
     *
     * Binds the param UBO at its reflected binding and a (texture, sampler) pair at
     * every reflected sampler binding.  Pass nullptr/nullptr for both fallback arguments
     * if the caller has no textures to bind (e.g. the per-entity override-only path).
     *
     * @param fallbackTexture  1×1 white texture used when a sampler resolves to nothing.
     * @param fallbackSampler  Default sampler (LINEAR/REPEAT).  Required on Vulkan;
     *                         drives glTexParameter* state on the OpenGL backend.
     * @param textureResolver  Optional: reflected sampler name → real texture. Called
     *                         per reflected sampler; on null/empty result the binding
     *                         falls back to @p fallbackTexture. Pass {} to bind only
     *                         fallbacks (e.g. the per-entity override-only path).
     */
    inline MaterialGpuResources BuildMaterialResources(RendererAPI* renderer,
                                                       const ShaderReflection& refl,
                                                       const Ref<Texture>& fallbackTexture,
                                                       const Ref<Sampler>& fallbackSampler,
                                                       const TextureResolver& textureResolver = {}) {
        MaterialGpuResources res;
        if (!renderer) return res;
        auto device = renderer->GetDevice();
        if (!device) return res;

        res.Block = FindMaterialBlock(refl);

        // Nothing material-specific to bind (e.g. the Flat shader): no resources.
        if (!res.Block && refl.Samplers.empty())
            return res;

        // Descriptor layout: the material UBO (if any) + every sampler binding.
        DescriptorSetLayoutDesc layoutDesc;
        if (res.Block) {
            layoutDesc.Bindings.push_back(
                { res.Block->Binding, DescriptorType::UniformBuffer, 1, ShaderStage::Fragment });
        }
        for (const auto& s : refl.Samplers) {
            if (IsSystemSampler(s.Name)) continue;   // bound by the renderer's system set, not the material
            layoutDesc.Bindings.push_back(
                { s.Binding, DescriptorType::CombinedImageSampler, 1, ShaderStage::Fragment });
        }
        layoutDesc.DebugName = "Material_Set";
        auto layout = device->CreateDescriptorSetLayout(layoutDesc);
        res.Set = device->AllocateDescriptorSet(layout);

        if (res.Block) {
            BufferDesc bd;
            bd.Size      = res.Block->Size > 0 ? res.Block->Size : 16;
            bd.Usage     = BufferUsage::UniformBuffer;
            bd.Memory    = MemoryUsage::CPUToGPU;
            bd.DebugName = "Material_ParamUBO";
            res.ParamUBO = device->CreateBuffer(bd);
            res.Set->SetBuffer(res.Block->Binding, res.ParamUBO);
        }
        for (const auto& s : refl.Samplers) {
            if (IsSystemSampler(s.Name)) continue;   // bound by the renderer's system set, not the material
            Ref<Texture> tex = textureResolver ? textureResolver(s.Name) : nullptr;
            if (!tex) tex = fallbackTexture;
            if (tex && fallbackSampler)
                res.Set->SetTexture(s.Binding, tex, fallbackSampler);
        }
        res.Set->Update();
        return res;
    }

    /**
     * @brief Pack resolved parameter values into a material's param UBO.
     * @param resolve  name → value (own override → parent → nullptr for "unset").
     * Unset members are left zero-filled.
     */
    inline void PackMaterialResources(const MaterialGpuResources& res,
                                      const std::function<const MaterialParam*(const std::string&)>& resolve) {
        if (!res.Block || !res.ParamUBO) return;

        std::vector<uint8_t> cpu(res.Block->Size, 0);
        for (const auto& member : res.Block->Members) {
            const MaterialParam* p = resolve(member.Name);
            if (!p) continue;
            const uint32_t n = std::min<uint32_t>(member.Size, p->ByteSize());
            if (member.Offset + n <= cpu.size())
                std::memcpy(cpu.data() + member.Offset, p->Data, n);
        }
        res.ParamUBO->SetData(cpu.data(), cpu.size());
    }

} // namespace Echelon
