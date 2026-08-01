#pragma once

/**
 * @file ShaderAsset.hpp
 * @brief Shader asset — owns per-stage SPIR-V + reflection, builds the GPU shader.
 *
 * Authored in Slang, compiled to SPIR-V (the IR) at import time by the
 * ShaderImporter, and fed directly to the backend (OpenGL via GL_ARB_gl_spirv).
 * The compiled bytecode + reflection are retained CPU-side so the GPU shader can
 * be rebuilt on renderer hot-swap and asset hot-reload — mirroring Mesh.
 *
 * Named ShaderAsset (not Shader) to avoid clashing with the GraphicsAPI's
 * abstract `class Shader` (the GPU object this asset produces).
 */

#include "GraphicsAPI/Shader.hpp"
#include "GraphicsAPI/Device.hpp"

#include "Renderer/RendererAPI.hpp"

#include "Asset/Asset.hpp"

namespace Echelon {

    class ShaderAsset : public Asset {
    public:
        ShaderAsset() = default;
        ~ShaderAsset() override = default;

        /** @brief Set the compiled program description (SPIR-V stages + reflection). Does not touch the GPU. */
        void SetDesc(ShaderDesc desc) { m_Desc = std::move(desc); }

        AssetType GetType() const override { return AssetType::Shader; }

        /** @brief (Re)create the GPU shader program from the retained SPIR-V. */
        void UploadGPU(RendererAPI* renderer) override {
            if (!renderer) return;
            auto device = renderer->GetDevice();
            if (!device) return;
            if (!m_GpuShader && !m_Desc.Stages.empty())
                m_GpuShader = device->CreateShader(m_Desc);
        }

        /** @brief Drop the GPU shader; SPIR-V + reflection are retained for a later rebuild. */
        void ReleaseGPU() override { m_GpuShader = nullptr; }

        /** @brief Absorb freshly recompiled SPIR-V/reflection in place so held Refs stay valid. */
        void ReloadFrom(const Ref<Asset>& fresh) override {
            auto other = std::dynamic_pointer_cast<ShaderAsset>(fresh);
            if (!other) return;
            ReleaseGPU();
            m_Desc = other->m_Desc;
        }

        [[nodiscard]] bool IsValid() const override { return m_GpuShader != nullptr; }

        [[nodiscard]] const Ref<Shader>&      GetGpuShader() const { return m_GpuShader; }
        [[nodiscard]] const ShaderReflection& GetReflection() const { return m_Desc.Reflection; }
        [[nodiscard]] const ShaderDesc&       GetDesc() const { return m_Desc; }

    private:
        ShaderDesc  m_Desc;
        Ref<Shader> m_GpuShader = nullptr;
    };

} // namespace Echelon
