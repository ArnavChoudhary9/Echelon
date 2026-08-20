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
        uint32_t     GetSamples() const override { return m_Samples; }
        void         Resize(uint32_t width, uint32_t height) override;
        bool         ReadPixel(uint32_t attachmentIndex, int32_t x, int32_t y,
                               void* out, uint32_t outSize) override;

        /** @brief Resolve the multisample render FBO into the single-sample resolve textures. */
        void         Resolve() override;

        GLuint GetHandle() const { return m_FBO; }

        /** @brief Bind this FBO as the current render target (the multisample FBO when MSAA). */
        void Bind() const;

        /** @brief Unbind (restore default framebuffer). */
        static void Unbind();

    private:
        void Invalidate();
        void Cleanup();

        GLuint   m_FBO    = 0;   ///< render target FBO (multisample when m_Samples > 1)
        uint32_t m_Width  = 0;
        uint32_t m_Height = 0;
        uint32_t m_Samples = 1;

        FramebufferDesc m_Desc;

        // Single-sample sampleable textures returned by GetColorAttachment / GetDepthAttachment.
        // When MSAA, these back the resolve FBO; the multisample render targets are renderbuffers.
        std::vector<Ref<OpenGLTexture>> m_ColorAttachments;
        Ref<OpenGLTexture>              m_DepthAttachment;

        // MSAA-only (m_Samples > 1): multisample render FBO uses renderbuffers; m_ResolveFBO holds
        // the single-sample color textures above, populated by Resolve() via glBlitFramebuffer.
        GLuint               m_ResolveFBO = 0;
        std::vector<GLuint>  m_ColorRBOs;
        GLuint               m_DepthRBO   = 0;
    };

} // namespace Echelon
