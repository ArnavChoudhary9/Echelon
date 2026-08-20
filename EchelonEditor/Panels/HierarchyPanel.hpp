#pragma once

/**
 * @file HierarchyPanel.hpp
 * @brief Scene hierarchy: lists entities and publishes selection changes.
 *
 * Selection is not stored here — clicking an entity publishes an
 * EntitySelectedEvent on the bus; the EditorLayer coordinator writes the result
 * into EditorContext::Selection, which this panel reads back to highlight the row.
 */

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

#include <entt/entt.hpp>

class HierarchyPanel : public Panel {
public:
    explicit HierarchyPanel(Ref<EditorContext> ctx)
        : Panel("Hierarchy"), m_Ctx(std::move(ctx)) {}

protected:
    void OnDraw() override {
        auto registry = m_Ctx->ActiveScene ? m_Ctx->ActiveScene->GetEntityRegistry().lock() : nullptr;
        if (!registry) return;

        auto view = registry->view<TagComponent>();
        for (auto entity : view) {
            const auto& tag = view.get<TagComponent>(entity);
            const bool selected = (entity == m_Ctx->Selection);
            ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
            if (ImGui::Selectable(tag.Tag.c_str(), selected))
                PublishEvent(EntitySelectedEvent{ entity });
            ImGui::PopID();
        }

        // Click empty space to deselect.
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::IsAnyItemHovered())
            PublishEvent(EntitySelectedEvent{ entt::null });
    }

private:
    Ref<EditorContext> m_Ctx;
};
