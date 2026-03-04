#include "OpenGLFramebuffer.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLUtils.hpp"
#include "Echelon/Core/Log.hpp"

namespace Echelon {

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferDesc& desc)
        : m_Width(desc.Width), m_Height(desc.Height), m_Desc(desc)
    {
        Invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer()
    {
        Cleanup();
    }

    Ref<Texture> OpenGLFramebuffer::GetColorAttachment(uint32_t index) const
    {
        if (index < m_ColorAttachments.size())
            return m_ColorAttachments[index];
        return nullptr;
    }

    Ref<Texture> OpenGLFramebuffer::GetDepthAttachment() const
    {
        return m_DepthAttachment;
    }

    void OpenGLFramebuffer::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0) return;
        m_Width  = width;
        m_Height = height;
        Invalidate();
    }

    void OpenGLFramebuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0, m_Width, m_Height);
    }

    void OpenGLFramebuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Cleanup()
    {
        if (m_FBO) {
            glDeleteFramebuffers(1, &m_FBO);
            m_FBO = 0;
        }
        m_ColorAttachments.clear();
        m_DepthAttachment = nullptr;
    }

    void OpenGLFramebuffer::Invalidate()
    {
        Cleanup();

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // Color attachments
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_Desc.ColorAttachments.size()); ++i) {
            const auto& att = m_Desc.ColorAttachments[i];

            Ref<OpenGLTexture> tex;
            if (att.ExistingTexture) {
                tex = std::static_pointer_cast<OpenGLTexture>(att.ExistingTexture);
            } else {
                TextureDesc td;
                td.Type   = TextureType::RenderTarget;
                td.Format = att.Format;
                td.Usage  = TextureUsage::RenderTarget | TextureUsage::Sampled;
                td.Width  = m_Width;
                td.Height = m_Height;
                tex = CreateRef<OpenGLTexture>(td);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                   GL_TEXTURE_2D, tex->GetHandle(), 0);
            m_ColorAttachments.push_back(tex);
        }

        // Depth attachment
        if (m_Desc.HasDepthAttachment) {
            const auto& att = m_Desc.DepthAttachment;

            Ref<OpenGLTexture> tex;
            if (att.ExistingTexture) {
                tex = std::static_pointer_cast<OpenGLTexture>(att.ExistingTexture);
            } else {
                TextureDesc td;
                td.Type   = TextureType::Depth;
                td.Format = att.Format;
                td.Usage  = TextureUsage::DepthStencil;
                td.Width  = m_Width;
                td.Height = m_Height;
                tex = CreateRef<OpenGLTexture>(td);
            }

            GLenum attachPoint = GL_DEPTH_ATTACHMENT;
            if (att.Format == TextureFormat::D24_UNORM_S8_UINT ||
                att.Format == TextureFormat::D32_FLOAT_S8_UINT) {
                attachPoint = GL_DEPTH_STENCIL_ATTACHMENT;
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, attachPoint,
                                   GL_TEXTURE_2D, tex->GetHandle(), 0);
            m_DepthAttachment = tex;
        }

        // Set draw buffers
        if (!m_ColorAttachments.empty()) {
            std::vector<GLenum> drawBuffers(m_ColorAttachments.size());
            for (uint32_t i = 0; i < drawBuffers.size(); ++i)
                drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
            glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        } else {
            glDrawBuffer(GL_NONE);
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            ECHELON_LOG_ERROR("OpenGL framebuffer incomplete: 0x{:X}", status);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

} // namespace Echelon
