#include "Material/RayMaterialCache.hpp"

#include "Echelon/Asset/Material/Material.hpp"
#include "Echelon/Asset/Material/MaterialParam.hpp"
#include "Echelon/Asset/Shader/ShaderAsset.hpp"
#include "Echelon/Asset/Texture/TextureAsset.hpp"
#include "Echelon/Asset/Mesh/StandardVertex.hpp"
#include "Echelon/Asset/AssetManager.hpp"
#include "Echelon/GraphicsAPI/Device.hpp"
#include "Echelon/GraphicsAPI/Pipeline.hpp"
#include "Echelon/GraphicsAPI/RenderPass.hpp"
#include "Echelon/GraphicsAPI/Texture.hpp"
#include "Echelon/GraphicsAPI/DescriptorSet.hpp"
#include "Echelon/Renderer/RendererAPI.hpp"
#include "Echelon/Core/Log.hpp"

namespace Echelon {

    // Resolve a shader reference (handle first, then path hint) to a ShaderAsset.
    // (Relocated from the old Material::UploadGPU — material→GPU translation is the
    // renderer's job now.)
    static Ref<ShaderAsset> ResolveShader(const UUID& handle, const std::string& source) {
        auto& assets = AssetManager::Get();
        Ref<ShaderAsset> shader = handle.IsNull() ? nullptr : assets.GetAssetAs<ShaderAsset>(handle);
        if (!shader && !source.empty()) {
            UUID h = assets.GetHandle(source);
            if (!h.IsNull()) shader = assets.GetAssetAs<ShaderAsset>(h);
        }
        return shader;
    }

    // Resolve a material's texture reference (sampler name → path) to a GPU texture.
    // Returns nullptr on any miss so the caller falls back to the white texture.
    static Ref<Texture> ResolveMaterialTexture(RendererAPI* renderer,
                                               const std::string& samplerName,
                                               const std::string& path) {
        if (path.empty()) return nullptr;
        auto& assets = AssetManager::Get();
        UUID handle = assets.GetHandle(path);
        if (handle.IsNull()) return nullptr;

        auto texAsset = assets.GetAssetAs<TextureAsset>(handle);
        if (!texAsset) {
            ECHELON_LOG_WARN("[RayMaterial] Texture '{}' for sampler '{}' is not a texture asset.",
                             path, samplerName);
            return nullptr;
        }
        texAsset->UploadGPU(renderer);  // idempotent; guarantees readiness
        return texAsset->GetGpuTexture();
    }

    void RayMaterialCache::EnsureFallbacks(RendererAPI* renderer) {
        auto device = renderer->GetDevice();
        if (!device) return;

        // 1×1 white fallback so shaders that sample a texture render correctly before
        // real texture assets are assigned.
        if (!m_WhiteTexture) {
            TextureDesc td;
            td.Width = 1; td.Height = 1;
            td.Format = TextureFormat::RGBA8_UNORM;
            td.Usage  = TextureUsage::Sampled;
            td.DebugName = "RayMaterial_WhiteFallback";
            m_WhiteTexture = device->CreateTexture(td);
            const uint8_t white[4] = { 255, 255, 255, 255 };
            m_WhiteTexture->SetData(white, sizeof(white));
        }
        if (!m_DefaultSampler) {
            SamplerDesc sd;   // defaults: Linear, Repeat
            m_DefaultSampler = device->CreateSampler(sd);
        }
    }

    const RayMaterialCache::MaterialGpu&
    RayMaterialCache::GetOrBuild(const Ref<Material>& mat, RendererAPI* renderer,
                                 const Ref<RenderPass>& scenePass) {
        if (auto it = m_Materials.find(mat.get()); it != m_Materials.end())
            return it->second;

        MaterialGpu gpu;

        auto device = renderer ? renderer->GetDevice() : nullptr;
        Ref<ShaderAsset> shader = ResolveShader(mat->ShaderHandle, mat->ShaderSource);
        if (device && shader) {
            shader->UploadGPU(renderer);   // no-op if already uploaded
            if (shader->GetGpuShader()) {
                const ShaderReflection& refl = shader->GetReflection();

                // Reflection-driven pipeline (vertex layout from reflection — no
                // hand-written attributes). Back-face culling (front = CCW): engine
                // primitives + OBJ meshes are wound CCW-outward.
                PipelineDesc pd;
                pd.ShaderProgram          = shader->GetGpuShader();
                pd.Layout                 = StandardVertex::FromReflection(refl);
                pd.Topology               = PrimitiveTopology::TriangleList;
                pd.Depth.DepthTestEnable  = true;
                pd.Depth.DepthWriteEnable = true;
                pd.Raster.Cull            = CullMode::Back;
                pd.Raster.Winding         = FrontFace::CounterClockwise;
                pd.Pass                   = scenePass;   // compatible with the scene ("forward") pass
                pd.DebugName              = "Material_Pipeline";
                gpu.PipelineRef = device->CreatePipeline(pd);

                // Bind real texture assets to reflected samplers by name (white fallback
                // for any sampler without a matching entry in the material's Textures).
                EnsureFallbacks(renderer);
                Material* m = mat.get();
                auto resolver = [m, renderer](const std::string& samplerName) -> Ref<Texture> {
                    auto it = m->Textures.find(samplerName);
                    if (it == m->Textures.end()) return nullptr;   // → white fallback
                    return ResolveMaterialTexture(renderer, samplerName, it->second);
                };

                gpu.Base = BuildMaterialResources(renderer, refl, m_WhiteTexture,
                                                  m_DefaultSampler, resolver);
                PackMaterialResources(gpu.Base,
                    [m](const std::string& name) { return m->Resolve(name); });
            } else {
                ECHELON_LOG_ERROR("[RayMaterial] shader '{}' failed to build a GPU program",
                                  mat->ShaderSource);
            }
        } else if (!shader) {
            ECHELON_LOG_ERROR("[RayMaterial] material could not resolve its shader '{}'",
                              mat->ShaderSource);
        }

        return m_Materials.emplace(mat.get(), std::move(gpu)).first->second;
    }

    void RayMaterialCache::BuildOverride(uint32_t entityId, const Ref<Material>& mat,
                                         const std::unordered_map<std::string, MaterialParam>& overrides,
                                         RendererAPI* renderer) {
        Ref<ShaderAsset> shader = ResolveShader(mat->ShaderHandle, mat->ShaderSource);
        if (!shader || !shader->GetGpuShader()) {
            m_Overrides.erase(entityId);
            return;
        }

        // Override set: its own param UBO only (no textures — those come from the base
        // set). Values resolve override → base material → shader default.
        MaterialGpuResources res =
            BuildMaterialResources(renderer, shader->GetReflection(), nullptr, nullptr);
        Material* m = mat.get();
        PackMaterialResources(res, [&overrides, m](const std::string& name) -> const MaterialParam* {
            auto it = overrides.find(name);
            if (it != overrides.end()) return &it->second;
            return m->Resolve(name);
        });
        m_Overrides[entityId] = std::move(res);
    }

    Ref<DescriptorSet> RayMaterialCache::GetOverride(uint32_t entityId) const {
        auto it = m_Overrides.find(entityId);
        return it != m_Overrides.end() ? it->second.Set : nullptr;
    }

    void RayMaterialCache::DropOverride(uint32_t entityId) {
        m_Overrides.erase(entityId);
    }

    void RayMaterialCache::Clear() {
        m_Materials.clear();
        m_Overrides.clear();
        m_WhiteTexture   = nullptr;
        m_DefaultSampler = nullptr;
    }

} // namespace Echelon
