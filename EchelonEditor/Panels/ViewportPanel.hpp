#pragma once

/**
 * @file ViewportPanel.hpp
 * @brief The scene viewport: presents the offscreen render and handles click-picking.
 *
 * Uses a custom OnImGUIRender (zero window padding + a full-panel image) rather
 * than the default Panel window. Writes viewport focus/hover/size into the shared
 * EditorContext (the coordinator reads them next frame to drive the camera and
 * resize the render target). A left-click with the camera inactive reads the
 * object-id target and publishes an EntitySelectedEvent.
 */

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

#include <entt/entt.hpp>
#include <cstdint>

class ViewportPanel : public Panel {
public:
    explicit ViewportPanel(Ref<EditorContext> ctx)
        : Panel("Viewport"), m_Ctx(std::move(ctx)) {}

    void OnImGUIRender() override {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        m_Ctx->ViewportFocused = ImGui::IsWindowFocused();
        m_Ctx->ViewportHovered = ImGui::IsWindowHovered();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        m_Ctx->ViewportSize = { avail.x, avail.y };

        auto* renderer = Renderer::Get().GetActive();
        Ref<Texture> tex = renderer ? renderer->GetViewportTexture() : nullptr;
        if (tex && avail.x > 0.0f && avail.y > 0.0f) {
            // GL textures have their origin at the bottom-left, so flip V.
            ImGui::Image(static_cast<ImTextureID>(tex->GetNativeHandle()),
                         avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

            // Left-click without the camera active picks the entity under the cursor.
            if (renderer && !m_Ctx->CameraActive()
                && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 rmin = ImGui::GetItemRectMin();
                const ImVec2 rsz  = ImGui::GetItemRectSize();
                const float localX = ImGui::GetMousePos().x - rmin.x;
                const float localY = ImGui::GetMousePos().y - rmin.y;
                if (rsz.x > 0.0f && rsz.y > 0.0f &&
                    localX >= 0.0f && localY >= 0.0f && localX < rsz.x && localY < rsz.y) {
                    const uint32_t gx = static_cast<uint32_t>(localX);
                    // The id target shares the scene's bottom-left GL origin; the image
                    // is shown V-flipped (upright), so flip the click's Y to texel space.
                    const uint32_t gy = static_cast<uint32_t>(rsz.y - 1.0f - localY);
                    PickEntityAt(renderer, gx, gy);
                }
            }
        } else {
            ImGui::TextUnformatted("No viewport texture");
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    // Read the object-id pass's target at a texel and publish the entity there.
    void PickEntityAt(RendererAPI* renderer, uint32_t x, uint32_t y) {
        uint32_t raw = 0;
        if (!renderer->ReadTargetPixel(kEditorIdResource, x, y, &raw, sizeof(raw)))
            return;
        // EntityID.slang writes (id + 1); the target is cleared to 0 = "nothing".
        const entt::entity picked = (raw == 0u) ? entt::null
                                                : static_cast<entt::entity>(raw - 1u);
        PublishEvent(EntitySelectedEvent{ picked });
    }

    Ref<EditorContext> m_Ctx;
};
