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

    // Returns true and sets outBinding if the reflection contains a UBO named 'name'.
    inline bool FindUBOBinding(const ShaderReflection& refl, const char* name, uint32_t& outBinding) {
        for (const auto& ub : refl.UniformBuffers) {
            if (ub.Name == name) { outBinding = ub.Binding; return true; }
        }
        return false;
    }

} // namespace Echelon
