#include "Material/RayMaterialCache.hpp"

#include "Echelon/Asset/Material/Material.hpp"
#include "Echelon/Asset/Material/MaterialTemplate.hpp"
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

    Ref<Pipeline> RayMaterialCache::GetOrBuildPipeline(const MaterialTemplate* tmpl, bool transparent,
                                                       const Ref<ShaderAsset>& shader, RendererAPI* renderer,
                                                       const Ref<RenderPass>& scenePass) {
        // Key: template identity + the blend variant (template ptr is ≥2-aligned, so bit 0 is free).
        const uint64_t key = (reinterpret_cast<uint64_t>(tmpl) & ~1ull) | (transparent ? 1ull : 0ull);
        if (auto it = m_Pipelines.find(key); it != m_Pipelines.end())
            return it->second;

        auto device = renderer->GetDevice();
        const ShaderReflection& refl = shader->GetReflection();

        // Reflection-driven pipeline; render state from the template. Transparent instances
        // get an alpha-blend variant that does NOT write depth (drawn back-to-front after opaque).
        PipelineDesc pd;
        pd.ShaderProgram          = shader->GetGpuShader();
        pd.Layout                 = StandardVertex::FromReflection(refl);
        pd.Topology               = tmpl->Topology;
        pd.Depth.DepthTestEnable  = tmpl->DepthTest;
        pd.Depth.DepthWriteEnable = transparent ? false : tmpl->DepthWrite;
        pd.Raster.Cull            = tmpl->Cull;
        pd.Raster.Winding         = tmpl->Winding;
        if (transparent) {
            BlendAttachment ba;
            ba.BlendEnable   = true;
            ba.SrcColorBlend = BlendFactor::SrcAlpha;
            ba.DstColorBlend = BlendFactor::OneMinusSrcAlpha;
            ba.ColorBlendOp  = BlendOp::Add;
            ba.SrcAlphaBlend = BlendFactor::One;
            ba.DstAlphaBlend = BlendFactor::OneMinusSrcAlpha;
            ba.AlphaBlendOp  = BlendOp::Add;
            pd.Blend.Attachments.push_back(ba);
        }
        pd.Pass      = scenePass;   // compatible with the scene ("forward") pass
        pd.DebugName = transparent ? "Material_Pipeline(blend)" : "Material_Pipeline";

        auto pipe = device->CreatePipeline(pd);
        m_Pipelines[key] = pipe;
        return pipe;
    }

    const RayMaterialCache::MaterialGpu&
    RayMaterialCache::GetOrBuild(const Ref<Material>& mat, RendererAPI* renderer,
                                 const Ref<RenderPass>& scenePass) {
        InstanceEntry& e = m_Materials[mat.get()];   // default-constructs on first use

        Ref<MaterialTemplate> tmpl   = mat->GetTemplate();
        Ref<ShaderAsset>      shader = tmpl ? ResolveShader(tmpl->ShaderHandle, tmpl->ShaderSource) : nullptr;

        auto device = renderer ? renderer->GetDevice() : nullptr;
        if (!tmpl) {
            ECHELON_LOG_ERROR("[RayMaterial] material could not resolve its template '{}'", mat->TemplateSource);
            return e.Gpu;
        }
        if (!shader) {
            ECHELON_LOG_ERROR("[RayMaterial] template could not resolve its shader '{}'", tmpl->ShaderSource);
            return e.Gpu;
        }
        if (!device) return e.Gpu;

        shader->UploadGPU(renderer);   // no-op if already uploaded
        if (!shader->GetGpuShader()) {
            ECHELON_LOG_ERROR("[RayMaterial] shader '{}' failed to build a GPU program", tmpl->ShaderSource);
            return e.Gpu;
        }

        const bool templateChanged = (e.BuiltTemplate != tmpl.get());
        const bool needResources   = templateChanged || (e.BuiltVersion != mat->Version) || !e.Gpu.Base.Set;
        const bool needPipeline    = templateChanged || (e.BuiltTransparent != mat->Transparent) || !e.Gpu.PipelineRef;

        if (needPipeline)
            e.Gpu.PipelineRef = GetOrBuildPipeline(tmpl.get(), mat->Transparent, shader, renderer, scenePass);

        if (needResources) {
            // Bind real texture assets to reflected samplers by name (white fallback for any
            // sampler without a matching entry in the instance's Textures).
            EnsureFallbacks(renderer);
            Material* m = mat.get();
            auto resolver = [m, renderer](const std::string& samplerName) -> Ref<Texture> {
                auto it = m->Textures.find(samplerName);
                if (it == m->Textures.end()) return nullptr;   // → white fallback
                return ResolveMaterialTexture(renderer, samplerName, it->second);
            };
            e.Gpu.Base = BuildMaterialResources(renderer, shader->GetReflection(),
                                                m_WhiteTexture, m_DefaultSampler, resolver);
            PackMaterialResources(e.Gpu.Base,
                [m](const std::string& name) { return m->Resolve(name); });
        }

        e.BuiltTemplate    = tmpl.get();
        e.BuiltVersion     = mat->Version;
        e.BuiltTransparent = mat->Transparent;
        return e.Gpu;
    }

    void RayMaterialCache::Clear() {
        m_Pipelines.clear();
        m_Materials.clear();
        m_WhiteTexture   = nullptr;
        m_DefaultSampler = nullptr;
    }

} // namespace Echelon
