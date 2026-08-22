#pragma once

/**
 * @file HierarchyPanel.hpp
 * @brief Scene hierarchy tree: selection, entity creation, and drag-drop reparenting.
 *
 * Renders the scene as a nested tree driven by each entity's RelationshipComponent.
 * Only root entities are iterated at the top level; children recurse. Structural
 * mutations (create / delete / reparent) are DEFERRED to the end of the frame via
 * m_Pending — mutating the registry mid-iteration (adding/destroying entities while
 * walking a view) is unsafe.
 *
 * Communicates only through EditorContext (state) + the EventBus (EntitySelectedEvent).
 */

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

#include <entt/entt.hpp>

class HierarchyPanel : public Panel {
public:
    explicit HierarchyPanel(Ref<EditorContext> ctx)
        : Panel("Hierarchy"), m_Ctx(std::move(ctx)) {
        m_Closable = false;   // essential panel — no close [x]
    }

protected:
    void OnDraw() override {
        auto scene    = m_Ctx->ActiveScene;
        auto registry = scene ? scene->GetEntityRegistry().lock() : nullptr;
        if (!registry) return;

        m_Pending = {};   // reset deferred ops each frame

        // ---- Header: create entity -------------------------------------
        if (ImGui::Button("+ Add Entity"))
            m_Pending.createEmptyRoot = true;
        ImGui::SameLine();
        ImGui::TextDisabled("drag a row onto another to re-parent");
        ImGui::Separator();

        // ---- Tree (scrollable) -----------------------------------------
        ImGui::BeginChild("##tree", ImVec2(0, 0), false);

        auto view = registry->view<RelationshipComponent>();
        for (auto e : view) {
            if (IsRoot(*registry, view.get<RelationshipComponent>(e)))
                DrawEntityNode(e, *registry);
        }

        // Remaining blank space: click to deselect, drop to unparent, right-click to create.
        ImVec2 avail = ImGui::GetContentRegionAvail();
        avail.y = avail.y > 1.0f ? avail.y : 1.0f;
        ImGui::InvisibleButton("##blank", avail);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            PublishEvent(EntitySelectedEvent{ entt::null });
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kEntityPayload)) {
                m_Pending.reparentChild = *static_cast<const entt::entity*>(p->Data);
                m_Pending.reparentTo    = entt::null;   // to root
                m_Pending.hasReparent   = true;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopupContextItem("##blankctx")) {
            if (ImGui::MenuItem("Create Empty Entity"))
                m_Pending.createEmptyRoot = true;
            ImGui::EndPopup();
        }

        ImGui::EndChild();

        ApplyPending(scene, *registry);
    }

