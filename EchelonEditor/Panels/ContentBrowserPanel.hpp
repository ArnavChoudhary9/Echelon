#pragma once

#include "Echelon/ImGui/Panel.hpp"
#include "EditorContext.hpp"

#include "Echelon/Asset/AssetManager.hpp"
#include "Echelon/Asset/Material/Material.hpp"
#include "Echelon/Asset/Material/MaterialTemplate.hpp"
#include "Echelon/Asset/Importers/Material/MaterialImporter.hpp"          // SaveMaterial
#include "Echelon/Asset/Importers/Material/MaterialTemplateImporter.hpp"  // SaveMaterialTemplate

#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <utility>

namespace fs = std::filesystem;

class ContentBrowserPanel : public Panel {
public:
    explicit ContentBrowserPanel(Ref<EditorContext> ctx)
        : Panel("Content Browser"), m_Ctx(std::move(ctx))
    {
        m_Closable = false;   // essential panel — no close [x]
        auto project = Application::Get().GetProject();
        m_RootPath    = project ? project->GetRootDirectory() : fs::current_path();
        m_CurrentPath = m_RootPath;
    }

protected:
    void OnDraw() override {
        DrawNavBar();
        ImGui::Separator();
        DrawBreadcrumb();
        ImGui::Separator();
        DrawContents();
    }

private:
    // ---- Nav bar -------------------------------------------------------
    void DrawNavBar() {
        const bool canBack    = !m_BackStack.empty();
        const bool canForward = !m_ForwardStack.empty();
        const bool canUp      = (m_CurrentPath != m_RootPath && m_CurrentPath.has_parent_path());

        if (!canBack) ImGui::BeginDisabled();
        if (ImGui::ArrowButton("##cbback", ImGuiDir_Left)) NavigateBack();
        if (!canBack) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Back");

        ImGui::SameLine();

        if (!canForward) ImGui::BeginDisabled();
        if (ImGui::ArrowButton("##cbfwd", ImGuiDir_Right)) NavigateForward();
        if (!canForward) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Forward");

        ImGui::SameLine();

        if (!canUp) ImGui::BeginDisabled();
        if (ImGui::Button("  ^  ")) NavigateTo(m_CurrentPath.parent_path());
        if (!canUp) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Up");

        // ---- Create new assets in the current folder -----------------------
        ImGui::SameLine();
        if (ImGui::Button("+ New")) ImGui::OpenPopup("##cbnew");
        if (ImGui::BeginPopup("##cbnew")) {
            if (ImGui::MenuItem("Material"))          CreateMaterial();
            if (ImGui::MenuItem("Material Template")) CreateMaterialTemplate();
            ImGui::EndPopup();
        }
    }

    // ---- Asset creation ------------------------------------------------
    fs::path UniquePath(const std::string& stem, const std::string& ext) const {
        fs::path p = m_CurrentPath / (stem + ext);
        for (int i = 1; fs::exists(p); ++i)
            p = m_CurrentPath / (stem + std::to_string(i) + ext);
        return p;
    }

