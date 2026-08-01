#include "Asset/Importers/Shader/ShaderImporter.hpp"
#include "Asset/Shader/ShaderAsset.hpp"
#include "Renderer/RendererLoader.hpp"   // ExecutableDir() — engine shader include dir
#include "Core/Log.hpp"

#include <slang.h>
#include <slang-com-ptr.h>

#include <fstream>
#include <sstream>
#include <exception>
#include <vector>
#include <string>
#include <algorithm>

namespace Echelon {

    using Slang::ComPtr;

    // ------------------------------------------------------------------
    // Global session — created once, reused across imports and hot-reloads.
    // ------------------------------------------------------------------
    static slang::IGlobalSession* GetGlobalSession() {
        static ComPtr<slang::IGlobalSession> s_Global = [] {
            ComPtr<slang::IGlobalSession> g;
            slang::createGlobalSession(g.writeRef());
            return g;
        }();
        return s_Global.get();
    }

    // ------------------------------------------------------------------
    // Slang → engine enum/format mapping
    // ------------------------------------------------------------------
    static ShaderStage ToShaderStage(SlangStage stage) {
        switch (stage) {
            case SLANG_STAGE_VERTEX:   return ShaderStage::Vertex;
            case SLANG_STAGE_FRAGMENT: return ShaderStage::Fragment;
            case SLANG_STAGE_GEOMETRY: return ShaderStage::Geometry;
            case SLANG_STAGE_HULL:     return ShaderStage::TessellationControl;
            case SLANG_STAGE_DOMAIN:   return ShaderStage::TessellationEvaluation;
            case SLANG_STAGE_COMPUTE:  return ShaderStage::Compute;
            default:                   return ShaderStage::Vertex;
        }
    }

    // Map a Slang scalar+component count to an engine VertexAttributeFormat.
    static VertexAttributeFormat ToVertexFormat(slang::TypeReflection* type) {
        const unsigned comps = static_cast<unsigned>(type->getElementCount() > 0
                                                     ? type->getElementCount() : 1);
        const auto scalar = type->getScalarType();

        switch (scalar) {
            case slang::TypeReflection::Float32:
                switch (comps) {
                    case 1:  return VertexAttributeFormat::Float;
                    case 2:  return VertexAttributeFormat::Float2;
                    case 3:  return VertexAttributeFormat::Float3;
                    default: return VertexAttributeFormat::Float4;
                }
            case slang::TypeReflection::Int32:
                switch (comps) {
                    case 1:  return VertexAttributeFormat::Int;
                    case 2:  return VertexAttributeFormat::Int2;
                    case 3:  return VertexAttributeFormat::Int3;
                    default: return VertexAttributeFormat::Int4;
                }
            case slang::TypeReflection::UInt32:
                switch (comps) {
                    case 1:  return VertexAttributeFormat::UInt;
                    case 2:  return VertexAttributeFormat::UInt2;
                    case 3:  return VertexAttributeFormat::UInt3;
                    default: return VertexAttributeFormat::UInt4;
                }
            default:
                return VertexAttributeFormat::Float3;
        }
    }

    // Emit vertex inputs from a varying-input variable: recurse structs, else emit one.
    static void CollectVertexInputs(slang::VariableLayoutReflection* var,
                                    std::vector<ReflectedVertexInput>& out) {
        slang::TypeLayoutReflection* tl = var->getTypeLayout();
        if (tl->getKind() == slang::TypeReflection::Kind::Struct) {
            const unsigned fields = tl->getFieldCount();
            for (unsigned f = 0; f < fields; ++f)
                CollectVertexInputs(tl->getFieldByIndex(f), out);
            return;
        }

        ReflectedVertexInput vi;
        vi.Name     = var->getName() ? var->getName() : "";
        vi.Location = static_cast<uint32_t>(var->getOffset(SLANG_PARAMETER_CATEGORY_VARYING_INPUT));
        vi.Format   = ToVertexFormat(var->getType());
        out.push_back(std::move(vi));
    }

