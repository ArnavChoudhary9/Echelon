#include "OpenGLFramebuffer.hpp"
#include "OpenGLTexture.hpp"
#include "OpenGLUtils.hpp"
#include "Echelon/Core/Log.hpp"

#include <algorithm>

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
        if (m_FBO)        { glDeleteFramebuffers(1, &m_FBO);        m_FBO = 0; }
        if (m_ResolveFBO) { glDeleteFramebuffers(1, &m_ResolveFBO); m_ResolveFBO = 0; }
        if (!m_ColorRBOs.empty()) {
            glDeleteRenderbuffers(static_cast<GLsizei>(m_ColorRBOs.size()), m_ColorRBOs.data());
            m_ColorRBOs.clear();
        }
        if (m_DepthRBO)   { glDeleteRenderbuffers(1, &m_DepthRBO);  m_DepthRBO = 0; }
        m_ColorAttachments.clear();
        m_DepthAttachment = nullptr;
    }

    static void SetDrawBuffers(uint32_t colorCount)
    {
        if (colorCount > 0) {
            std::vector<GLenum> bufs(colorCount);
            for (uint32_t i = 0; i < colorCount; ++i) bufs[i] = GL_COLOR_ATTACHMENT0 + i;
            glDrawBuffers(static_cast<GLsizei>(bufs.size()), bufs.data());
        } else {
            glDrawBuffer(GL_NONE);   // depth-only
        }
    }

    // Attach a texture to an FBO attachment point, deriving the GL attach call from the
    // texture's real target: a plain 2D texture, a single cubemap face, or one array layer.
    // Layer/Mip come from the FramebufferAttachment (0/0 for ordinary 2D targets), which is
    // how the renderer drives per-face cubemap rendering (shadows, IBL) and per-mip prefiltering.
    static void AttachFramebufferTexture(GLenum attachPoint, const Ref<OpenGLTexture>& tex,
                                         uint32_t layer, uint32_t mip)
    {
        const GLenum target = tex->GetGLTarget();
        if (target == GL_TEXTURE_CUBE_MAP) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachPoint,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer, tex->GetHandle(), mip);
        } else if (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_3D) {
            glFramebufferTextureLayer(GL_FRAMEBUFFER, attachPoint, tex->GetHandle(), mip, layer);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachPoint, GL_TEXTURE_2D, tex->GetHandle(), mip);
        }
    }

    void OpenGLFramebuffer::Invalidate()
    {
        Cleanup();

        m_Samples = std::max(1u, m_Desc.Samples);
        if (m_Samples > 1) {
            GLint maxSamples = 1;
            glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
            m_Samples = std::min(m_Samples, static_cast<uint32_t>(std::max(1, maxSamples)));
        }
        const bool msaa = m_Samples > 1;

        // ---- Single-sample FBO: sampleable color textures (+ depth texture when single-sample).
        // In MSAA mode this becomes the RESOLVE target; GetColorAttachment always returns these. ----
        GLuint texFBO = 0;
        glGenFramebuffers(1, &texFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, texFBO);

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

            AttachFramebufferTexture(GL_COLOR_ATTACHMENT0 + i, tex, att.Layer, att.MipLevel);
            m_ColorAttachments.push_back(tex);
        }

        // Depth as a sampleable texture only when single-sample (MSAA depth is a renderbuffer below).
        if (m_Desc.HasDepthAttachment && !msaa) {
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

            AttachFramebufferTexture(attachPoint, tex, att.Layer, att.MipLevel);
            m_DepthAttachment = tex;
        }

        SetDrawBuffers(static_cast<uint32_t>(m_ColorAttachments.size()));

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            ECHELON_LOG_ERROR("OpenGL {} framebuffer incomplete: 0x{:X}",
                              msaa ? "resolve" : "single-sample", status);

        if (!msaa) {
            m_FBO = texFBO;               // the sole render+sample target
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }

        // ---- MSAA render FBO: multisample renderbuffers (color + depth). ----
        m_ResolveFBO = texFBO;
        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        for (uint32_t i = 0; i < static_cast<uint32_t>(m_Desc.ColorAttachments.size()); ++i) {
            GLuint rbo = 0;
            glGenRenderbuffers(1, &rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_Samples,
                OpenGLUtils::ToGLInternalFormat(m_Desc.ColorAttachments[i].Format), m_Width, m_Height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, rbo);
            m_ColorRBOs.push_back(rbo);
        }

        if (m_Desc.HasDepthAttachment) {
            const auto& att = m_Desc.DepthAttachment;
            GLenum attachPoint = GL_DEPTH_ATTACHMENT;
            if (att.Format == TextureFormat::D24_UNORM_S8_UINT ||
                att.Format == TextureFormat::D32_FLOAT_S8_UINT) {
                attachPoint = GL_DEPTH_STENCIL_ATTACHMENT;
            }
            glGenRenderbuffers(1, &m_DepthRBO);
            glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRBO);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_Samples,
                OpenGLUtils::ToGLInternalFormat(att.Format), m_Width, m_Height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachPoint, GL_RENDERBUFFER, m_DepthRBO);
        }

        SetDrawBuffers(static_cast<uint32_t>(m_ColorAttachments.size()));

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            ECHELON_LOG_ERROR("OpenGL MSAA framebuffer incomplete: 0x{:X}", status);

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Resolve()
    {
        if (m_Samples <= 1 || m_ResolveFBO == 0) return;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_ResolveFBO);
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_ColorAttachments.size()); ++i) {
            glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
            glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
            glBlitFramebuffer(0, 0, m_Width, m_Height, 0, 0, m_Width, m_Height,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

} // namespace Echelon