    // A new material instance on the built-in PBR template, seeded with sane params.
    void CreateMaterial() {
        auto mat = CreateRef<Echelon::Material>();
        mat->TemplateSource = "PBR.ehmaterialtype";
        mat->Params["BaseColor"] = Echelon::MaterialParam::Make(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
        mat->Params["Metallic"]  = Echelon::MaterialParam::Make(0.0f);
        mat->Params["Roughness"] = Echelon::MaterialParam::Make(0.5f);
        mat->Params["Occlusion"] = Echelon::MaterialParam::Make(1.0f);
        Echelon::SaveMaterial(mat, UniquePath("NewMaterial", ".ehmaterial"));
    }

    // A new material template — a copy of the built-in PBR template as a starting point.
    void CreateMaterialTemplate() {
        auto& assets = Echelon::AssetManager::Get();
        Echelon::UUID h = assets.GetHandle("PBR.ehmaterialtype");
        auto tmpl = h.IsNull() ? nullptr : assets.GetAssetAs<Echelon::MaterialTemplate>(h);
        if (tmpl)
            Echelon::SaveMaterialTemplate(tmpl, UniquePath("NewMaterialTemplate", ".ehmaterialtype"));
    }

    // ---- Breadcrumb ----------------------------------------------------
    void DrawBreadcrumb() {
        if (ImGui::SmallButton(m_RootPath.filename().string().c_str()))
            NavigateTo(m_RootPath);

        fs::path rel;
        try { rel = fs::relative(m_CurrentPath, m_RootPath); }
        catch (...) {}

        fs::path accumulated = m_RootPath;
        for (const auto& part : rel) {
            const std::string seg = part.string();
            if (seg == ".") continue;
            accumulated /= part;
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::TextUnformatted("/");
            ImGui::SameLine(0.0f, 2.0f);
            const fs::path target = accumulated;
            if (ImGui::SmallButton(seg.c_str()))
                NavigateTo(target);
        }
    }

    // ---- Grid contents -------------------------------------------------
    void DrawContents() {
        const float iconSz   = 110.0f;
        const float colW     = iconSz + 36.0f;  // fixed column width inc. gutters
        const float panelW   = ImGui::GetContentRegionAvail().x;
        const int   cols     = std::max(1, static_cast<int>(panelW / colW));

        using Entry = std::pair<fs::directory_entry, bool>;
        std::vector<Entry> entries;
        std::unordered_set<std::string> seen;
        try {
            for (const auto& e : fs::directory_iterator(m_CurrentPath)) {
                const std::string ps = e.path().string();
                if (!seen.insert(ps).second) continue;
                entries.push_back({ e, e.is_directory() });
            }
        } catch (...) {}

        std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            if (a.second != b.second) return a.second > b.second;
            return a.first.path().filename() < b.first.path().filename();
        });

