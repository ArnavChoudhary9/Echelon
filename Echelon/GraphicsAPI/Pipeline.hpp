#pragma once

/**
 * @file Pipeline.hpp
 * @brief Graphics & compute pipeline interfaces and all pipeline-state types
 *        (vertex layout, raster, depth, blend).
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // CompareOp

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    // Forward declarations
    class Shader;
    class RenderPass;
    class DescriptorSetLayout;

    // ================================================================
    // Vertex layout
    // ================================================================

    /**
     * @brief Vertex attribute data type.
     */
    enum class VertexAttributeFormat : uint8_t
    {
        Float  = 0,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4,
        UInt,
        UInt2,
        UInt3,
        UInt4
    };

    /**
     * @brief Describes a single vertex attribute within a vertex layout.
     */
    struct VertexAttribute
    {
        std::string           Name;
        VertexAttributeFormat Format  = VertexAttributeFormat::Float3;
        uint32_t              Offset  = 0;
        uint32_t              Binding = 0;
    };

    /**
     * @brief The rate at which vertex data is consumed.
     */
    enum class VertexInputRate : uint8_t
    {
        PerVertex = 0,
        PerInstance
    };

    /**
     * @brief Describes a single vertex buffer binding within a vertex layout.
     */
    struct VertexBinding
    {
        uint32_t        Binding   = 0;
        uint32_t        Stride    = 0;
        VertexInputRate InputRate = VertexInputRate::PerVertex;
    };

    /**
     * @brief Describes the complete vertex input layout for a graphics pipeline.
     */
    struct VertexLayout
    {
        std::vector<VertexBinding>   Bindings;
        std::vector<VertexAttribute> Attributes;
    };

    // ================================================================
    // Primitive topology
    // ================================================================

    /**
     * @brief Primitive topology for draw calls.
     */
    enum class PrimitiveTopology : uint8_t
    {
        TriangleList = 0,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList
    };

    // ================================================================
    // Rasterizer state
    // ================================================================

    /**
     * @brief Polygon fill mode.
     */
    enum class PolygonMode : uint8_t
    {
        Fill = 0,
        Wireframe,
        Point
    };

    /**
     * @brief Face culling mode.
     */
    enum class CullMode : uint8_t
    {
        None = 0,
        Front,
        Back,
        FrontAndBack
    };

    /**
     * @brief Front-face winding order.
     */
    enum class FrontFace : uint8_t
    {
        CounterClockwise = 0,
        Clockwise
    };

    /**
     * @brief Rasterizer state descriptor.
     */
    struct RasterState
    {
        PolygonMode Polygon          = PolygonMode::Fill;
        CullMode    Cull             = CullMode::Back;
        FrontFace   Winding          = FrontFace::CounterClockwise;
        bool        DepthClampEnable = false;
    };

    // ================================================================
    // Depth / stencil state
    // ================================================================

    /**
     * @brief Depth / stencil state descriptor.
     */
    struct DepthState
    {
        bool      DepthTestEnable  = true;
        bool      DepthWriteEnable = true;
        CompareOp DepthCompareOp   = CompareOp::Less;
    };

    // ================================================================
    // Blend state
    // ================================================================

    /**
     * @brief Blend factor for color blending.
     */
    enum class BlendFactor : uint8_t
    {
        Zero = 0,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha
    };

    /**
     * @brief Blend operation.
     */
    enum class BlendOp : uint8_t
    {
        Add = 0,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    /**
     * @brief Blend state for a single color attachment.
     */
    struct BlendAttachment
    {
        bool        BlendEnable   = false;
        BlendFactor SrcColorBlend = BlendFactor::SrcAlpha;
        BlendFactor DstColorBlend = BlendFactor::OneMinusSrcAlpha;
        BlendOp     ColorBlendOp  = BlendOp::Add;
        BlendFactor SrcAlphaBlend = BlendFactor::One;
        BlendFactor DstAlphaBlend = BlendFactor::Zero;
        BlendOp     AlphaBlendOp  = BlendOp::Add;
    };

    /**
     * @brief Full blend state descriptor for a pipeline.
     */
    struct BlendState
    {
        std::vector<BlendAttachment> Attachments;
    };

    // ================================================================
    // Pipeline descriptors
    // ================================================================

    /**
     * @brief Descriptor for creating a graphics pipeline.
     */
    struct PipelineDesc
    {
        Ref<Shader>                           ShaderProgram = nullptr;
        VertexLayout                          Layout        = {};
        BlendState                            Blend         = {};
        DepthState                            Depth         = {};
        RasterState                           Raster        = {};
        PrimitiveTopology                     Topology      = PrimitiveTopology::TriangleList;
        Ref<RenderPass>                       Pass          = nullptr;
        std::vector<Ref<DescriptorSetLayout>> SetLayouts;
        std::string                           DebugName     = "";
    };

    /**
     * @brief Descriptor for creating a compute pipeline.
     */
    struct ComputePipelineDesc
    {
        Ref<Shader>                           ComputeShader = nullptr;
        std::vector<Ref<DescriptorSetLayout>> SetLayouts;
        std::string                           DebugName     = "";
    };

    // ================================================================
    // Pipeline interfaces
    // ================================================================

    /**
     * @brief Abstract graphics (rasterization) pipeline.
     *
     * Encapsulates all fixed-function and programmable state required for a
     * draw call: shaders, vertex layout, blend/depth/raster state, and the
     * render pass it is compatible with.
     *
     * Pipelines are immutable once created.
     */
    class Pipeline
    {
    public:
        virtual ~Pipeline() = default;

        /**
         * @brief Get the shader program associated with this pipeline.
         * @return Ref<Shader> Handle to the shader, or nullptr.
         */
        virtual Ref<Shader> GetShader() const = 0;
    };

    /**
     * @brief Abstract compute pipeline.
     *
     * Encapsulates a compute shader and its resource layout for GPU compute
     * dispatch calls (particle sim, post-processing, culling, etc.).
     *
     * Compute pipelines are immutable once created.
     */
    class ComputePipeline
    {
    public:
        virtual ~ComputePipeline() = default;
    };

} // namespace Echelon
