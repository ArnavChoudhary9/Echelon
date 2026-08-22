#pragma once

/**
 * @file InspectorPanel.hpp
 * @brief Inspector: edits the components of the currently selected entity.
 *
 * Reads EditorContext::Selection (kept in sync by the coordinator from bus
 * EntitySelectedEvents) and edits the selected entity's components in place.
 */

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

#include <entt/entt.hpp>
#include <cstring>

class InspectorPanel : public Panel {
public:
    explicit InspectorPanel(Ref<EditorContext> ctx)
        : Panel("Inspector"), m_Ctx(std::move(ctx)) {
        m_Closable = false;   // essential panel — no close [x]
    }

protected:
    void OnDraw() override {
        auto registry = m_Ctx->ActiveScene ? m_Ctx->ActiveScene->GetEntityRegistry().lock() : nullptr;
        const entt::entity sel = m_Ctx->Selection;

        if (!registry || sel == entt::null || !registry->valid(sel)) {
            ImGui::TextUnformatted("No entity selected");
            return;
        }

        if (auto* tag = registry->try_get<TagComponent>(sel)) {
            char buffer[256];
            std::memset(buffer, 0, sizeof(buffer));
            std::strncpy(buffer, tag->Tag.c_str(), sizeof(buffer) - 1);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
                tag->Tag = buffer;
        }
        ImGui::Separator();

        if (auto* t = registry->try_get<TransformComponent>(sel)) {
            ImGui::DragFloat3("Position", &t->Position.x, 0.05f);
            ImGui::DragFloat3("Rotation", &t->Rotation.x, 0.25f);
            ImGui::DragFloat3("Scale",    &t->Scale.x,    0.05f);
        }
        if (auto* l = registry->try_get<LightComponent>(sel)) {
            ImGui::Separator();
            ImGui::ColorEdit3("Light Color", &l->Color.x);
            ImGui::DragFloat("Intensity", &l->Intensity, 0.1f, 0.0f, 1000.0f);
        }
    }

private:
    Ref<EditorContext> m_Ctx;
};
