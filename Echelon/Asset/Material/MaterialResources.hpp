#pragma once

/**
 * @file MaterialResources.hpp
 * @brief Shared helpers to build & pack a material's reflection-driven GPU
 *        resources (parameter UBO + descriptor set). Used by both Material and
 *        MaterialInstance so the packing logic lives in one place.
 *
 * The "material block" is the first reflected uniform buffer that is NOT part of
 * the fixed shader constant system (g_Frame / g_Object). Its members are the
 * material's editable parameters; reflected samplers are material textures.
 */

#include "GraphicsAPI/Device.hpp"
#include "GraphicsAPI/ShaderReflection.hpp"
#include "Renderer/RendererAPI.hpp"
#include "Asset/Material/MaterialParam.hpp"

#include <functional>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

namespace Echelon {

    /** @brief True for the engine-provided system constant buffers (not material params). */
    inline bool IsSystemUBO(const std::string& name) {
        return name == "g_Frame" || name == "g_Object";
    }

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

    /**
     * @brief Build the material's param UBO + descriptor set from a shader's reflection.
     * Binds the param UBO at its reflected binding and a fallback texture at every
     * reflected sampler binding. Returns empty resources if there is nothing to bind.
     */
    inline MaterialGpuResources BuildMaterialResources(RendererAPI* renderer,
                                                       const ShaderReflection& refl,
                                                       const Ref<Texture>& fallbackTexture) {
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
            if (fallbackTexture) res.Set->SetTexture(s.Binding, fallbackTexture);
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
