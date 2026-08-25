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

#include "Echelon/Asset/Material/Material.hpp"
#include "Echelon/Asset/Material/MaterialTemplate.hpp"
#include "Echelon/Asset/Importers/Material/MaterialImporter.hpp"   // SaveMaterial

#include "imgui_internal.h"   // PushMultiItemsWidths for the vec3 control

#include <entt/entt.hpp>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <typeinfo>

namespace fs = std::filesystem;

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

        DrawComponent<MeshComponent>("Mesh", entity, [this](auto& c) {
            if (DrawSourceField(c.MeshSource, "DND_MESH", m_MeshOptions)) {
                c.MeshHandle   = UUID::Null();
                c.ResolveEpoch = UINT64_MAX;
                c.Invalidate();
            }
            ImGui::TextDisabled(c.IsValid() ? "resolved" : "unresolved");
        });

        DrawComponent<MaterialComponent>("Material", entity, [this](auto& c) {
            if (DrawSourceField(c.MaterialSource, "DND_MATERIAL", m_MaterialOptions)) {
                c.MaterialHandle = UUID::Null();
                c.ResolveEpoch   = UINT64_MAX;
                c.Invalidate();
            }
            ImGui::TextDisabled(c.RuntimeMaterial ? "resolved" : "unresolved (default pipeline)");

            Echelon::Ref<Echelon::Material> mat = c.RuntimeMaterial;
            if (!mat) return;
            Echelon::Ref<Echelon::MaterialTemplate> tmpl = mat->GetTemplate();

            // Template (BRDF) name — read-only; change the template by editing the .ehmaterial.
            ImGui::Spacing();
            ImGui::TextDisabled("Template");
            ImGui::SameLine();
            ImGui::TextUnformatted(mat->TemplateSource.empty() ? "(none)" : mat->TemplateSource.c_str());

            // Per-instance blend flag (opaque vs transparent pipeline variant).
            bool transparent = mat->Transparent;
            if (ImGui::Checkbox("Transparent", &transparent)) {
                mat->Transparent = transparent;
                mat->Invalidate();
            }

            if (!tmpl) {
                ImGui::TextDisabled("(template unresolved — parameters unavailable)");
                return;
            }

            constexpr ImGuiTableFlags kTF =
                ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV;
            const float kLabelW = ImGui::GetFontSize() * 6.0f;

            // Reflection/schema-driven: iterate the TEMPLATE's parameter schema, editing the
            // shared instance's values directly (affects every object using this material).
            if (!tmpl->Params.empty()) {
                ImGui::Spacing();
                ImGui::SeparatorText("Parameters");
                if (ImGui::BeginTable("##mparams", 2, kTF)) {
                    ImGui::TableSetupColumn("##L", ImGuiTableColumnFlags_WidthFixed, kLabelW);
                    ImGui::TableSetupColumn("##V", ImGuiTableColumnFlags_WidthStretch);
                    for (const auto& pd : tmpl->Params)
                        DrawMaterialParamRow(pd, *mat);
                    ImGui::EndTable();
                }
            }
            if (!tmpl->Textures.empty()) {
                ImGui::Spacing();
                ImGui::SeparatorText("Textures");
                if (ImGui::BeginTable("##mtex", 2, kTF)) {
                    ImGui::TableSetupColumn("##L", ImGuiTableColumnFlags_WidthFixed, kLabelW);
                    ImGui::TableSetupColumn("##V", ImGuiTableColumnFlags_WidthStretch);
                    for (const auto& ts : tmpl->Textures)
                        DrawTextureRow(ts.Slot, *mat);
                    ImGui::EndTable();
                }
            }

            // Persist edits back to the shared .ehmaterial asset (file-backed instances only).
            const std::string& src = c.MaterialSource;
            const bool fileBacked = src.size() > 11 &&
                                    src.compare(src.size() - 11, 11, ".ehmaterial") == 0;
            if (fileBacked) {
                ImGui::Spacing();
                if (ImGui::Button("Save Material")) {
                    if (auto project = Application::Get().GetProject()) {
                        fs::path path = project->GetAssetsDirectory() / src;
                        Echelon::SaveMaterial(mat, path);
                    }
                }
            }
        });
    }

    // ==================================================================
    // Header: entity name + "Add Component" popup
    // ==================================================================
    void DrawTagAndAddButton(Entity entity) {
        const float addBtnW = ImGui::CalcTextSize("Add Component").x
                            + ImGui::GetStyle().FramePadding.x * 2.0f + 4.0f;

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

    /** @brief Source path field: label | input+DnD | dropdown picker. */
    bool DrawSourceField(std::string& source, const char* dndType,
                         std::vector<std::string>& options) {
        bool changed    = false;
        bool openPicker = false;
        ImGui::PushID(dndType);

        if (ImGui::BeginTable("##sf", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("##L", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("##V", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Source");
            ImGui::TableSetColumnIndex(1);

            const float btnW = ImGui::GetFrameHeight();
            ImGui::SetNextItemWidth(
                ImGui::GetContentRegionAvail().x - btnW - ImGui::GetStyle().ItemSpacing.x);

            char buf[256] = {};
            std::strncpy(buf, source.c_str(), sizeof(buf) - 1);
            if (ImGui::InputText("##src", buf, sizeof(buf))) {
                source = buf; changed = true;
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(dndType)) {
                    source = static_cast<const char*>(p->Data); changed = true;
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine();
            // Store the click result — do NOT call OpenPopup here.
            // BeginTable pushes its own ID scope, so a popup opened inside it
            // gets a different full ID than one opened outside → popup never shows.
            openPicker = ImGui::ArrowButton("##pick", ImGuiDir_Down);
            ImGui::EndTable();
        }

        // Both OpenPopup and BeginPopup must share the same ID scope (PushID(dndType)).
        if (openPicker) {
            RefreshAssetCache();
            ImGui::OpenPopup("##opts");
        }
        if (ImGui::BeginPopup("##opts")) {
            if (options.empty()) {
                ImGui::TextDisabled("(no assets found)");
            } else {
                for (const auto& opt : options) {
                    const bool sel = (source == opt);
                    if (ImGui::Selectable(opt.c_str(), sel)) {
                        source = opt; changed = true; ImGui::CloseCurrentPopup();
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    void RefreshAssetCache() {
        m_MeshOptions     = { "Sphere", "Cube", "Plane" };
        m_MaterialOptions.clear();

        auto project = Application::Get().GetProject();
        if (!project) return;

        const fs::path root      = project->GetRootDirectory();
        const fs::path assetsDir = root / "Assets";
        const fs::path base      = fs::is_directory(assetsDir) ? assetsDir : root;
        try {
            for (const auto& e : fs::recursive_directory_iterator(
                     root, fs::directory_options::skip_permission_denied)) {
                if (!e.is_regular_file()) continue;
                const std::string ext = e.path().extension().string();
                try {
                    const fs::path rel = fs::relative(e.path(), base);
                    if (rel.begin() != rel.end() && rel.begin()->string() == "..") continue;
                    const std::string relStr = rel.string();
                    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf")
                        m_MeshOptions.push_back(relStr);
                    else if (ext == ".ehmaterial")
                        m_MaterialOptions.push_back(relStr);
                } catch (...) {}
            }
        } catch (...) {}
    }

    /**
     * @brief One row inside an active BeginTable("##mparams", 2) block.
     *        Driven by the template's parameter schema (type + UI hint). Edits the
     *        shared Material instance's value directly (yellow = the instance overrides
     *        the template default). Color params (Hint == Color) use a swatch; others a
     *        type-appropriate drag editor. Inline reset [x] clears the instance value so
     *        it falls back to the template default. Bumps Material::Version on change so
     *        the renderer re-packs for every object using this material.
     */
    static void DrawMaterialParamRow(const Echelon::MaterialTemplate::ParamDesc& pd,
                                     Echelon::Material& mat) {
        using Echelon::MaterialParam;
        using Echelon::MaterialParamType;
        using Echelon::ParamUi;

        const std::string& name = pd.Name;
        auto it                 = mat.Params.find(name);
        const bool hasValue     = (it != mat.Params.end());
        MaterialParam current   = hasValue ? it->second : pd.Default;
        bool changed            = false;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::PushID(name.c_str());

        ImGui::PushStyleColor(ImGuiCol_Text,
            hasValue ? ImVec4(0.90f, 0.78f, 0.30f, 1.0f)
                     : ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        ImGui::TextUnformatted(name.c_str());
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", name.c_str());

        ImGui::TableSetColumnIndex(1);

        const float fh     = ImGui::GetFrameHeight();
        const float resetW = hasValue ? (fh + ImGui::GetStyle().ItemSpacing.x) : 0.0f;

        const bool colorField = (pd.Hint == ParamUi::Color) &&
                                (pd.Type == MaterialParamType::Float3 ||
                                 pd.Type == MaterialParamType::Float4);
        if (!colorField)
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - resetW);

        switch (pd.Type) {
            case MaterialParamType::Float: {
                float v = current.Data[0];
                if (ImGui::DragFloat("##v", &v, 0.01f))
                    { mat.Params[name] = MaterialParam::Make(v); changed = true; }
                break;
            }
            case MaterialParamType::Float2: {
                glm::vec2 v(current.Data[0], current.Data[1]);
                if (ImGui::DragFloat2("##v", &v.x, 0.01f))
                    { mat.Params[name] = MaterialParam::Make(v); changed = true; }
                break;
            }
            case MaterialParamType::Float3: {
                glm::vec3 v(current.Data[0], current.Data[1], current.Data[2]);
                bool edit = colorField
                    ? ImGui::ColorEdit3("##v", &v.x, ImGuiColorEditFlags_NoInputs)
                    : ImGui::DragFloat3("##v", &v.x, 0.01f);
                if (edit) { mat.Params[name] = MaterialParam::Make(v); changed = true; }
                break;
            }
            case MaterialParamType::Float4: {
                glm::vec4 v(current.Data[0], current.Data[1], current.Data[2], current.Data[3]);
                bool edit = colorField
                    ? ImGui::ColorEdit4("##v", &v.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview)
                    : ImGui::DragFloat4("##v", &v.x, 0.01f);
                if (edit) { mat.Params[name] = MaterialParam::Make(v); changed = true; }
                break;
            }
            case MaterialParamType::Int: {
                int v = static_cast<int>(current.Data[0]);
                if (ImGui::DragInt("##v", &v))
                    { mat.Params[name] = MaterialParam::MakeInt(v); changed = true; }
                break;
            }
            default: ImGui::TextDisabled("(mat4)"); break;
        }

        if (hasValue) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.50f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
            if (ImGui::Button("x", ImVec2(fh, fh))) {
                mat.Params.erase(name); changed = true;
            }
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset to template default");
        }

        if (changed) mat.Invalidate();
        ImGui::PopID();
    }

    /** @brief One row inside an active BeginTable("##mtex", 2) block.
     *         Shows a template texture slot and the instance's editable path field.
     *         Accepts DND_IMAGE drag-drop payloads. Editing writes the shared Material
     *         instance's Textures[slot] and bumps Version so the renderer re-resolves.
     */
    static void DrawTextureRow(const std::string& slotName, Echelon::Material& mat) {
        ImGui::TableNextRow();
        ImGui::PushID(slotName.c_str());

        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
        ImGui::TextUnformatted(slotName.c_str());
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", slotName.c_str());

        ImGui::TableSetColumnIndex(1);
        auto it = mat.Textures.find(slotName);
        const std::string cur = (it != mat.Textures.end()) ? it->second : std::string();
        char buf[256] = {};
        std::strncpy(buf, cur.c_str(), sizeof(buf) - 1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::InputText("##t", buf, sizeof(buf)))
            { mat.Textures[slotName] = buf; mat.Invalidate(); }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("DND_IMAGE"))
                { mat.Textures[slotName] = static_cast<const char*>(p->Data); mat.Invalidate(); }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
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

    Ref<EditorContext>       m_Ctx;
    std::vector<std::string> m_MeshOptions;
    std::vector<std::string> m_MaterialOptions;
};
