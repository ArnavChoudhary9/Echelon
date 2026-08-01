#pragma once

/**
 * @file MaterialParam.hpp
 * @brief A typed material parameter value (packed into a shader's reflected UBO).
 *
 * Material parameters are reflection-driven: the editable set comes from a
 * shader's reflected uniform-buffer members. A MaterialParam stores one value in
 * a fixed 16-float scratch (enough for up to a mat4) plus a type tag for
 * serialization and correct byte-size packing.
 */

#include "glm/glm.hpp"

#include <cstdint>
#include <cstring>

namespace Echelon {

    enum class MaterialParamType : uint8_t {
        Float = 0, Float2, Float3, Float4, Int, Mat4
    };

    struct MaterialParam {
        MaterialParamType Type = MaterialParamType::Float4;
        float             Data[16] = { 0.0f };

        MaterialParam() = default;

        static MaterialParam Make(float v)            { MaterialParam p; p.Type = MaterialParamType::Float;  p.Data[0] = v; return p; }
        static MaterialParam Make(const glm::vec2& v) { MaterialParam p; p.Type = MaterialParamType::Float2; std::memcpy(p.Data, &v, sizeof(v)); return p; }
        static MaterialParam Make(const glm::vec3& v) { MaterialParam p; p.Type = MaterialParamType::Float3; std::memcpy(p.Data, &v, sizeof(v)); return p; }
        static MaterialParam Make(const glm::vec4& v) { MaterialParam p; p.Type = MaterialParamType::Float4; std::memcpy(p.Data, &v, sizeof(v)); return p; }
        static MaterialParam MakeInt(int v)           { MaterialParam p; p.Type = MaterialParamType::Int;    std::memcpy(p.Data, &v, sizeof(v)); return p; }
        static MaterialParam Make(const glm::mat4& v) { MaterialParam p; p.Type = MaterialParamType::Mat4;   std::memcpy(p.Data, &v, sizeof(v)); return p; }

        /** @brief Number of bytes this value occupies when packed into a UBO. */
        uint32_t ByteSize() const {
            switch (Type) {
                case MaterialParamType::Float:  return 4;
                case MaterialParamType::Float2: return 8;
                case MaterialParamType::Float3: return 12;
                case MaterialParamType::Float4: return 16;
                case MaterialParamType::Int:    return 4;
                case MaterialParamType::Mat4:   return 64;
            }
            return 16;
        }
    };

    inline const char* MaterialParamTypeToString(MaterialParamType t) {
        switch (t) {
            case MaterialParamType::Float:  return "Float";
            case MaterialParamType::Float2: return "Float2";
            case MaterialParamType::Float3: return "Float3";
            case MaterialParamType::Float4: return "Float4";
            case MaterialParamType::Int:    return "Int";
            case MaterialParamType::Mat4:   return "Mat4";
        }
        return "Float4";
    }

    inline MaterialParamType MaterialParamTypeFromString(const std::string& s) {
        if (s == "Float")  return MaterialParamType::Float;
        if (s == "Float2") return MaterialParamType::Float2;
        if (s == "Float3") return MaterialParamType::Float3;
        if (s == "Int")    return MaterialParamType::Int;
        if (s == "Mat4")   return MaterialParamType::Mat4;
        return MaterialParamType::Float4;
    }

} // namespace Echelon
