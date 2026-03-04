#pragma once

/**
 * @file OpenGLRenderPass.hpp
 * @brief OpenGL implementation of the RenderPass interface.
 *
 * OpenGL has no first-class render pass object. This stores the attachment
 * descriptions and clear values so the CommandBuffer can apply them
 * when BeginRenderPass is called.
 */

#include "Echelon/GraphicsAPI/RenderPass.hpp"

namespace Echelon {

    class OpenGLRenderPass : public RenderPass {
    public:
        OpenGLRenderPass(const RenderPassDesc& desc);
        ~OpenGLRenderPass() override = default;

        uint32_t           GetColorAttachmentCount() const override { return static_cast<uint32_t>(m_Desc.ColorAttachments.size()); }
        bool               HasDepthAttachment() const override      { return m_Desc.HasDepthAttachment; }
        const std::string& GetDebugName() const override            { return m_Desc.DebugName; }

        const RenderPassDesc& GetDesc() const { return m_Desc; }

    private:
        RenderPassDesc m_Desc;
    };

} // namespace Echelon
