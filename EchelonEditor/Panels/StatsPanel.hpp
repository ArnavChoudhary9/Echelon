#pragma once

/**
 * @file StatsPanel.hpp
 * @brief Frame stats, mode readout, and a text play/stop control.
 *
 * The play/stop button publishes an EditorCommandEvent (same channel the toolbar
 * uses) rather than toggling play state directly.
 */

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

class StatsPanel : public Panel {
public:
    explicit StatsPanel(Ref<EditorContext> ctx)
        : Panel("Stats"), m_Ctx(std::move(ctx)) {}

protected:
    void OnDraw() override {
        const float dt = m_Ctx->LastDeltaTime;
        ImGui::Text("%.1f FPS (%.2f ms)", dt > 0.0f ? 1.0f / dt : 0.0f, dt * 1000.0f);
        if (auto* renderer = Renderer::Get().GetActive()) {
            const RenderStats s = renderer->GetStats();
            ImGui::Text("Draw calls: %u", s.DrawCalls);
        }
        ImGui::Separator();

        if (ImGui::Button(m_Ctx->IsPlaying ? "Stop" : "Play"))
            PublishEvent(EditorCommandEvent{ m_Ctx->IsPlaying ? EditorAction::Stop
                                                              : EditorAction::Play });
        ImGui::SameLine();
        ImGui::Text("Mode: %s", m_Ctx->IsPlaying
                        ? (m_Ctx->IsPaused ? "Play (paused)" : "Play (scene copy)")
                        : "Edit");

        ImGui::Checkbox("Free Camera (no Alt)", &m_Ctx->FreeCam);
        ImGui::Text("Viewport: %.0f x %.0f", m_Ctx->ViewportSize.x, m_Ctx->ViewportSize.y);
        ImGui::Text("Cam: %.1f, %.1f, %.1f", m_Ctx->EditorPos.x, m_Ctx->EditorPos.y, m_Ctx->EditorPos.z);
        ImGui::Separator();
        ImGui::TextWrapped("Camera: hold Left Alt (or toggle Free Camera with `). "
                           "LMB look, MMB pan, scroll dolly, WASD/QE fly. "
                           "Click a mesh (camera inactive) to select it. P toggles play.");
    }

private:
    Ref<EditorContext> m_Ctx;
};
