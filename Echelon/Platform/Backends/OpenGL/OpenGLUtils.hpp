#pragma once

/**
 * @file OpenGLUtils.hpp
 * @brief Conversion helpers between Echelon GraphicsAPI enums and OpenGL constants.
 */

#include <glad/gl.h>

#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"
#include "Echelon/GraphicsAPI/Buffer.hpp"
#include "Echelon/GraphicsAPI/Texture.hpp"
#include "Echelon/GraphicsAPI/Pipeline.hpp"
#include "Echelon/GraphicsAPI/RenderPass.hpp"

namespace Echelon::OpenGLUtils {

    // ================================================================
    // Texture format conversions
    // ================================================================

    inline GLenum ToGLInternalFormat(TextureFormat fmt)
    {
        switch (fmt) {
            case TextureFormat::R8_UNORM:           return GL_R8;
            case TextureFormat::RG8_UNORM:          return GL_RG8;
            case TextureFormat::RGBA8_UNORM:        return GL_RGBA8;
            case TextureFormat::RGBA8_SRGB:         return GL_SRGB8_ALPHA8;
            case TextureFormat::BGRA8_UNORM:        return GL_RGBA8;  // GL doesn't distinguish BGRA internal
            case TextureFormat::BGRA8_SRGB:         return GL_SRGB8_ALPHA8;
            case TextureFormat::R16_FLOAT:          return GL_R16F;
            case TextureFormat::RG16_FLOAT:         return GL_RG16F;
            case TextureFormat::RGBA16_FLOAT:       return GL_RGBA16F;
            case TextureFormat::R32_FLOAT:          return GL_R32F;
            case TextureFormat::RG32_FLOAT:         return GL_RG32F;
            case TextureFormat::RGBA32_FLOAT:       return GL_RGBA32F;
            case TextureFormat::D16_UNORM:          return GL_DEPTH_COMPONENT16;
            case TextureFormat::D24_UNORM_S8_UINT:  return GL_DEPTH24_STENCIL8;
            case TextureFormat::D32_FLOAT:          return GL_DEPTH_COMPONENT32F;
            case TextureFormat::D32_FLOAT_S8_UINT:  return GL_DEPTH32F_STENCIL8;
            default:                                return GL_RGBA8;
        }
    }

    inline GLenum ToGLFormat(TextureFormat fmt)
    {
        if (IsDepthFormat(fmt)) {
            switch (fmt) {
                case TextureFormat::D24_UNORM_S8_UINT:
                case TextureFormat::D32_FLOAT_S8_UINT:
                    return GL_DEPTH_STENCIL;
                default:
                    return GL_DEPTH_COMPONENT;
            }
        }
        switch (fmt) {
            case TextureFormat::R8_UNORM:
            case TextureFormat::R16_FLOAT:
            case TextureFormat::R32_FLOAT:      return GL_RED;
            case TextureFormat::RG8_UNORM:
            case TextureFormat::RG16_FLOAT:
            case TextureFormat::RG32_FLOAT:     return GL_RG;
            case TextureFormat::BGRA8_UNORM:
            case TextureFormat::BGRA8_SRGB:     return GL_BGRA;
            default:                            return GL_RGBA;
        }
    }

    inline GLenum ToGLDataType(TextureFormat fmt)
    {
        switch (fmt) {
            case TextureFormat::RGBA16_FLOAT:
            case TextureFormat::RG16_FLOAT:
            case TextureFormat::R16_FLOAT:      return GL_HALF_FLOAT;
            case TextureFormat::RGBA32_FLOAT:
            case TextureFormat::RG32_FLOAT:
            case TextureFormat::R32_FLOAT:
            case TextureFormat::D32_FLOAT:      return GL_FLOAT;
            case TextureFormat::D24_UNORM_S8_UINT:
                return GL_UNSIGNED_INT_24_8;
            case TextureFormat::D32_FLOAT_S8_UINT:
                return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
            default:                            return GL_UNSIGNED_BYTE;
        }
    }

    // ================================================================
    // Vertex attribute format
    // ================================================================

    inline GLenum ToGLBaseType(VertexAttributeFormat fmt)
    {
        switch (fmt) {
            case VertexAttributeFormat::Float:
            case VertexAttributeFormat::Float2:
            case VertexAttributeFormat::Float3:
            case VertexAttributeFormat::Float4:  return GL_FLOAT;
            case VertexAttributeFormat::Int:
            case VertexAttributeFormat::Int2:
            case VertexAttributeFormat::Int3:
            case VertexAttributeFormat::Int4:    return GL_INT;
            case VertexAttributeFormat::UInt:
            case VertexAttributeFormat::UInt2:
            case VertexAttributeFormat::UInt3:
            case VertexAttributeFormat::UInt4:   return GL_UNSIGNED_INT;
            default:                             return GL_FLOAT;
        }
    }

    inline uint32_t GetComponentCount(VertexAttributeFormat fmt)
    {
        switch (fmt) {
            case VertexAttributeFormat::Float:
            case VertexAttributeFormat::Int:
            case VertexAttributeFormat::UInt:    return 1;
            case VertexAttributeFormat::Float2:
            case VertexAttributeFormat::Int2:
            case VertexAttributeFormat::UInt2:   return 2;
            case VertexAttributeFormat::Float3:
            case VertexAttributeFormat::Int3:
            case VertexAttributeFormat::UInt3:   return 3;
            case VertexAttributeFormat::Float4:
            case VertexAttributeFormat::Int4:
            case VertexAttributeFormat::UInt4:   return 4;
            default:                             return 0;
        }
    }

