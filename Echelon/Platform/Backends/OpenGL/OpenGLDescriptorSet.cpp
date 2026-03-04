#include "OpenGLDescriptorSet.hpp"
#include "OpenGLBuffer.hpp"
#include "OpenGLTexture.hpp"

namespace Echelon {

    // ================================================================
    // Layout
    // ================================================================

    OpenGLDescriptorSetLayout::OpenGLDescriptorSetLayout(const DescriptorSetLayoutDesc& desc)
        : m_Bindings(desc.Bindings)
    {
    }

    // ================================================================
    // Descriptor Set
    // ================================================================

    OpenGLDescriptorSet::OpenGLDescriptorSet(const Ref<OpenGLDescriptorSetLayout>& layout)
        : m_Layout(layout)
    {
    }

    void OpenGLDescriptorSet::SetBuffer(uint32_t binding, const Ref<Buffer>& buffer,
                                         uint64_t offset, uint64_t range)
    {
        m_Buffers[binding] = { buffer, offset, range };
    }

    void OpenGLDescriptorSet::SetTexture(uint32_t binding, const Ref<Texture>& texture)
    {
        m_Textures[binding] = texture;
    }

    void OpenGLDescriptorSet::Update()
    {
        // OpenGL applies bindings at Bind() time — nothing to batch here
    }

    void OpenGLDescriptorSet::Bind(uint32_t /*setIndex*/) const
    {
        // Bind uniform / storage buffers
        for (const auto& [binding, bb] : m_Buffers) {
            auto glBuf = std::static_pointer_cast<OpenGLBuffer>(bb.buffer);
            if (!glBuf) continue;

            GLenum target = glBuf->GetTarget();
            uint64_t range = bb.range ? bb.range : glBuf->GetSize();

            if (target == GL_UNIFORM_BUFFER || target == GL_SHADER_STORAGE_BUFFER) {
                glBindBufferRange(target, binding,
                                  glBuf->GetHandle(),
                                  static_cast<GLintptr>(bb.offset),
                                  static_cast<GLsizeiptr>(range));
            }
        }

        // Bind textures to texture units
        for (const auto& [binding, tex] : m_Textures) {
            auto glTex = std::static_pointer_cast<OpenGLTexture>(tex);
            if (!glTex) continue;

            glActiveTexture(GL_TEXTURE0 + binding);
            glBindTexture(glTex->GetGLTarget(), glTex->GetHandle());
        }
    }

} // namespace Echelon
