#pragma once

/**
 * @file Shader.hpp
 * @brief Compiled shader program interface and all shader-related types.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // ShaderStage

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    // ================================================================
    // Shader types
    // ================================================================

    /**
     * @brief Format of shader source or bytecode.
     */
    enum class ShaderSourceFormat : uint8_t
    {
        GLSL = 0,
        HLSL,
        SPIRV,
        MSL
    };

    /**
     * @brief Descriptor for specifying a single shader stage when creating
     *        a shader program.
     */
    struct ShaderStageDesc
    {
        ShaderStage            Stage      = ShaderStage::Vertex;
        ShaderSourceFormat     Format     = ShaderSourceFormat::SPIRV;
        std::vector<uint8_t>   Source;
        std::string            EntryPoint = "main";
    };

    /**
     * @brief Descriptor for creating a complete shader program.
     */
    struct ShaderDesc
    {
        std::vector<ShaderStageDesc> Stages;
        std::string                  DebugName = "";
    };

    // ================================================================
    // Shader interface
    // ================================================================

    /**
     * @brief Abstract compiled shader program.
     *
     * A shader program is composed of one or more stages (vertex, fragment,
     * compute, etc.) compiled from SPIR-V or native shader sources via the
     * backend.  Pipelines reference shaders as immutable objects.
     */
    class Shader
    {
    public:
        virtual ~Shader() = default;

        /**
         * @brief Get the debug name assigned to this shader.
         * @return const std::string&
         */
        virtual const std::string& GetName() const = 0;

        /**
         * @brief Query whether a specific shader stage is present.
         * @param stage The stage to check.
         * @return true if the shader contains the given stage.
         */
        virtual bool HasStage(ShaderStage stage) const = 0;

        // ---- Uniform setters ----
        // These allow renderers to set shader parameters without knowing
        // the concrete backend.  Default implementations are no-ops for
        // backends that use descriptor-sets / uniform-buffers instead.

        virtual void SetInt(const std::string& name, int value) const { (void)name; (void)value; }
        virtual void SetFloat(const std::string& name, float value) const { (void)name; (void)value; }
        virtual void SetVec3(const std::string& name, float x, float y, float z) const { (void)name; (void)x; (void)y; (void)z; }
        virtual void SetVec4(const std::string& name, float x, float y, float z, float w) const { (void)name; (void)x; (void)y; (void)z; (void)w; }
        virtual void SetMat4(const std::string& name, const float* value) const { (void)name; (void)value; }
    };

} // namespace Echelon
