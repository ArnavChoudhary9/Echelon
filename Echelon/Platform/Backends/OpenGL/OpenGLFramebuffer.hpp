#pragma once

/**
 * @file OpenGLFramebuffer.hpp
 * @brief OpenGL implementation of the Framebuffer interface.
 */

#include "Echelon/GraphicsAPI/Framebuffer.hpp"
#include "Echelon/Core/Base.hpp"

#include <glad/gl.h>
#include <vector>

namespace Echelon {

    class OpenGLTexture;

    class OpenGLFramebuffer : public Framebuffer {
    public:
        OpenGLFramebuffer(const FramebufferDesc& desc);
        ~OpenGLFramebuffer() override;

        uint32_t     GetWidth() const override  { return m_Width; }
        uint32_t     GetHeight() const override { return m_Height; }
        uint32_t     GetColorAttachmentCount() const override { return static_cast<uint32_t>(m_ColorAttachments.size()); }
        Ref<Texture> GetColorAttachment(uint32_t index) const override;
        Ref<Texture> GetDepthAttachment() const override;
        bool         HasDepthAttachment() const override { return m_DepthAttachment != nullptr; }
        void         Resize(uint32_t width, uint32_t height) override;

        GLuint GetHandle() const { return m_FBO; }

        /** @brief Bind this FBO as the current render target. */
        void Bind() const;

        /** @brief Unbind (restore default framebuffer). */
        static void Unbind();

    private:
        void Invalidate();
        void Cleanup();

        GLuint   m_FBO    = 0;
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;

        FramebufferDesc m_Desc;

        std::vector<Ref<OpenGLTexture>> m_ColorAttachments;
        Ref<OpenGLTexture>              m_DepthAttachment;
    };

} // namespace Echelon