        // Zero-padding table so GetColumnWidth() == colW, giving full centering control.
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 12.0f));
        if (ImGui::BeginTable("##cbgrid", cols,
                ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_NoHostExtendX)) {
            // Force each column to exactly colW.
            for (int c = 0; c < cols; ++c)
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, colW);

            for (auto& [entry, isDir] : entries) {
                ImGui::TableNextColumn();
                DrawEntry(entry, isDir, iconSz, colW);
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    void DrawEntry(const fs::directory_entry& entry, bool isDir,
                   float iconSz, float colW) {
        const std::string name = entry.path().filename().string();
        const std::string ext  = entry.path().extension().string();

        ImGui::PushID(name.c_str());

        // ---- Icon button (centered in column) --------------------------
        const float startX  = ImGui::GetCursorPosX();
        const float iconOffX = (colW - iconSz) * 0.5f;
        ImGui::SetCursorPosX(startX + iconOffX);

        const Ref<TextureAsset>& icon = isDir ? m_Ctx->DirIcon : m_Ctx->FileIcon;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

        if (icon && icon->IsValid()) {
            ImGui::ImageButton("##icon",
                static_cast<ImTextureID>(icon->GetGpuTexture()->GetNativeHandle()),
                ImVec2(iconSz, iconSz));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,
                isDir ? ImVec4(0.85f, 0.70f, 0.25f, 1.0f) : ImVec4(0.45f, 0.45f, 0.48f, 1.0f));
            ImGui::Button("##icon", ImVec2(iconSz, iconSz));
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // Overlay the file-type label at the bottom of the file icon (uses last-item rect).
        if (!isDir) {
            const char* type = ExtLabel(ext);
            const ImVec2 tsz = ImGui::CalcTextSize(type);
            const ImVec2 rMin = ImGui::GetItemRectMin();
            const ImVec2 pos(rMin.x + (iconSz - tsz.x) * 0.5f,
                             rMin.y + iconSz * 0.68f);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(pos.x - 4.0f, pos.y - 2.0f),
                ImVec2(pos.x + tsz.x + 4.0f, pos.y + tsz.y + 2.0f),
                IM_COL32(0, 0, 0, 172), 3.0f);
            ImGui::GetWindowDrawList()->AddText(pos, IM_COL32(255, 255, 255, 230), type);
        }

        // Drag source: scenes, meshes, materials, and images can be dragged onto targets.
        if (!isDir) {
            const bool isImg = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga");
            const bool draggable = (ext == ".ehscene" || ext == ".obj" || ext == ".fbx"
                                 || ext == ".gltf"    || ext == ".ehmaterial" || isImg);
            if (draggable && ImGui::BeginDragDropSource()) {
                const std::string rel = RelativePath(entry.path());
                const void* data      = rel.c_str();
                const int   sz        = static_cast<int>(rel.size() + 1);

                if      (ext == ".ehscene")    ImGui::SetDragDropPayload("DND_SCENE",    data, sz);
                else if (ext == ".ehmaterial") ImGui::SetDragDropPayload("DND_MATERIAL", data, sz);
                else if (isImg)                ImGui::SetDragDropPayload("DND_IMAGE",    data, sz);
                else                           ImGui::SetDragDropPayload("DND_MESH",     data, sz);

                ImGui::TextUnformatted(name.c_str());
                ImGui::EndDragDropSource();
            }
        }

        // Double-click a folder to enter it.
        if (isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            NavigateTo(entry.path());

        // ---- Filename label (centered, large font) ---------------------
        if (m_Ctx->FontLarge) ImGui::PushFont(m_Ctx->FontLarge);

        std::string display = name.size() > 15 ? name.substr(0, 14) + ".." : name;
        const ImVec2 textSz = ImGui::CalcTextSize(display.c_str());
        ImGui::SetCursorPosX(startX + std::max(0.0f, (colW - textSz.x) * 0.5f));
        ImGui::TextUnformatted(display.c_str());

        if (m_Ctx->FontLarge) ImGui::PopFont();

        if (ImGui::IsItemHovered() && name.size() > 15)
            ImGui::SetTooltip("%s", name.c_str());

        ImGui::PopID();
    }

    // ---- Navigation ----------------------------------------------------
    void NavigateTo(const fs::path& path) {
        if (path == m_CurrentPath) return;
        m_BackStack.push_back(m_CurrentPath);
        m_ForwardStack.clear();
        m_CurrentPath = path;
    }

    void NavigateBack() {
        if (m_BackStack.empty()) return;
        m_ForwardStack.push_back(m_CurrentPath);
        m_CurrentPath = m_BackStack.back();
        m_BackStack.pop_back();
    }

    void NavigateForward() {
        if (m_ForwardStack.empty()) return;
        m_BackStack.push_back(m_CurrentPath);
        m_CurrentPath = m_ForwardStack.back();
        m_ForwardStack.pop_back();
    }

    // ---- Helpers -------------------------------------------------------
    // Returns the project-relative path that the asset resolver expects.
    // Strips a leading "Assets/" component when that sub-directory exists,
    // because the resolver uses Assets/ as its base (e.g. "Meshes/Monkey.obj").
    std::string RelativePath(const fs::path& absPath) const {
        try {
            const fs::path assetsDir = m_RootPath / "Assets";
            if (fs::is_directory(assetsDir)) {
                const fs::path rel = fs::relative(absPath, assetsDir);
                // rel starts with ".." when absPath is NOT under assetsDir
                if (rel.begin() != rel.end() && rel.begin()->string() != "..")
                    return rel.string();
            }
            return fs::relative(absPath, m_RootPath).string();
        } catch (...) { return absPath.string(); }
    }

    static const char* ExtLabel(const std::string& ext) {
        if (ext == ".ehmaterial")     return "MAT";
        if (ext == ".ehmaterialtype") return "MATT";
        if (ext == ".ehpipeline") return "PIPE";
        if (ext == ".ehscene")    return "SCN";
        if (ext == ".slang")      return "SLNG";
        if (ext == ".glsl")       return "GLSL";
        if (ext == ".hlsl")       return "HLSL";
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") return "IMG";
        if (ext == ".obj" || ext == ".fbx" || ext == ".gltf") return "MESH";
        if (ext == ".ttf" || ext == ".otf") return "FONT";
        return "FILE";
    }

    Ref<EditorContext>    m_Ctx;
    fs::path              m_RootPath;
    fs::path              m_CurrentPath;
    std::vector<fs::path> m_BackStack;
    std::vector<fs::path> m_ForwardStack;
};
