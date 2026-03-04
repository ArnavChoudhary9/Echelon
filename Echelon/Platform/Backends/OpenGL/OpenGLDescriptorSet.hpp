#pragma once

/**
 * @file OpenGLDescriptorSet.hpp
 * @brief OpenGL implementation of DescriptorSetLayout / DescriptorSet.
 *
 * OpenGL has no native descriptor set concept. These wrappers track
 * uniform buffer and texture bindings so they can be applied before draw calls.
 */

#include "Echelon/GraphicsAPI/DescriptorSet.hpp"
#include "Echelon/Core/Base.hpp"

#include <glad/gl.h>
#include <unordered_map>

namespace Echelon {

    class OpenGLDescriptorSetLayout : public DescriptorSetLayout {
    public:
        OpenGLDescriptorSetLayout(const DescriptorSetLayoutDesc& desc);
        ~OpenGLDescriptorSetLayout() override = default;

        uint32_t GetBindingCount() const override { return static_cast<uint32_t>(m_Bindings.size()); }
        const std::vector<DescriptorBinding>& GetBindings() const { return m_Bindings; }

    private:
        std::vector<DescriptorBinding> m_Bindings;
    };

    class OpenGLDescriptorSet : public DescriptorSet {
    public:
        OpenGLDescriptorSet(const Ref<OpenGLDescriptorSetLayout>& layout);
        ~OpenGLDescriptorSet() override = default;

        void SetBuffer(uint32_t binding, const Ref<Buffer>& buffer,
                       uint64_t offset = 0, uint64_t range = 0) override;
        void SetTexture(uint32_t binding, const Ref<Texture>& texture) override;
        void Update() override;

        /** @brief Apply all bindings to the current GL state. */
        void Bind(uint32_t setIndex) const;

    private:
        struct BufferBinding {
            Ref<Buffer> buffer;
            uint64_t    offset = 0;
            uint64_t    range  = 0;
        };

        Ref<OpenGLDescriptorSetLayout>              m_Layout;
        std::unordered_map<uint32_t, BufferBinding>  m_Buffers;
        std::unordered_map<uint32_t, Ref<Texture>>   m_Textures;
    };

} // namespace Echelon
