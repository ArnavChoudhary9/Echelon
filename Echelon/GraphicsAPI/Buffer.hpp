#pragma once

/**
 * @file Buffer.hpp
 * @brief GPU buffer interface and all buffer-related types.
 */

#include "Echelon/Core/Base.hpp"

#include <cstdint>
#include <string>

namespace Echelon {

    // ================================================================
    // Buffer types
    // ================================================================

    /**
     * @brief How a buffer will be used by the GPU.  Flags can be combined.
     */
    enum class BufferUsage : uint32_t {
        VertexBuffer   = 1 << 0,
        IndexBuffer    = 1 << 1,
        UniformBuffer  = 1 << 2,
        StorageBuffer  = 1 << 3,
        IndirectBuffer = 1 << 4,
        TransferSrc    = 1 << 5,
        TransferDst    = 1 << 6
    };

    inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline BufferUsage operator&(BufferUsage a, BufferUsage b) {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }
    inline bool HasFlag(BufferUsage value, BufferUsage flag) {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    /**
     * @brief Memory access pattern hint for resource allocation.
     */
    enum class MemoryUsage : uint8_t {
        GPUOnly = 0,   ///< GPU-only memory, fastest access
        CPUToGPU,      ///< Written by CPU, read by GPU (upload)
        GPUToCPU       ///< Written by GPU, read by CPU (readback)
    };

    /**
     * @brief Descriptor for creating a buffer.
     */
    struct BufferDesc {
        uint64_t    Size        = 0;
        BufferUsage Usage       = BufferUsage::VertexBuffer;
        MemoryUsage Memory      = MemoryUsage::GPUOnly;
        const void* InitialData = nullptr;
        std::string DebugName   = "";
    };

    // ================================================================
    // Buffer interface
    // ================================================================

    /**
     * @brief Abstract GPU buffer resource.
     *
     * Represents any linear memory allocation on the GPU: vertex data,
     * index data, uniform/constant parameters, storage buffers, etc.
     * Concrete backends implement the actual allocation strategy.
     */
    class Buffer {
    public:
        virtual ~Buffer() = default;

        /**
         * @brief Map the buffer to CPU-accessible memory.
         *
         * Valid only for buffers created with CPUToGPU or GPUToCPU memory usage.
         * @return void* Pointer to the mapped memory, or nullptr on failure.
         */
        virtual void* Map() = 0;

        /**
         * @brief Unmap previously mapped buffer memory.
         */
        virtual void Unmap() = 0;

        /**
         * @brief Upload data to the buffer.
         *
         * @param data   Pointer to source data.
         * @param size   Size of data in bytes.
         * @param offset Byte offset into the buffer.
         */
        virtual void SetData(const void* data, uint64_t size, uint64_t offset = 0) = 0;

        /**
         * @brief Get the size of the buffer in bytes.
         * @return uint64_t Size in bytes.
         */
        virtual uint64_t GetSize() const = 0;

        /**
         * @brief Get the usage flags of this buffer.
         * @return BufferUsage
         */
        virtual BufferUsage GetUsage() const = 0;
    };

} // namespace Echelon
