#include "Asset/Material/Material.hpp"
#include "Asset/Mesh/StandardVertex.hpp"
#include "Asset/Texture/TextureAsset.hpp"
#include "Asset/AssetManager.hpp"
#include "Core/Log.hpp"
#include "Instrumentation/Instrumentation.hpp"

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
    // Uploads the texture asset (idempotent) so it is ready regardless of the order
    // assets are rebuilt in on a renderer hot-swap.
    static Ref<Texture> ResolveMaterialTexture(RendererAPI* renderer,
                                               const std::string& samplerName,
                                               const std::string& path) {
        if (path.empty()) return nullptr;
        auto& assets = AssetManager::Get();
        UUID handle = assets.GetHandle(path);
        if (handle.IsNull()) return nullptr;

        auto texAsset = assets.GetAssetAs<TextureAsset>(handle);
        if (!texAsset) {
            ECHELON_LOG_WARN("[Material] Texture '{}' for sampler '{}' is not a texture asset.",
                             path, samplerName);
            return nullptr;
        }
        texAsset->UploadGPU(renderer);  // idempotent; guarantees readiness
        return texAsset->GetGpuTexture();
    }

    const MaterialParam* Material::Resolve(const std::string& name) const {
        auto it = Params.find(name);
        if (it != Params.end()) return &it->second;
        if (m_Parent) return m_Parent->Resolve(name);
        return nullptr;
    }

    void Material::UploadGPU(RendererAPI* renderer) {
        ECHELON_PROFILE_FUNCTION();
        if (!renderer) return;
        auto device = renderer->GetDevice();
        if (!device) return;

        // Resolve + upload the shader.
        m_Shader = ResolveShader(ShaderHandle, ShaderSource);
        if (!m_Shader) {
            ECHELON_LOG_ERROR("[Material] '{}' could not resolve its shader", ShaderSource);
            return;
        }
        m_Shader->UploadGPU(renderer);  // no-op if already uploaded
        if (!m_Shader->GetGpuShader()) {
            ECHELON_LOG_ERROR("[Material] shader '{}' failed to build a GPU program", ShaderSource);
            return;
        }

        // Resolve + upload the optional parent material (a saved instance).
        if (!m_Parent && !ParentHandle.IsNull()) {
            m_Parent = AssetManager::Get().GetAssetAs<Material>(ParentHandle);
            if (m_Parent) m_Parent->UploadGPU(renderer);
        }

        const ShaderReflection& refl = m_Shader->GetReflection();

        // Build the pipeline from reflection (vertex layout) — no hand-written layout.
        PipelineDesc pd;
        pd.ShaderProgram = m_Shader->GetGpuShader();
        pd.Layout        = StandardVertex::FromReflection(refl);
        pd.Topology      = PrimitiveTopology::TriangleList;
        pd.Depth.DepthTestEnable  = true;
        pd.Depth.DepthWriteEnable = true;
        pd.Raster.Cull            = CullMode::None;
        pd.DebugName     = "Material_Pipeline";
        m_Pipeline = device->CreatePipeline(pd);

        // 1×1 white fallback texture + default sampler so shaders that sample a texture
        // render correctly before real texture assets are assigned.
        if (!m_DefaultTexture) {
            TextureDesc td;
            td.Width = 1; td.Height = 1;
            td.Format = TextureFormat::RGBA8_UNORM;
            td.Usage  = TextureUsage::Sampled;
            td.DebugName = "Material_WhiteFallback";
            m_DefaultTexture = device->CreateTexture(td);
            const uint8_t white[4] = { 255, 255, 255, 255 };
            m_DefaultTexture->SetData(white, sizeof(white));
        }
        if (!m_DefaultSampler) {
            SamplerDesc sd;   // defaults: Linear, Repeat
            m_DefaultSampler = device->CreateSampler(sd);
        }

        // Bind real texture assets to reflected samplers by name (falling back to
        // the white texture for any sampler without a matching entry in Textures).
        auto resolver = [this, renderer](const std::string& samplerName) -> Ref<Texture> {
            auto it = Textures.find(samplerName);
            if (it == Textures.end()) return nullptr;   // → white fallback
            return ResolveMaterialTexture(renderer, samplerName, it->second);
        };

        m_Resources = BuildMaterialResources(renderer, refl, m_DefaultTexture,
                                             m_DefaultSampler, resolver);
        Repack();

        if (!Textures.empty())
            ECHELON_LOG_DEBUG("[Material] '{}' bound {} texture(s) across {} reflected sampler(s).",
                              ShaderSource, Textures.size(), refl.Samplers.size());
    }

    void Material::Repack() {
        PackMaterialResources(m_Resources,
            [this](const std::string& name) { return Resolve(name); });
    }

    void Material::ReleaseGPU() {
        m_Pipeline       = nullptr;
        m_Resources      = {};
        m_DefaultTexture = nullptr;
        m_DefaultSampler = nullptr;
        // m_Shader/m_Parent are assets owned by the AssetManager — do not release here.
    }

    void Material::ReloadFrom(const Ref<Asset>& fresh) {
        auto other = std::dynamic_pointer_cast<Material>(fresh);
        if (!other) return;
        ReleaseGPU();
        ShaderHandle = other->ShaderHandle;
        ShaderSource = other->ShaderSource;
        ParentHandle = other->ParentHandle;
        ParentSource = other->ParentSource;
        Params       = other->Params;
        Textures     = other->Textures;
        m_Shader     = nullptr;
        m_Parent     = nullptr;
    }

} // namespace Echelon
