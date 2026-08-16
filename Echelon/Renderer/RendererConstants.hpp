#pragma once

/**
 * @file RendererConstants.hpp
 * @brief CPU-side mirrors of the engine's fixed shader ABI (Echelon.slang).
 *
 * Any renderer plugin that uploads g_Frame / g_Object should include this
 * header instead of redeclaring these structs locally. Sizes must stay in
 * sync with the std140 layout Slang reflects (Frame = 224 B, Object = 128 B).
 */

#include "Echelon/GraphicsAPI/ShaderReflection.hpp"

#include "glm/glm.hpp"

#include <cstdint>
#include <string>

namespace Echelon {

    struct FrameConstantsCPU {
        glm::mat4 View{ 1.0f };
        glm::mat4 Projection{ 1.0f };
        glm::mat4 ViewProjection{ 1.0f };
        glm::vec4 CameraPosition{ 0.0f };
        glm::vec4 TimeParams{ 0.0f };
    };

    struct ObjectConstantsCPU {
        glm::mat4 Model{ 1.0f };
        glm::mat4 NormalMatrix{ 1.0f };
    };

    // ---- Lighting scaffold: CPU mirror of Echelon.slang's g_Lights (std140). ----
    // Keep ECHELON_MAX_LIGHTS in sync with ECH_MAX_LIGHTS in Echelon.slang.
    constexpr int ECHELON_MAX_LIGHTS = 16;

    struct GpuLightCPU {
        glm::vec4 Position{ 0.0f };     // xyz world pos; w = type
        glm::vec4 Direction{ 0.0f, -1.0f, 0.0f, 10.0f }; // xyz dir; w = range
        glm::vec4 Color{ 1.0f };        // rgb; w = intensity
        glm::vec4 SpotParams{ 0.9f, 0.8f, 0.0f, 0.0f };  // inner/outer cos
    };

    struct LightConstantsCPU {
        glm::vec4   Ambient{ 0.03f, 0.03f, 0.03f, 1.0f };
        glm::ivec4  Count{ 0 };         // x = active light count
        GpuLightCPU Lights[ECHELON_MAX_LIGHTS];
    };

    // ---- Shadow mapping: CPU mirror of Echelon.slang's g_Shadows (std140). ----
    struct ShadowConstantsCPU {
        glm::mat4 DirViewProj{ 1.0f };
        glm::mat4 SpotViewProj{ 1.0f };
        glm::vec4 DirParams{ -1.0f, 0.0015f, 0.02f, 1.0f };   // x=caster idx(-1=none), y=depth bias, z=normal bias, w=strength
        glm::vec4 SpotParams{ -1.0f, 0.0015f, 0.02f, 1.0f };  // x=caster idx, y=depth bias, z=normal bias, w=strength
        glm::vec4 PointPosIndex{ 0.0f, 0.0f, 0.0f, -1.0f };   // xyz=point light pos, w=caster idx(-1=none)
        glm::vec4 PointParams{ 25.0f, 0.05f, 1.0f, 0.0f };    // x=far plane, y=depth bias, z=strength, w=reserved
        glm::vec4 MapParams{ 0.0f };                          // x=dir texel, y=spot texel, z=point texel, w=reserved
    };

    // ---- Per-view shadow depth pass: CPU mirror of Echelon.slang's g_ShadowPass. ----
    struct ShadowPassConstantsCPU {
        glm::mat4 LightViewProj{ 1.0f };
        glm::vec4 LightPosFar{ 0.0f, 0.0f, 0.0f, 25.0f };     // xyz=point light pos, w=far plane
    };

    // ---- Image-based lighting: CPU mirror of Echelon.slang's g_Ibl. ----
    struct IblConstantsCPU {
        glm::vec4 Params{ 4.0f, 1.0f, 0.0f, 0.0f };           // x=prefilter max mip, y=ambient intensity, z=enabled, w=reserved
    };

    // ---- IBL precompute: CPU mirror of IblCommon.slang's g_IblGen (renderer-internal). ----
    struct IblGenParamsCPU {
        glm::vec4 MaAxis{ 0.0f };   // cube face major axis
        glm::vec4 ScVec{ 0.0f };    // cube face s-axis
        glm::vec4 TcVec{ 0.0f };    // cube face t-axis
        glm::vec4 Params{ 0.0f };   // x = roughness, y = mip count
        glm::vec4 Sun{ 0.0f, 1.0f, 0.0f, 1.0f };   // xyz = toward sun, w = intensity
    };

    // Returns true and sets outBinding if the reflection contains a UBO named 'name'.
    inline bool FindUBOBinding(const ShaderReflection& refl, const char* name, uint32_t& outBinding) {
        for (const auto& ub : refl.UniformBuffers) {
            if (ub.Name == name) { outBinding = ub.Binding; return true; }
        }
        return false;
    }

    // Returns true and sets outBinding if the reflection contains a sampler named 'name'.
    inline bool FindSamplerBinding(const ShaderReflection& refl, const char* name, uint32_t& outBinding) {
        for (const auto& s : refl.Samplers) {
            if (s.Name == name) { outBinding = s.Binding; return true; }
        }
        return false;
    }

    // ---- Single source of truth for the engine's system resources (the fixed ABI). ----
    // These are resolved by name from reflection: the renderer binds them on the system
    // descriptor set, and the material system EXCLUDES them (so they are never mistaken
    // for material params / textures). Adding a system CB or sampler = one entry here.

    /** @brief True for an engine-provided system constant buffer (not a material param block). */
    inline bool IsSystemUniformName(const std::string& n) {
        return n == "g_Frame" || n == "g_Object" || n == "g_Lights"
            || n == "g_Shadows" || n == "g_ShadowPass" || n == "g_Ibl";
    }

    /** @brief True for an engine-provided system sampler (shadow maps + IBL), not a material texture. */
    inline bool IsSystemSamplerName(const std::string& n) {
        return n == "g_ShadowDir" || n == "g_ShadowSpot" || n == "g_ShadowPoint"
            || n == "g_IrradianceMap" || n == "g_PrefilterMap" || n == "g_BrdfLUT";
    }

} // namespace Echelon
