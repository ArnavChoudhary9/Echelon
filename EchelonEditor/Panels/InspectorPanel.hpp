#pragma once

/**
 * @file InspectorPanel.hpp
 * @brief Inspector: edits the components of the currently selected entity.
 *
 * Reads EditorContext::Selection (kept in sync by the coordinator from bus
 * EntitySelectedEvents) and edits the selected entity's components in place.
 *
 * ------------------------------------------------------------------------
 * ADDING A NEW COMPONENT TO THE INSPECTOR (two one-liners):
 *
 *   1. In DrawComponents(), add a draw block describing its UI:
 *          DrawComponent<MyComponent>("My Component", entity, [](auto& c) {
 *              ImGui::DragFloat("Value", &c.Value);
 *          });
 *
 *   2. In the "Add Component" popup (DrawAddComponentMenu()), let the user
 *      attach it:
 *          AddComponentEntry<MyComponent>(entity, "My Component");
 *
 * DrawComponent<T> handles the collapsing framed header, the per-component
 * "remove" menu, and the has-component check; the lambda only draws the
 * fields. Pass allowRemove=false for structural components (e.g. Transform).
 * ------------------------------------------------------------------------
 */

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

#include "imgui_internal.h"   // PushMultiItemsWidths for the vec3 control

#include <entt/entt.hpp>
#include <cstring>
#include <cstdint>
#include <string>
#include <typeinfo>

