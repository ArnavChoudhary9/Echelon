#pragma once

/**
 * @file Panel.hpp
 * @brief Base class for a dockable ImGui UI panel.
 *
 * A Panel IS an Overlay, so it lives on the application layer stack and is driven
 * by the standard OnUpdate / OnEvent / OnImGUI* phases (see Application::Run). The
 * default OnImGUIRender wraps a single ImGui window around OnDraw(); panels that
 * need custom window handling (zero padding, custom flags, a floating/no-titlebar
 * window, or no window at all) override OnImGUIRender directly.
 *
 * All Layer/Overlay hooks have empty defaults so a concrete panel overrides only
 * what it needs. Panels are expected to communicate through the core EventBus
 * rather than holding direct references to one another.
 *
 * NOTE: this header pulls in imgui.h, so include it only from UI-aware translation
 * units (it is intentionally NOT part of the Echelon.hpp umbrella).
 */

#include "Echelon/Layer/Overlay.hpp"

#include "imgui.h"

#include <string>
#include <utility>

namespace Echelon {

    class Panel : public Overlay {
    public:
        explicit Panel(std::string name, bool open = true)
            : m_Name(std::move(name)), m_Open(open) {}

        // ---- Layer defaults (override as needed) ----
        void OnAttach() override {}
        void OnDetach() override {}
        void OnUpdate(float /*deltaTime*/) override {}
        void OnEvent(Event& /*event*/) override {}

        // ---- Overlay ImGui phases ----
        void OnImGUIBegin() override {}
        void OnImGUIEnd()   override {}

        // Default: one window per panel, body drawn by OnDraw(). Always paired with
        // End() even when Begin() returns false (collapsed) — ImGui requires it.
        void OnImGUIRender() override {
            if (!m_Open) return;
            ImGui::Begin(m_Name.c_str(), m_Closable ? &m_Open : nullptr, m_WindowFlags);
            OnDraw();
            ImGui::End();
        }

        const std::string& GetName() const { return m_Name; }
        bool  IsOpen() const     { return m_Open; }
        void  SetOpen(bool open) { m_Open = open; }
        bool* OpenPtr()          { return &m_Open; }   // for View menu checkboxes

    protected:
        /** @brief Panel body — called between ImGui::Begin/End by the default render. */
        virtual void OnDraw() {}

        std::string      m_Name;
        bool             m_Open        = true;
        bool             m_Closable    = true;   // show the window's [x] close button
        ImGuiWindowFlags m_WindowFlags = 0;
    };

} // namespace Echelon
