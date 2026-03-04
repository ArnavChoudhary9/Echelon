#include "OpenGLTexture.hpp"
#include "OpenGLUtils.hpp"

namespace Echelon {

    OpenGLTexture::OpenGLTexture(const TextureDesc& desc)
        : m_Width(desc.Width), m_Height(desc.Height), m_Depth(desc.Depth),
          m_MipLevels(desc.MipLevels), m_Type(desc.Type), m_Format(desc.Format)
    {
        switch (desc.Type) {
            case TextureType::TextureCube:  m_GLTarget = GL_TEXTURE_CUBE_MAP; break;
            case TextureType::Texture3D:    m_GLTarget = GL_TEXTURE_3D;       break;
            case TextureType::TextureArray: m_GLTarget = GL_TEXTURE_2D_ARRAY; break;
            default:                        m_GLTarget = GL_TEXTURE_2D;       break;
        }

        glGenTextures(1, &m_Handle);
        glBindTexture(m_GLTarget, m_Handle);

        GLenum internalFmt = OpenGLUtils::ToGLInternalFormat(desc.Format);

        if (m_GLTarget == GL_TEXTURE_2D) {
            glTexStorage2D(m_GLTarget, m_MipLevels, internalFmt, m_Width, m_Height);
        } else if (m_GLTarget == GL_TEXTURE_3D || m_GLTarget == GL_TEXTURE_2D_ARRAY) {
            glTexStorage3D(m_GLTarget, m_MipLevels, internalFmt, m_Width, m_Height, m_Depth);
        } else if (m_GLTarget == GL_TEXTURE_CUBE_MAP) {
            glTexStorage2D(m_GLTarget, m_MipLevels, internalFmt, m_Width, m_Height);
        }

        // Default sampling parameters
        glTexParameteri(m_GLTarget, GL_TEXTURE_MIN_FILTER, (m_MipLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(m_GLTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(m_GLTarget, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(m_GLTarget, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glBindTexture(m_GLTarget, 0);
    }

    OpenGLTexture::~OpenGLTexture()
    {
        if (m_Handle)
            glDeleteTextures(1, &m_Handle);
    }

    void OpenGLTexture::SetData(const void* data, uint64_t /*size*/,
                                uint32_t mipLevel, uint32_t /*arrayLayer*/)
    {
        GLenum format   = OpenGLUtils::ToGLFormat(m_Format);
        GLenum dataType = OpenGLUtils::ToGLDataType(m_Format);

        glBindTexture(m_GLTarget, m_Handle);

        uint32_t w = m_Width  >> mipLevel;
        uint32_t h = m_Height >> mipLevel;
        if (w == 0) w = 1;
        if (h == 0) h = 1;

        glTexSubImage2D(m_GLTarget, mipLevel, 0, 0, w, h, format, dataType, data);

        if (m_MipLevels > 1 && mipLevel == 0)
            glGenerateMipmap(m_GLTarget);

        glBindTexture(m_GLTarget, 0);
    }

} // namespace Echelon