class InspectorPanel : public Panel {
public:
    explicit InspectorPanel(Ref<EditorContext> ctx)
        : Panel("Inspector"), m_Ctx(std::move(ctx)) {
        m_Closable = false;   // essential panel — no close [x]
    }

protected:
    void OnDraw() override {
        Ref<Scene> scene = m_Ctx->ActiveScene;
        auto registry = scene ? scene->GetEntityRegistry().lock() : nullptr;
        const entt::entity sel = m_Ctx->Selection;

        if (!scene || !registry || sel == entt::null || !registry->valid(sel)) {
            ImGui::TextUnformatted("No entity selected");
            return;
        }

        // Wrap the raw handle so the modular helpers can add / remove / query
        // components uniformly (see Entity in ECS/Entity.hpp).
        Entity entity{ sel, CreateWeakRef(scene) };
        DrawComponents(entity);
    }

private:
    // ==================================================================
    // Component list — the modular part. Each block is self-contained.
    // ==================================================================
    void DrawComponents(Entity entity) {
        DrawTagAndAddButton(entity);

        DrawComponent<TransformComponent>("Transform", entity, [](auto& c) {
            DrawVec3Control("Position", c.Position);
            DrawVec3Control("Rotation", c.Rotation);           // stored as euler degrees
            DrawVec3Control("Scale",    c.Scale, 1.0f);
        }, /*allowRemove=*/false);

        DrawComponent<CameraComponent>("Camera", entity, [](auto& c) {
            Camera& cam = c.Cam;
            ImGui::Checkbox("Primary", &c.Primary);

            const char* projTypes[] = { "Perspective", "Orthographic" };
            const int   current     = static_cast<int>(cam.GetProjectionType());
            if (ImGui::BeginCombo("Projection", projTypes[current])) {
                for (int i = 0; i < 2; ++i) {
                    const bool selected = (current == i);
                    if (ImGui::Selectable(projTypes[i], selected))
                        cam.SetProjectionType(static_cast<ProjectionType>(i));
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (cam.GetProjectionType() == ProjectionType::Perspective) {
                float fov = cam.GetFOV(), nearC = cam.GetNearClip(), farC = cam.GetFarClip();
                bool changed = false;
                changed |= ImGui::DragFloat("FOV",  &fov,   0.1f, 1.0f,   179.0f);
                changed |= ImGui::DragFloat("Near", &nearC, 0.01f, 0.001f, farC);
                changed |= ImGui::DragFloat("Far",  &farC,  1.0f,  nearC,  100000.0f);
                if (changed) cam.SetPerspective(fov, nearC, farC);
            } else {
                float size = cam.GetOrthoSize(), nearC = cam.GetOrthoNearClip(), farC = cam.GetOrthoFarClip();
                bool changed = false;
                changed |= ImGui::DragFloat("Size", &size,  0.1f);
                changed |= ImGui::DragFloat("Near", &nearC, 0.1f);
                changed |= ImGui::DragFloat("Far",  &farC,  0.1f);
                if (changed) cam.SetOrthographic(size, nearC, farC);
                ImGui::Checkbox("Fixed Aspect Ratio", &c.FixedAspect);
            }
        });

        DrawComponent<LightComponent>("Light", entity, [](auto& c) {
            const char* types[]  = { "Directional", "Point", "Spot" };
            const int   current  = static_cast<int>(c.Type);
            if (ImGui::BeginCombo("Type", types[current])) {
                for (int i = 0; i < 3; ++i) {
                    const bool selected = (current == i);
                    if (ImGui::Selectable(types[i], selected))
                        c.Type = static_cast<LightType>(i);
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("Enabled", &c.Enabled);
            ImGui::ColorEdit3("Color", &c.Color.x);
            ImGui::DragFloat("Intensity", &c.Intensity, 0.1f, 0.0f, 1000.0f);

            if (c.Type != LightType::Directional)
                ImGui::DragFloat("Range", &c.Range, 0.1f, 0.0f, 10000.0f);

            if (c.Type == LightType::Spot) {
                ImGui::DragFloat("Inner Angle", &c.InnerAngle, 0.5f, 0.0f, c.OuterAngle);
                ImGui::DragFloat("Outer Angle", &c.OuterAngle, 0.5f, c.InnerAngle, 89.0f);
            }

            ImGui::Checkbox("Casts Shadows", &c.CastsShadows);
            if (c.CastsShadows)
                ImGui::DragFloat("Shadow Bias", &c.ShadowBias, 0.0001f, 0.0f, 0.1f, "%.4f");
        });

        DrawComponent<MeshComponent>("Mesh", entity, [](auto& c) {
            // Editing the source clears the (possibly stale) handle and forces a
            // fresh resolve next frame — see RenderGraph mesh resolution.
            if (DrawAssetSourceField("Source", c.MeshSource)) {
                c.MeshHandle   = UUID::Null();
                c.ResolveEpoch = UINT64_MAX;
                c.Invalidate();
            }
            ImGui::TextDisabled(c.IsValid() ? "resolved" : "unresolved");
        });

        DrawComponent<MaterialComponent>("Material", entity, [](auto& c) {
            if (DrawAssetSourceField("Source", c.MaterialSource)) {
                c.MaterialHandle = UUID::Null();
                c.ResolveEpoch   = UINT64_MAX;
                c.Invalidate();
            }
            ImGui::TextDisabled(c.RuntimeMaterial ? "resolved" : "unresolved (default pipeline)");
        });
    }

    // ==================================================================
    // Header: entity name + "Add Component" popup
    // ==================================================================
    void DrawTagAndAddButton(Entity entity) {
        constexpr float addBtnW = 120.0f;

        if (entity.HasComponent<TagComponent>()) {
            auto& tag = entity.GetComponent<TagComponent>().Tag;
            char buffer[256];
            std::memset(buffer, 0, sizeof(buffer));
            std::strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);
            ImGui::SetNextItemWidth(
                ImGui::GetContentRegionAvail().x - addBtnW - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
                tag = buffer;
            ImGui::SameLine();
        }

        if (ImGui::Button("Add Component", ImVec2(addBtnW, 0.0f)))
            ImGui::OpenPopup("AddComponent");

        if (ImGui::BeginPopup("AddComponent")) {
            AddComponentEntry<CameraComponent>(entity,   "Camera");
            AddComponentEntry<LightComponent>(entity,    "Light");
            AddComponentEntry<MeshComponent>(entity,     "Mesh");
            AddComponentEntry<MaterialComponent>(entity, "Material");
            ImGui::EndPopup();
        }
    }

    // ==================================================================
    // Reusable UI building blocks
    // ==================================================================

    /**
     * @brief Labelled X/Y/Z drag control with colored per-axis reset buttons.
     *        Clicking an axis letter resets that component to @p resetValue.
     */
    static void DrawVec3Control(const std::string& label, glm::vec3& values,
                                float resetValue = 0.0f, float columnWidth = 90.0f) {
        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::TextUnformatted(label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        const float  lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        const ImVec2 buttonSize(lineHeight + 3.0f, lineHeight);

        AxisButton("X", ImVec4(0.80f, 0.15f, 0.20f, 1.0f), buttonSize, values.x, resetValue);
        ImGui::SameLine();
        ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        AxisButton("Y", ImVec4(0.20f, 0.65f, 0.25f, 1.0f), buttonSize, values.y, resetValue);
        ImGui::SameLine();
        ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();
        ImGui::SameLine();

        AxisButton("Z", ImVec4(0.15f, 0.30f, 0.80f, 1.0f), buttonSize, values.z, resetValue);
        ImGui::SameLine();
        ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();
    }

    /**
     * @brief Framed collapsing section for one component, with a "remove" menu.
     *        Draws nothing when the entity lacks the component.
     */
    template<typename T, typename UIFunction>
    static void DrawComponent(const std::string& name, Entity entity,
                              UIFunction uiFunction, bool allowRemove = true) {
        if (!entity.HasComponent<T>()) return;

        constexpr ImGuiTreeNodeFlags treeNodeFlags =
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_FramePadding;

        auto& component = entity.GetComponent<T>();

        ImGui::PushID(name.c_str());   // scope the "..." button + settings popup per component

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        const float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImGui::Separator();
        const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(typeid(T).hash_code()),
                                            treeNodeFlags, "%s", name.c_str());
        ImGui::PopStyleVar();

        bool removeComponent = false;
        if (allowRemove) {
            ImGui::SameLine(avail.x - lineHeight * 0.5f);
            if (ImGui::Button("...", ImVec2(lineHeight, lineHeight)))
                ImGui::OpenPopup("ComponentSettings");
            if (ImGui::BeginPopup("ComponentSettings")) {
                if (ImGui::MenuItem("Remove Component"))
                    removeComponent = true;
                ImGui::EndPopup();
            }
        }

        if (open) {
            uiFunction(component);
            ImGui::TreePop();
        }

        ImGui::PopID();

        if (removeComponent)
            entity.RemoveComponent<T>();
    }

    /** @brief Menu item that attaches component T when the entity doesn't have it. */
    template<typename T>
    static void AddComponentEntry(Entity entity, const char* label) {
        if (entity.HasComponent<T>()) return;
        if (ImGui::MenuItem(label)) {
            entity.AddComponent<T>();
            ImGui::CloseCurrentPopup();
        }
    }

    /** @brief Editable text field bound to an asset source string; returns true on change. */
    static bool DrawAssetSourceField(const char* label, std::string& source) {
        char buffer[256];
        std::memset(buffer, 0, sizeof(buffer));
        std::strncpy(buffer, source.c_str(), sizeof(buffer) - 1);
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            source = buffer;
            return true;
        }
        return false;
    }

    /** @brief One colored, resettable axis button used by DrawVec3Control. */
    static void AxisButton(const char* label, const ImVec4& color,
                           const ImVec2& size, float& value, float resetValue) {
        const ImVec4 hovered(color.x + 0.1f, color.y + 0.1f, color.z + 0.1f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  color);
        if (ImGui::Button(label, size))
            value = resetValue;
        ImGui::PopStyleColor(3);
    }

    Ref<EditorContext> m_Ctx;
};
