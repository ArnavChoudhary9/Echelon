#pragma once

/**
 * @file ToolbarPanel.hpp
 * @brief Play/Pause/Stop/Restart/Step transport toolbar.
 *
 * A normal dockable, resizable, movable window (drag its body to move/dock it).
 * Icon size scales with the UI font size. Buttons are enabled per transport state
 * and emit EditorCommandEvents on the bus — the toolbar knows nothing about how
 * play mode is implemented.
 */

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

class ToolbarPanel : public Panel {
public:
    explicit ToolbarPanel(Ref<EditorContext> ctx)
        : Panel("Toolbar"), m_Ctx(std::move(ctx)) {}

    void OnImGUIRender() override {
        const ImGuiViewport* vp  = ImGui::GetMainViewport();
        const ImGuiStyle&    sty = ImGui::GetStyle();

        const float winPad = 6.0f;
        const float gap    = 4.0f;
        const int   nBtns  = 5;
        const float menuH  = ImGui::GetFrameHeight();

        // Use a fixed initial width/height; buttons will fill whatever the user resizes to.
        const float initIconSz = ImGui::GetFontSize() * 1.15f;
        const float initBtnW   = initIconSz + sty.FramePadding.x * 2.0f;
        const float initRowW   = nBtns * initBtnW + (nBtns - 1) * gap;
        const float initW      = initRowW + winPad * 2.0f;
        const float initH      = initIconSz + sty.FramePadding.y * 2.0f + winPad * 2.0f + ImGui::GetFrameHeight();

        // First-use placement: centered below the menu bar. Dockable + resizable +
        // movable thereafter; position/size persist in imgui.ini.
        ImGui::SetNextWindowPos(
            ImVec2(vp->WorkPos.x + (vp->WorkSize.x - initW) * 0.5f, vp->WorkPos.y + menuH),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(initW, initH), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.85f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(winPad, winPad));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(gap, 0.0f));
        // Zero vertical frame padding so the image fills all available height.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(sty.FramePadding.x, 0.0f));
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar(3);

        // Derive icon size from actual available height so buttons fill the window.
        const ImVec2 avail  = ImGui::GetContentRegionAvail();
        const float  iconSz = avail.y;    // full vertical space = icon height
        const float  btnW   = iconSz + sty.FramePadding.x * 2.0f;
        const float  rowW   = nBtns * btnW + (nBtns - 1) * gap;

        // Center horizontally only.
        const float offX = (avail.x - rowW) * 0.5f;
        if (offX > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offX);

        // Dark (white) icons for a dark theme, light (black) for a light theme.
        const ImVec4& bg = sty.Colors[ImGuiCol_WindowBg];
        const bool isDark = (bg.x + bg.y + bg.z) < 1.5f;
        const EditorIcons& icons = isDark ? m_Ctx->DarkIcons : m_Ctx->LightIcons;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.f, 1.f, 1.f, 0.30f));

        const bool editMode = !m_Ctx->IsPlaying;
        const bool playing  =  m_Ctx->IsPlaying && !m_Ctx->IsPaused;
        const bool paused   =  m_Ctx->IsPlaying &&  m_Ctx->IsPaused;

        auto iconBtn = [&](const char* id, const Ref<TextureAsset>& icon,
                           bool enabled, const char* tip) -> bool {
            if (!enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.30f);
            bool clicked = false;
            if (icon && icon->IsValid()) {
                clicked = ImGui::ImageButton(id,
                    static_cast<ImTextureID>(icon->GetGpuTexture()->GetNativeHandle()),
                    ImVec2(iconSz, iconSz));
            } else {
                clicked = ImGui::Button(id, ImVec2(iconSz, iconSz));
            }
            if (!enabled) ImGui::PopStyleVar();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", tip);
            return clicked && enabled;
        };

        auto cmd = [](EditorAction a) { PublishEvent(EditorCommandEvent{ a }); };

        if (iconBtn("##play", icons.Play, editMode || paused, paused ? "Resume" : "Play"))
            cmd(paused ? EditorAction::Resume : EditorAction::Play);
        ImGui::SameLine();
        if (iconBtn("##pause", icons.Pause, playing, "Pause"))
            cmd(EditorAction::Pause);
        ImGui::SameLine();
        if (iconBtn("##stop", icons.Stop, m_Ctx->IsPlaying, "Stop"))
            cmd(EditorAction::Stop);
        ImGui::SameLine();
        if (iconBtn("##restart", icons.Restart, m_Ctx->IsPlaying, "Restart"))
            cmd(EditorAction::Restart);
        ImGui::SameLine();
        if (iconBtn("##step", icons.Step, paused, "Step Frame"))
            cmd(EditorAction::Step);

        ImGui::PopStyleColor(3);
        ImGui::End();
    }

private:
    Ref<EditorContext> m_Ctx;
};
