#include "Asset/Material/Material.hpp"
#include "Asset/Mesh/StandardVertex.hpp"
#include "Asset/AssetManager.hpp"
#include "Core/Log.hpp"

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

    const MaterialParam* Material::Resolve(const std::string& name) const {
        auto it = Params.find(name);
        if (it != Params.end()) return &it->second;
        if (m_Parent) return m_Parent->Resolve(name);
        return nullptr;
    }

    void Material::UploadGPU(RendererAPI* renderer) {
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

        // A 1x1 white fallback texture so shaders that sample a texture render even
        // before real texture assets exist. (Texture-asset loading is future work.)
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

        m_Resources = BuildMaterialResources(renderer, refl, m_DefaultTexture);
        Repack();
    }

    void Material::Repack() {
        PackMaterialResources(m_Resources,
            [this](const std::string& name) { return Resolve(name); });
    }

    void Material::ReleaseGPU() {
        m_Pipeline      = nullptr;
        m_Resources     = {};
        m_DefaultTexture = nullptr;
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