private:
    static constexpr const char* kEntityPayload = "ECHELON_ENTITY";

    // Deferred structural ops, applied after the tree is fully drawn.
    struct Pending {
        bool         createEmptyRoot = false;
        entt::entity createChildOf   = entt::null;
        entt::entity deleteEntity    = entt::null;
        bool         hasReparent     = false;
        entt::entity reparentChild   = entt::null;
        entt::entity reparentTo      = entt::null;   // null = unparent to root
    };

    // ---- Tree node -----------------------------------------------------
    void DrawEntityNode(entt::entity e, entt::registry& reg) {
        const auto* tag   = reg.try_get<TagComponent>(e);
        const char* label = (tag && !tag->Tag.empty()) ? tag->Tag.c_str() : "Entity";
        const auto* rel   = reg.try_get<RelationshipComponent>(e);
        const bool  hasChildren = rel && HasAnyValidChild(reg, *rel);
        const bool  selected    = (e == m_Ctx->Selection);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                                 | ImGuiTreeNodeFlags_OpenOnDoubleClick
                                 | ImGuiTreeNodeFlags_SpanFullWidth;
        if (selected)     flags |= ImGuiTreeNodeFlags_Selected;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        const ImVec2 nodePos = ImGui::GetCursorScreenPos();

        ImGui::PushID(static_cast<int>(entt::to_integral(e)));
        const bool open = ImGui::TreeNodeEx(label, flags);

        // Select on click, but not when the click was on the arrow (toggle).
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
            PublishEvent(EntitySelectedEvent{ e });

        // Drag source: carry the entity handle (valid this frame, same registry).
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload(kEntityPayload, &e, sizeof(entt::entity));
            ImGui::Text("%s", label);
            ImGui::EndDragDropSource();
        }
        // Drop target: reparent the dragged entity under this one.
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kEntityPayload)) {
                m_Pending.reparentChild = *static_cast<const entt::entity*>(p->Data);
                m_Pending.reparentTo    = e;
                m_Pending.hasReparent   = true;
            }
            ImGui::EndDragDropTarget();
        }

        // Per-node context menu.
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Create Empty Child"))
                m_Pending.createChildOf = e;
            if (rel && rel->Parent.has_value() && ImGui::MenuItem("Unparent")) {
                m_Pending.reparentChild = e;
                m_Pending.reparentTo    = entt::null;
                m_Pending.hasReparent   = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Create Empty Entity"))
                m_Pending.createEmptyRoot = true;
            if (ImGui::MenuItem("Delete"))
                m_Pending.deleteEntity = e;
            ImGui::EndPopup();
        }

        if (hasChildren && open) {
            // Hierarchy guide lines: a vertical spine under the parent, with a
            // horizontal tick out to each child row's vertical center.
            ImDrawList* dl       = ImGui::GetWindowDrawList();
            const ImU32  lineCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            const float  lineX   = nodePos.x + ImGui::GetTreeNodeToLabelSpacing() * 0.5f;
            const float  halfRow = ImGui::GetTextLineHeight() * 0.5f;
            const float  topY    = ImGui::GetCursorScreenPos().y;
            float        lastMidY = topY;

            for (const UUID& childUUID : rel->Children) {
                entt::entity child = FindByUUID(reg, childUUID);
                if (child == entt::null) continue;
                const ImVec2 childPos = ImGui::GetCursorScreenPos();
                const float  childMidY = childPos.y + halfRow;
                dl->AddLine(ImVec2(lineX, childMidY), ImVec2(childPos.x, childMidY), lineCol, 1.0f);
                lastMidY = childMidY;
                DrawEntityNode(child, reg);
            }
            dl->AddLine(ImVec2(lineX, topY), ImVec2(lineX, lastMidY), lineCol, 1.0f);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // ---- Deferred mutations --------------------------------------------
    void ApplyPending(Ref<Scene>& scene, entt::registry& reg) {
        if (m_Pending.hasReparent && reg.valid(m_Pending.reparentChild)) {
            Entity child = scene->FindEntityByUUID(reg.get<IDComponent>(m_Pending.reparentChild).ID);
            Entity parent{};
            if (m_Pending.reparentTo != entt::null && reg.valid(m_Pending.reparentTo))
                parent = scene->FindEntityByUUID(reg.get<IDComponent>(m_Pending.reparentTo).ID);
            scene->ReparentKeepingWorldTransform(child, parent);
        }

        if (m_Pending.createChildOf != entt::null && reg.valid(m_Pending.createChildOf)) {
            Entity parent = scene->FindEntityByUUID(reg.get<IDComponent>(m_Pending.createChildOf).ID);
            Entity c = scene->AddEntity("Empty Entity");
            scene->SetParent(c, parent);
            PublishEvent(EntitySelectedEvent{ static_cast<entt::entity>(c) });
        }

        if (m_Pending.createEmptyRoot) {
            Entity e = scene->AddEntity("Empty Entity");
            PublishEvent(EntitySelectedEvent{ static_cast<entt::entity>(e) });
        }

        if (m_Pending.deleteEntity != entt::null && reg.valid(m_Pending.deleteEntity)) {
            const bool wasSelected = (m_Ctx->Selection == m_Pending.deleteEntity);
            Entity e = scene->FindEntityByUUID(reg.get<IDComponent>(m_Pending.deleteEntity).ID);
            scene->DestroyEntity(e, /*destroyChildren=*/true);
            if (wasSelected)
                PublishEvent(EntitySelectedEvent{ entt::null });
        }
    }

    // ---- Helpers -------------------------------------------------------
    static bool IsRoot(entt::registry& reg, const RelationshipComponent& rel) {
        if (!rel.Parent.has_value()) return true;
        // A dangling parent reference (target no longer exists) is treated as root so
        // the entity never silently disappears from the tree.
        return FindByUUID(reg, *rel.Parent) == entt::null;
    }

    static bool HasAnyValidChild(entt::registry& reg, const RelationshipComponent& rel) {
        for (const UUID& c : rel.Children)
            if (FindByUUID(reg, c) != entt::null) return true;
        return false;
    }

    // Linear scan — acceptable for editor-scale scenes.
    static entt::entity FindByUUID(entt::registry& reg, const UUID& uuid) {
        auto view = reg.view<IDComponent>();
        for (auto e : view)
            if (view.get<IDComponent>(e).ID == uuid)
                return e;
        return entt::null;
    }

    Ref<EditorContext> m_Ctx;
    Pending            m_Pending;
};
