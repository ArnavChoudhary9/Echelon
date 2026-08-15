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

    // Returns true and sets outBinding if the reflection contains a UBO named 'name'.
    inline bool FindUBOBinding(const ShaderReflection& refl, const char* name, uint32_t& outBinding) {
        for (const auto& ub : refl.UniformBuffers) {
            if (ub.Name == name) { outBinding = ub.Binding; return true; }
        }
        return false;
    }

} // namespace Echelon
