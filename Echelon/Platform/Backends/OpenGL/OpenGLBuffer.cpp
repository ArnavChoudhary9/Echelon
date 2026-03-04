#include "OpenGLBuffer.hpp"

namespace Echelon {

    // Determine appropriate GL target from BufferUsage flags
    static GLenum DetermineTarget(BufferUsage usage)
    {
        if (HasFlag(usage, BufferUsage::IndexBuffer))    return GL_ELEMENT_ARRAY_BUFFER;
        if (HasFlag(usage, BufferUsage::UniformBuffer))  return GL_UNIFORM_BUFFER;
        if (HasFlag(usage, BufferUsage::StorageBuffer))  return GL_SHADER_STORAGE_BUFFER;
        if (HasFlag(usage, BufferUsage::IndirectBuffer)) return GL_DRAW_INDIRECT_BUFFER;
        return GL_ARRAY_BUFFER; // VertexBuffer or default
    }

    // GL usage hint from MemoryUsage
    static GLenum DetermineGLUsage(MemoryUsage mem)
    {
        switch (mem) {
            case MemoryUsage::CPUToGPU: return GL_DYNAMIC_DRAW;
            case MemoryUsage::GPUToCPU: return GL_STREAM_READ;
            default:                    return GL_STATIC_DRAW;
        }
    }

    OpenGLBuffer::OpenGLBuffer(const BufferDesc& desc)
        : m_Size(desc.Size), m_Usage(desc.Usage), m_Memory(desc.Memory)
    {
        m_Target = DetermineTarget(desc.Usage);

        glGenBuffers(1, &m_Handle);
        glBindBuffer(m_Target, m_Handle);
        glBufferData(m_Target,
                     static_cast<GLsizeiptr>(desc.Size),
                     desc.InitialData,
                     DetermineGLUsage(desc.Memory));
        glBindBuffer(m_Target, 0);
    }

    OpenGLBuffer::~OpenGLBuffer()
    {
        if (m_Handle)
            glDeleteBuffers(1, &m_Handle);
    }

    void* OpenGLBuffer::Map()
    {
        glBindBuffer(m_Target, m_Handle);
        GLenum access = (m_Memory == MemoryUsage::GPUToCPU) ? GL_READ_ONLY : GL_WRITE_ONLY;
        void* ptr = glMapBuffer(m_Target, access);
        return ptr;
    }

    void OpenGLBuffer::Unmap()
    {
        glUnmapBuffer(m_Target);
        glBindBuffer(m_Target, 0);
    }

    void OpenGLBuffer::SetData(const void* data, uint64_t size, uint64_t offset)
    {
        glBindBuffer(m_Target, m_Handle);
        glBufferSubData(m_Target,
                        static_cast<GLintptr>(offset),
                        static_cast<GLsizeiptr>(size),
                        data);
        glBindBuffer(m_Target, 0);
    }

} // namespace Echelon
