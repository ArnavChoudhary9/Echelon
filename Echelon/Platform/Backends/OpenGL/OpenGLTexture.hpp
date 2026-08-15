#pragma once

// OpenGLSampler is defined in this header because it is a trivial no-op companion
// to OpenGLTexture: GL uses per-texture glTexParameter* state, not sampler objects.

/**
 * @file OpenGLTexture.hpp
 * @brief OpenGL implementation of the Texture interface.
 */

#include "Echelon/GraphicsAPI/Texture.hpp"

#include <glad/gl.h>

namespace Echelon {

    /**
     * @brief OpenGL sampler — a no-op placeholder.
     *
     * OpenGL uses per-texture glTexParameter* state set during OpenGLTexture
     * construction.  No GL sampler object is created or bound.  A Vulkan backend
     * would replace this with a real VkSampler.
     */
    class OpenGLSampler : public Sampler {
    public:
        explicit OpenGLSampler(const SamplerDesc& /*desc*/) {}
        ~OpenGLSampler() override = default;
    };

    class OpenGLTexture : public Texture {
    public:
        OpenGLTexture(const TextureDesc& desc);
        ~OpenGLTexture() override;

        uint32_t      GetWidth() const override     { return m_Width; }
        uint32_t      GetHeight() const override    { return m_Height; }
        uint32_t      GetDepth() const override     { return m_Depth; }
        uint32_t      GetMipLevels() const override { return m_MipLevels; }
        TextureType   GetType() const override      { return m_Type; }
        TextureFormat GetFormat() const override     { return m_Format; }

        void SetData(const void* data, uint64_t size,
                     uint32_t mipLevel = 0, uint32_t arrayLayer = 0) override;

        GLuint GetHandle() const { return m_Handle; }
        GLenum GetGLTarget() const { return m_GLTarget; }

    private:
        GLuint        m_Handle    = 0;
        GLenum        m_GLTarget  = GL_TEXTURE_2D;
        uint32_t      m_Width     = 1;
        uint32_t      m_Height    = 1;
        uint32_t      m_Depth     = 1;
        uint32_t      m_MipLevels = 1;
        TextureType   m_Type      = TextureType::Texture2D;
        TextureFormat m_Format    = TextureFormat::RGBA8_UNORM;
    };

} // namespace Echelon