    inline bool IsIntegerFormat(VertexAttributeFormat fmt)
    {
        switch (fmt) {
            case VertexAttributeFormat::Int:
            case VertexAttributeFormat::Int2:
            case VertexAttributeFormat::Int3:
            case VertexAttributeFormat::Int4:
            case VertexAttributeFormat::UInt:
            case VertexAttributeFormat::UInt2:
            case VertexAttributeFormat::UInt3:
            case VertexAttributeFormat::UInt4:   return true;
            default:                             return false;
        }
    }

    // ================================================================
    // Pipeline state conversions
    // ================================================================

    inline GLenum ToGLTopology(PrimitiveTopology topo)
    {
        switch (topo) {
            case PrimitiveTopology::TriangleList:   return GL_TRIANGLES;
            case PrimitiveTopology::TriangleStrip:  return GL_TRIANGLE_STRIP;
            case PrimitiveTopology::LineList:        return GL_LINES;
            case PrimitiveTopology::LineStrip:       return GL_LINE_STRIP;
            case PrimitiveTopology::PointList:       return GL_POINTS;
            default:                                return GL_TRIANGLES;
        }
    }

    inline GLenum ToGLPolygonMode(PolygonMode mode)
    {
        switch (mode) {
            case PolygonMode::Fill:      return GL_FILL;
            case PolygonMode::Wireframe: return GL_LINE;
            case PolygonMode::Point:     return GL_POINT;
            default:                     return GL_FILL;
        }
    }

    inline GLenum ToGLCullFace(CullMode mode)
    {
        switch (mode) {
            case CullMode::Front:        return GL_FRONT;
            case CullMode::Back:         return GL_BACK;
            case CullMode::FrontAndBack: return GL_FRONT_AND_BACK;
            default:                     return GL_BACK;
        }
    }

    inline GLenum ToGLFrontFace(FrontFace winding)
    {
        return winding == FrontFace::CounterClockwise ? GL_CCW : GL_CW;
    }

    inline GLenum ToGLCompareFunc(CompareOp op)
    {
        switch (op) {
            case CompareOp::Never:          return GL_NEVER;
            case CompareOp::Less:           return GL_LESS;
            case CompareOp::Equal:          return GL_EQUAL;
            case CompareOp::LessOrEqual:    return GL_LEQUAL;
            case CompareOp::Greater:        return GL_GREATER;
            case CompareOp::NotEqual:       return GL_NOTEQUAL;
            case CompareOp::GreaterOrEqual: return GL_GEQUAL;
            case CompareOp::Always:         return GL_ALWAYS;
            default:                        return GL_LESS;
        }
    }

    inline GLenum ToGLBlendFactor(BlendFactor f)
    {
        switch (f) {
            case BlendFactor::Zero:             return GL_ZERO;
            case BlendFactor::One:              return GL_ONE;
            case BlendFactor::SrcColor:         return GL_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
            case BlendFactor::DstColor:         return GL_DST_COLOR;
            case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
            case BlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstAlpha:         return GL_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
            default:                            return GL_ONE;
        }
    }

    inline GLenum ToGLBlendOp(BlendOp op)
    {
        switch (op) {
            case BlendOp::Add:             return GL_FUNC_ADD;
            case BlendOp::Subtract:        return GL_FUNC_SUBTRACT;
            case BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
            case BlendOp::Min:             return GL_MIN;
            case BlendOp::Max:             return GL_MAX;
            default:                       return GL_FUNC_ADD;
        }
    }

    inline GLenum ToGLIndexType(IndexType type)
    {
        return type == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    }

    inline GLenum ToGLFilterMode(FilterMode mode)
    {
        return mode == FilterMode::Nearest ? GL_NEAREST : GL_LINEAR;
    }

    inline GLenum ToGLAddressMode(AddressMode mode)
    {
        switch (mode) {
            case AddressMode::Repeat:         return GL_REPEAT;
            case AddressMode::MirroredRepeat: return GL_MIRRORED_REPEAT;
            case AddressMode::ClampToEdge:    return GL_CLAMP_TO_EDGE;
            case AddressMode::ClampToBorder:  return GL_CLAMP_TO_BORDER;
            default:                          return GL_REPEAT;
        }
    }

    inline GLenum ToGLShaderStage(ShaderStage stage)
    {
        switch (stage) {
            case ShaderStage::Vertex:                 return GL_VERTEX_SHADER;
            case ShaderStage::Fragment:               return GL_FRAGMENT_SHADER;
            case ShaderStage::Geometry:               return GL_GEOMETRY_SHADER;
            case ShaderStage::TessellationControl:    return GL_TESS_CONTROL_SHADER;
            case ShaderStage::TessellationEvaluation: return GL_TESS_EVALUATION_SHADER;
            case ShaderStage::Compute:                return GL_COMPUTE_SHADER;
            default:                                  return GL_VERTEX_SHADER;
        }
    }

} // namespace Echelon::OpenGLUtils
