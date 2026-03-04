#pragma once

/**
 * @file OpenGLBuffer.hpp
 * @brief OpenGL implementation of the Buffer interface.
 */

#include "Echelon/GraphicsAPI/Buffer.hpp"

#include <glad/gl.h>

namespace Echelon {

    class OpenGLBuffer : public Buffer {
    public:
        OpenGLBuffer(const BufferDesc& desc);
        ~OpenGLBuffer() override;

        void*       Map() override;
        void        Unmap() override;
        void        SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
        uint64_t    GetSize() const override  { return m_Size; }
        BufferUsage GetUsage() const override { return m_Usage; }

        /** @brief Get the raw OpenGL buffer object name. */
        GLuint GetHandle() const { return m_Handle; }

        /** @brief GL target (GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, etc.). */
        GLenum GetTarget() const { return m_Target; }

    private:
        GLuint      m_Handle = 0;
        GLenum      m_Target = GL_ARRAY_BUFFER;
        uint64_t    m_Size   = 0;
        BufferUsage m_Usage  = BufferUsage::VertexBuffer;
        MemoryUsage m_Memory = MemoryUsage::GPUOnly;
    };

} // namespace Echelon
