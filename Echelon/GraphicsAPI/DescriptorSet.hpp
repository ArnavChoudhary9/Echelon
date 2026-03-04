#pragma once

/**
 * @file DescriptorSet.hpp
 * @brief Descriptor set layout / descriptor set interfaces and all
 *        descriptor-related types.
 */

#include "Echelon/Core/Base.hpp"
#include "Echelon/GraphicsAPI/GraphicsTypes.hpp"   // ShaderStage

#include <cstdint>
#include <string>
#include <vector>

namespace Echelon {

    // Forward declarations
    class Buffer;
    class Texture;

    // ================================================================
    // Descriptor types
    // ================================================================

    /**
     * @brief Type of resource bound in a descriptor set.
     */
    enum class DescriptorType : uint8_t
    {
        UniformBuffer = 0,
        StorageBuffer,
        SampledTexture,
        StorageTexture,
        Sampler,
        CombinedImageSampler
    };

    /**
     * @brief A single binding within a descriptor set layout.
     */
    struct DescriptorBinding
    {
        uint32_t       Binding     = 0;
        DescriptorType Type        = DescriptorType::UniformBuffer;
        uint32_t       Count       = 1;
        ShaderStage    StageAccess = ShaderStage::Vertex;
    };

    /**
     * @brief Descriptor for creating a descriptor set layout.
     */
    struct DescriptorSetLayoutDesc
    {
        std::vector<DescriptorBinding> Bindings;
        std::string                    DebugName = "";
    };

    // ================================================================
    // Descriptor set layout interface
    // ================================================================

    /**
     * @brief Abstract descriptor set layout.
     *
     * Defines the shape of a descriptor set — the binding indices, resource
     * types and shader stages that the set exposes.  Layouts are used when
     * creating both pipelines and descriptor sets.
     */
    class DescriptorSetLayout
    {
    public:
        virtual ~DescriptorSetLayout() = default;

        /**
         * @brief Get the number of bindings in this layout.
         * @return uint32_t Binding count.
         */
        virtual uint32_t GetBindingCount() const = 0;
    };

    // ================================================================
    // Descriptor set interface
    // ================================================================

    /**
     * @brief Abstract descriptor set — a group of resource bindings.
     *
     * Descriptor sets map GPU resources (buffers, textures, samplers) to
     * shader binding points.  They are bound via CommandBuffer before draw
     * or dispatch calls.
     *
     * Example binding layout:
     * @code
     *     0 -> uniform buffer  (per-frame camera data)
     *     1 -> sampled texture (albedo)
     *     2 -> sampler
     *     3 -> storage buffer  (SSBO for compute)
     * @endcode
     */
    class DescriptorSet
    {
    public:
        virtual ~DescriptorSet() = default;

        /**
         * @brief Bind a buffer to a descriptor binding point.
         * @param binding The binding index within the set.
         * @param buffer  The buffer to bind.
         * @param offset  Byte offset into the buffer.
         * @param range   Number of bytes to expose (0 = entire buffer).
         */
        virtual void SetBuffer(uint32_t binding, const Ref<Buffer>& buffer,
                               uint64_t offset = 0, uint64_t range = 0) = 0;

        /**
         * @brief Bind a texture to a descriptor binding point.
         * @param binding The binding index within the set.
         * @param texture The texture to bind.
         */
        virtual void SetTexture(uint32_t binding, const Ref<Texture>& texture) = 0;

        /**
         * @brief Flush / update the descriptor set after changing bindings.
         *
         * Some backends batch descriptor writes and require an explicit update
         * call.  On others this may be a no-op.
         */
        virtual void Update() = 0;
    };

} // namespace Echelon
