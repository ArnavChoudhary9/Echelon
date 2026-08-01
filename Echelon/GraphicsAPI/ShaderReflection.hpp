#pragma once

/**
 * @file ShaderReflection.hpp
 * @brief Backend-neutral shader reflection data.
 *
 * These are plain engine structs — deliberately free of any Slang / SPIRV-Cross
 * dependency — so both the graphics backends (OpenGL, ...) and renderer plugins
 * (Ray) and the material system can consume reflection without linking a shader
 * compiler. The ShaderImporter populates them from Slang's reflection API; the
 * data then rides on ShaderDesc and ShaderAsset.
 */

#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // ShaderStage
#include "Echelon/GraphicsAPI/Pipeline.hpp"        // VertexAttributeFormat

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    /**
     * @brief A single vertex input attribute declared by a vertex shader.
     *
     * `Location` is the explicit SPIR-V input location (from the shader source's
     * semantic ordering). The engine's canonical vertex (StandardVertex) is
     * matched against this location, so the mesh loader never needs to know a
     * specific shader's attribute layout.
     */
    struct ReflectedVertexInput
    {
        std::string           Name;                                  ///< Field/semantic name (e.g. "position" / "POSITION").
        uint32_t              Location = 0;                          ///< SPIR-V input location.
        VertexAttributeFormat Format   = VertexAttributeFormat::Float3;
    };

    /**
     * @brief A member (field) of a reflected uniform/constant buffer.
     * Offsets/sizes are in the target's uniform layout (std140-compatible).
     */
    struct ReflectedUniformMember
    {
        std::string Name;
        uint32_t    Offset = 0;   ///< Byte offset within the buffer.
        uint32_t    Size   = 0;   ///< Byte size of the member.
    };

    /**
     * @brief A reflected uniform (constant) buffer.
     */
    struct ReflectedUniformBuffer
    {
        std::string                         Name;
        uint32_t                            Binding   = 0;   ///< SPIR-V binding index.
        uint32_t                            Set       = 0;   ///< SPIR-V descriptor set / space (GL ignores).
        uint32_t                            Size      = 0;   ///< Total buffer size in bytes.
        uint32_t                            StageMask = 0;   ///< Bitmask of (1u << ShaderStage) it appears in.
        std::vector<ReflectedUniformMember> Members;
    };

    /**
     * @brief A reflected texture/sampler resource.
     */
    struct ReflectedSampler
    {
        std::string Name;
        uint32_t    Binding = 0;   ///< SPIR-V binding index (maps to a GL texture unit).
        uint32_t    Set     = 0;
    };

    /**
     * @brief Full reflection for a compiled shader program.
     */
    struct ShaderReflection
    {
        std::vector<ReflectedVertexInput>   VertexInputs;   ///< Sorted by Location.
        std::vector<ReflectedUniformBuffer> UniformBuffers;
        std::vector<ReflectedSampler>       Samplers;
    };

    /** @brief Convenience: stage → single-bit mask for ReflectedUniformBuffer::StageMask. */
    inline uint32_t StageBit(ShaderStage stage) { return 1u << static_cast<uint32_t>(stage); }

} // namespace Echelon
