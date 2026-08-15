#pragma once

// OpenGLSampler is defined in this header because it is a small companion to
// OpenGLTexture: rather than a GL sampler object, it retains the requested
// SamplerDesc and applies it as per-texture glTexParameter* state at bind time.

/**
 * @file OpenGLTexture.hpp
 * @brief OpenGL implementation of the Texture interface.
 */

#include "Echelon/GraphicsAPI/Texture.hpp"

#include <glad/gl.h>

namespace Echelon {

    /**
     * @brief OpenGL sampler — retains a SamplerDesc and applies it as per-texture state.
     *
     * OpenGL predating GL_ARB_sampler_objects (and the simplest portable path) drives
     * filtering/addressing through glTexParameter* on the bound texture rather than a
     * standalone sampler object. This class stores the requested state and applies it
     * to whatever texture is bound at draw time (see Apply). A Vulkan backend would
     * instead wrap a real VkSampler. Applying at bind time (not texture-creation time)
     * lets the same texture be sampled through different samplers.
     */
    class OpenGLSampler : public Sampler {
    public:
        explicit OpenGLSampler(const SamplerDesc& desc) : m_Desc(desc) {}
        ~OpenGLSampler() override = default;

        const SamplerDesc& GetDesc() const { return m_Desc; }

        /**
         * @brief Apply this sampler's state to the currently-bound texture.
         * @param target GL texture target the texture is bound to (e.g. GL_TEXTURE_2D).
         * @param hasMips Whether the texture has more than one mip level (selects the
         *               mip-aware min filter). Textures without mips must not use a
         *               *_MIPMAP_* min filter or they render as incomplete (black).
         */
        void Apply(GLenum target, bool hasMips) const;

    private:
        SamplerDesc m_Desc;
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
