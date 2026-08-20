#pragma once

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

#include <entt/entt.hpp>
#include <vector>

class HierarchyPanel : public Panel {
public:
    explicit HierarchyPanel(Ref<EditorContext> ctx)
        : Panel("Hierarchy"), m_Ctx(std::move(ctx)) {}

    void OnAttach() override {
        // Clear history on any play-state transition — handles are registry-specific.
        m_PlaySub = OnMessage<PlayStateChangedEvent>([this](const PlayStateChangedEvent&) {
            m_BackStack.clear();
            m_ForwardStack.clear();
        });
    }

    void OnDetach() override { m_PlaySub.Reset(); }

protected:
    void OnDraw() override {
        auto registry = m_Ctx->ActiveScene
            ? m_Ctx->ActiveScene->GetEntityRegistry().lock() : nullptr;
        if (!registry) return;

        DrawNavBar(*registry);
        ImGui::Separator();

        // Draw only root entities (no parent); children rendered recursively.
        auto view = registry->view<RelationshipComponent>();
        for (auto e : view)
            if (!view.get<RelationshipComponent>(e).Parent.has_value())
                DrawEntityNode(e, *registry);

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::IsAnyItemHovered())
            SelectEntity(entt::null);
    }

private:
    // ---- Nav bar -------------------------------------------------------
    void DrawNavBar(entt::registry& reg) {
        const bool canBack    = !m_BackStack.empty();
        const bool canForward = !m_ForwardStack.empty();

        if (!canBack) ImGui::BeginDisabled();
        if (ImGui::ArrowButton("##back", ImGuiDir_Left)) NavigateBack(reg);
        if (!canBack) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Back");

        ImGui::SameLine();

        if (!canForward) ImGui::BeginDisabled();
        if (ImGui::ArrowButton("##fwd", ImGuiDir_Right)) NavigateForward(reg);
        if (!canForward) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Forward");
    }

    // ---- Tree node -----------------------------------------------------
    void DrawEntityNode(entt::entity e, entt::registry& reg) {
        const auto* tag = reg.try_get<TagComponent>(e);
        const char* label = tag ? tag->Tag.c_str() : "Entity";
        const bool  selected    = (e == m_Ctx->Selection);
        const auto* rel         = reg.try_get<RelationshipComponent>(e);
        const bool  hasChildren = rel && !rel->Children.empty();

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (selected)     flags |= ImGuiTreeNodeFlags_Selected;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        ImGui::PushID(static_cast<int>(entt::to_integral(e)));
        const bool open = ImGui::TreeNodeEx(label, flags);
        ImGui::PopID();

        // IsItemToggledOpen() is true only when the arrow was clicked; skip select in that case.
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
            SelectEntity(e);

        if (hasChildren && open) {
            for (const UUID& childUUID : rel->Children) {
                entt::entity child = FindByUUID(reg, childUUID);
                if (child != entt::null)
                    DrawEntityNode(child, reg);
            }
            ImGui::TreePop();
        }
    }

    // ---- Selection + history -------------------------------------------
    void SelectEntity(entt::entity newSel) {
        if (!m_Navigating && newSel != m_Ctx->Selection)
            RecordHistory(m_Ctx->Selection);
        PublishEvent(EntitySelectedEvent{ newSel });
    }

    void RecordHistory(entt::entity prev) {
        if (prev == entt::null) return;
        m_BackStack.push_back(prev);
        m_ForwardStack.clear();
    }

    void NavigateBack(entt::registry& reg) {
        // Skip any handles that no longer exist.
        while (!m_BackStack.empty() && !reg.valid(m_BackStack.back()))
            m_BackStack.pop_back();
        if (m_BackStack.empty()) return;

        entt::entity target = m_BackStack.back();
        m_BackStack.pop_back();

        if (m_Ctx->Selection != entt::null && reg.valid(m_Ctx->Selection))
            m_ForwardStack.push_back(m_Ctx->Selection);

        m_Navigating = true;
        PublishEvent(EntitySelectedEvent{ target });
        m_Navigating = false;
    }

    void NavigateForward(entt::registry& reg) {
        while (!m_ForwardStack.empty() && !reg.valid(m_ForwardStack.back()))
            m_ForwardStack.pop_back();
        if (m_ForwardStack.empty()) return;

        entt::entity target = m_ForwardStack.back();
        m_ForwardStack.pop_back();

        if (m_Ctx->Selection != entt::null && reg.valid(m_Ctx->Selection))
            m_BackStack.push_back(m_Ctx->Selection);

        m_Navigating = true;
        PublishEvent(EntitySelectedEvent{ target });
        m_Navigating = false;
    }

    // Linear scan — acceptable for editor-scale scenes.
    static entt::entity FindByUUID(entt::registry& reg, const UUID& uuid) {
        auto view = reg.view<IDComponent>();
        for (auto e : view)
            if (view.get<IDComponent>(e).ID == uuid)
                return e;
        return entt::null;
    }

    Ref<EditorContext>        m_Ctx;
    std::vector<entt::entity> m_BackStack;
    std::vector<entt::entity> m_ForwardStack;
    bool                      m_Navigating = false;
    ScopedSubscription        m_PlaySub;
};