    // ------------------------------------------------------------------
    // Reflection extraction
    // ------------------------------------------------------------------
    static void ExtractReflection(slang::ProgramLayout* layout, uint32_t programStageMask,
                                  ShaderReflection& refl) {
        // Vertex inputs: from the vertex entry point's parameters.
        const unsigned epCount = layout->getEntryPointCount();
        for (unsigned e = 0; e < epCount; ++e) {
            slang::EntryPointReflection* ep = layout->getEntryPointByIndex(e);
            if (ep->getStage() != SLANG_STAGE_VERTEX) continue;
            for (unsigned p = 0; p < ep->getParameterCount(); ++p) {
                slang::VariableLayoutReflection* param = ep->getParameterByIndex(p);
                if (param->getCategory() != slang::ParameterCategory::VaryingInput) continue;
                CollectVertexInputs(param, refl.VertexInputs);
            }
        }
        std::sort(refl.VertexInputs.begin(), refl.VertexInputs.end(),
                  [](const ReflectedVertexInput& a, const ReflectedVertexInput& b) {
                      return a.Location < b.Location;
                  });

        // Global parameters: constant buffers → UBOs; textures/samplers → samplers.
        const unsigned paramCount = layout->getParameterCount();
        for (unsigned i = 0; i < paramCount; ++i) {
            slang::VariableLayoutReflection* p  = layout->getParameterByIndex(i);
            slang::TypeLayoutReflection*     tl = p->getTypeLayout();
            const auto kind = tl->getKind();

            if (kind == slang::TypeReflection::Kind::ConstantBuffer) {
                ReflectedUniformBuffer ub;
                ub.Name      = p->getName() ? p->getName() : "";
                ub.Binding   = p->getBindingIndex();
                ub.Set       = p->getBindingSpace();
                ub.StageMask = programStageMask;

                slang::TypeLayoutReflection* elem = tl->getElementTypeLayout();
                if (elem) {
                    ub.Size = static_cast<uint32_t>(elem->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
                    const unsigned members = elem->getFieldCount();
                    for (unsigned m = 0; m < members; ++m) {
                        slang::VariableLayoutReflection* fld = elem->getFieldByIndex(m);
                        ReflectedUniformMember rm;
                        rm.Name   = fld->getName() ? fld->getName() : "";
                        rm.Offset = static_cast<uint32_t>(fld->getOffset(SLANG_PARAMETER_CATEGORY_UNIFORM));
                        rm.Size   = static_cast<uint32_t>(fld->getTypeLayout()->getSize(SLANG_PARAMETER_CATEGORY_UNIFORM));
                        // Fallback: derive total size if the buffer size came back 0.
                        if (ub.Size == 0) ub.Size = rm.Offset + rm.Size;
                        else              ub.Size = std::max<uint32_t>(ub.Size, rm.Offset + rm.Size);
                        ub.Members.push_back(std::move(rm));
                    }
                }
                refl.UniformBuffers.push_back(std::move(ub));
            }
            else if (kind == slang::TypeReflection::Kind::Resource ||
                     kind == slang::TypeReflection::Kind::SamplerState) {
                ReflectedSampler s;
                s.Name    = p->getName() ? p->getName() : "";
                s.Binding = p->getBindingIndex();
                s.Set     = p->getBindingSpace();
                refl.Samplers.push_back(std::move(s));
            }
        }
    }

    // ------------------------------------------------------------------
    // Import
    // ------------------------------------------------------------------
    ImportResult ShaderImporter::Import(const ImportContext& ctx) {
        const std::string path = ctx.GetPathString();

        try {
            slang::IGlobalSession* global = GetGlobalSession();
            if (!global)
                return ImportResult("Slang: failed to create global session");

            // Target SPIR-V. spirv_1_5 is broadly supported; GL_ARB_gl_spirv ingests it.
            slang::TargetDesc target = {};
            target.format  = SLANG_SPIRV;
            target.profile = global->findProfile("spirv_1_5");

            // Search paths: the shader's own directory + the engine shader-include
            // directory (<exe>/Shaders), where the engine ships Echelon.slang — so
            // any shader anywhere can `import Echelon;` (the shared constant system).
            const std::string dir       = ctx.GetPath().parent_path().string();
            const std::string engineDir = (RendererLoader::ExecutableDir() / "Shaders").string();

            std::vector<const char*> searchPaths;
            if (!dir.empty())       searchPaths.push_back(dir.c_str());
            if (!engineDir.empty()) searchPaths.push_back(engineDir.c_str());

            // Preserve entry-point names in the SPIR-V (Slang emits "main" by
            // default). glSpecializeShader looks up the entry by its emitted name,
            // so this keeps "vertexMain"/"fragmentMain" and lets one module carry
            // multiple distinct entry points.
            slang::CompilerOptionEntry options[1] = {};
            options[0].name             = slang::CompilerOptionName::VulkanUseEntryPointName;
            options[0].value.kind       = slang::CompilerOptionValueKind::Int;
            options[0].value.intValue0  = 1;

            slang::SessionDesc sd = {};
            sd.targets              = &target;
            sd.targetCount          = 1;
            sd.searchPaths          = searchPaths.data();
            sd.searchPathCount      = static_cast<SlangInt>(searchPaths.size());
            sd.compilerOptionEntries    = options;
            sd.compilerOptionEntryCount = 1;
            // Match glm's column-major storage so shaders use the natural mul(M, v).
            sd.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

            ComPtr<slang::ISession> session;
            if (SLANG_FAILED(global->createSession(sd, session.writeRef())))
                return ImportResult("Slang: failed to create session");

            // Read the source ourselves (robust vs. search-path module resolution).
            std::ifstream fin(path, std::ios::binary);
            if (!fin.is_open())
                return ImportResult("Slang: cannot open '" + path + "'");
            std::stringstream ss;
            ss << fin.rdbuf();
            const std::string source = ss.str();

            const std::string moduleName = ctx.GetPath().stem().string();

            ComPtr<slang::IBlob> diagnostics;
            slang::IModule* module = session->loadModuleFromSourceString(
                moduleName.c_str(), path.c_str(), source.c_str(), diagnostics.writeRef());
            if (diagnostics && diagnostics->getBufferSize())
                ECHELON_LOG_WARN("[ShaderImporter] '{}': {}", path,
                                 static_cast<const char*>(diagnostics->getBufferPointer()));
            if (!module)
                return ImportResult("Slang: failed to load module '" + path + "'");

            // Gather all [shader(...)] entry points defined in the module.
            const SlangInt32 epCount = module->getDefinedEntryPointCount();
            if (epCount == 0)
                return ImportResult("Slang: no [shader(...)] entry points in '" + path + "'");

            std::vector<ComPtr<slang::IEntryPoint>> entryPoints;
            std::vector<slang::IComponentType*>     components;
            components.push_back(module);
            for (SlangInt32 i = 0; i < epCount; ++i) {
                ComPtr<slang::IEntryPoint> ep;
                if (SLANG_FAILED(module->getDefinedEntryPoint(i, ep.writeRef())) || !ep)
                    return ImportResult("Slang: failed to get entry point");
                entryPoints.push_back(ep);
                components.push_back(ep.get());
            }

            // Compose + link the program.
            ComPtr<slang::IComponentType> composed;
            {
                ComPtr<slang::IBlob> d;
                if (SLANG_FAILED(session->createCompositeComponentType(
                        components.data(), static_cast<SlangInt>(components.size()),
                        composed.writeRef(), d.writeRef()))) {
                    return ImportResult(std::string("Slang: compose failed: ") +
                        (d ? static_cast<const char*>(d->getBufferPointer()) : "unknown"));
                }
            }
            ComPtr<slang::IComponentType> program;
            {
                ComPtr<slang::IBlob> d;
                if (SLANG_FAILED(composed->link(program.writeRef(), d.writeRef()))) {
                    return ImportResult(std::string("Slang: link failed: ") +
                        (d ? static_cast<const char*>(d->getBufferPointer()) : "unknown"));
                }
            }

            slang::ProgramLayout* layout = program->getLayout(0, diagnostics.writeRef());
            if (!layout)
                return ImportResult("Slang: no program layout for '" + path + "'");

            // Compute the union of stages present (used for UBO stage masks).
            uint32_t programStageMask = 0;
            for (unsigned e = 0; e < layout->getEntryPointCount(); ++e)
                programStageMask |= StageBit(ToShaderStage(layout->getEntryPointByIndex(e)->getStage()));

            // Build the ShaderDesc: one SPIR-V stage per entry point.
            ShaderDesc desc;
            desc.DebugName = moduleName;
            for (SlangInt32 i = 0; i < epCount; ++i) {
                ComPtr<slang::IBlob> code, d;
                if (SLANG_FAILED(program->getEntryPointCode(i, 0, code.writeRef(), d.writeRef())) || !code) {
                    return ImportResult(std::string("Slang: SPIR-V codegen failed: ") +
                        (d ? static_cast<const char*>(d->getBufferPointer()) : "unknown"));
                }

                slang::EntryPointReflection* epr = layout->getEntryPointByIndex(i);

                ShaderStageDesc stage;
                stage.Stage      = ToShaderStage(epr->getStage());
                stage.Format     = ShaderSourceFormat::SPIRV;
                stage.EntryPoint = epr->getName() ? epr->getName() : "main";
                const auto* bytes = static_cast<const uint8_t*>(code->getBufferPointer());
                stage.Source.assign(bytes, bytes + code->getBufferSize());
                desc.Stages.push_back(std::move(stage));
            }

            ExtractReflection(layout, programStageMask, desc.Reflection);

            auto asset = CreateRef<ShaderAsset>();
            asset->SetDesc(std::move(desc));

            ECHELON_LOG_INFO("[ShaderImporter] Compiled '{}' ({} stages, {} vtx inputs, {} UBOs, {} samplers).",
                             path, epCount, asset->GetReflection().VertexInputs.size(),
                             asset->GetReflection().UniformBuffers.size(),
                             asset->GetReflection().Samplers.size());
            return ImportResult(asset);
        }
        catch (const std::exception& e) {
            ECHELON_LOG_ERROR("[ShaderImporter] Exception compiling '{}': {}", path, e.what());
            return ImportResult(std::string("Exception: ") + e.what());
        }
    }

} // namespace Echelon
