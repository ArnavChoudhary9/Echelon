#pragma once

#include "Echelon/Echelon.hpp"
#include "Echelon/Asset/RenderPipeline/RenderPipelineDesc.hpp"   // aux id-pass description

#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder* for the first-run default layout

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>
#include <string>

using namespace Echelon;

class EditorOverlay : public Overlay {
public:
    EditorOverlay() : Overlay() {}
    virtual ~EditorOverlay() {}

    virtual void OnAttach() override {
        auto& window = Application::Get().GetWindow();

        // ---- Load or create scene ----
        auto project = Application::Get().GetProject();
        if (project) {
            m_EditScene = project->GetCurrentScene();
            if (!m_EditScene && !project->GetConfig().StartScene.empty())
                m_EditScene = project->OpenScene(project->GetConfig().StartScene);
            if (!m_EditScene)
                m_EditScene = project->NewScene("Editor Scene");
        } else {
            m_EditScene = CreateRef<Scene>("Editor Scene");
        }
        m_Scene = m_EditScene;   // active scene = the authoritative edit scene until Play

        auto* renderer = Renderer::Get().GetActive();
        renderer->SetVSync(false);

        // Present the scene inside the ImGui "Viewport" panel: render the pass graph
        // into an offscreen texture instead of the window backbuffer.
        renderer->SetViewportTarget(true);

        // Contribute the editor's object-id pass to the render pass graph (generic
        // auxiliary passes; nothing picking-specific lives in the renderer). It renders
        // every mesh's id into an R32_UINT target we read back on click. See
        // BuildIdPassDesc() + EntityID.slang.
        renderer->SetAuxiliaryPipeline(BuildIdPassDesc());

        // ---- Initialize editor camera ----
        m_EditorCamera.SetPerspective(60.0f, 0.1f, 1000.0f);
        m_EditorCamera.SetViewportSize(window.GetWidth(), window.GetHeight());
        m_EditorCamera.SetPosition(m_EditorPos);
        m_EditorCamera.SetRotation(m_EditorRot);

        // ---- Populate demo scene if empty ----
        auto registry = m_Scene->GetEntityRegistry().lock();
        bool hasCamera = false;
        if (registry) {
            auto camView = registry->view<CameraComponent>();
            hasCamera = !camView.empty();
        }

        if (!hasCamera) {
            Entity cameraEntity = m_Scene->AddEntity("Camera");
            auto& camTransform = cameraEntity.GetComponent<TransformComponent>();
            camTransform.Position = { 0.0f, 14.0f, 10.0f };
            camTransform.Rotation = { -52.0f, 0.0f, 0.0f };

            auto& cam = cameraEntity.AddComponent<CameraComponent>();
            cam.Primary = true;
            cam.Cam.SetPerspective(55.0f, 0.1f, 1000.0f);
            cam.Cam.SetViewportSize(window.GetWidth(), window.GetHeight());
            cam.Cam.SetPosition(camTransform.Position);
            cam.Cam.SetRotation(camTransform.Rotation);

            auto addMesh = [&](const char* tag, const char* meshSrc, const char* material,
                               glm::vec3 pos, glm::vec3 scale, glm::vec3 rot = glm::vec3(0.0f)) {
                Entity e = m_Scene->AddEntity(tag);
                auto& t = e.GetComponent<TransformComponent>();
                t.Position = pos; t.Scale = scale; t.Rotation = rot;
                e.AddComponent<MeshComponent>().MeshSource      = meshSrc;
                e.AddComponent<MaterialComponent>().MaterialSource = material;
            };

            addMesh("Ground",          "Plane",             "Materials/Ground.ehmaterial",   { 0, 0, 0 },          { 30, 1, 30 });
            // Spheres: solid materials from metallic (left) to rough (right).
            addMesh("Sphere_Metal",    "Sphere",            "Materials/Metal.ehmaterial",    { -6, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Gold",     "Sphere",            "Materials/Gold.ehmaterial",     { -3, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Plastic",  "Sphere",            "Materials/Plastic.ehmaterial",  {  0, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Orange",   "Sphere",            "Materials/Orange.ehmaterial",   {  3, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Rough",    "Sphere",            "Materials/Rough.ehmaterial",    {  6, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            // Cube carries the albedo texture (Lit).
            addMesh("Cube",            "Cube",              "Materials/Lit.ehmaterial",      { -2.5f, 0.6f, 3 },   { 1.2f, 1.2f, 1.2f }, { 0, 25, 0 });
            addMesh("Monkey",          "Meshs/Monkey.obj",  "Materials/Orange.ehmaterial",   {  2.5f, 1.0f, 3 },   { 1, 1, 1 },          { 0, -35, 0 });

            {
                Entity sun = m_Scene->AddEntity("Sun");
                sun.GetComponent<TransformComponent>().Rotation = { -50.0f, -35.0f, 0.0f };
                auto& l = sun.AddComponent<LightComponent>();
                l.Type = LightType::Directional; l.Color = { 1.0f, 0.96f, 0.9f };
                l.Intensity = 2.5f; l.Range = 40.0f; l.CastsShadows = true;
            }
            {
                Entity spot = m_Scene->AddEntity("Spot");
                auto& t = spot.GetComponent<TransformComponent>();
                t.Position = { 0.0f, 9.0f, 3.0f }; t.Rotation = { -72.0f, 0.0f, 0.0f };
                auto& l = spot.AddComponent<LightComponent>();
                l.Type = LightType::Spot; l.Color = { 0.3f, 0.7f, 1.0f };
                l.Intensity = 60.0f; l.Range = 25.0f; l.InnerAngle = 18.0f; l.OuterAngle = 30.0f;
                l.CastsShadows = true;
            }
            {
                Entity pt = m_Scene->AddEntity("PointWarm");
                pt.GetComponent<TransformComponent>().Position = { -5.0f, 3.0f, 2.0f };
                auto& l = pt.AddComponent<LightComponent>();
                l.Type = LightType::Point; l.Color = { 1.0f, 0.5f, 0.2f };
                l.Intensity = 40.0f; l.Range = 18.0f; l.CastsShadows = true; l.ShadowBias = 0.05f;
            }
            {
                Entity pt = m_Scene->AddEntity("PointFill");
                pt.GetComponent<TransformComponent>().Position = { 6.0f, 3.0f, -4.0f };
                auto& l = pt.AddComponent<LightComponent>();
                l.Type = LightType::Point; l.Color = { 0.4f, 0.5f, 1.0f };
                l.Intensity = 20.0f; l.Range = 15.0f; l.CastsShadows = false;
            }
        }
    }

    virtual void OnDetach() override {
        // Never persist play-mode changes: the project's current scene is the edit scene,
        // which we never mutate during play, so saving it is always the authoritative state.
        auto project = Application::Get().GetProject();
        if (project)
            project->SaveScene();
        m_Scene        = nullptr;
        m_EditScene    = nullptr;
        m_RuntimeScene = nullptr;
    }

    virtual void OnUpdate(float deltaTime) override {
        ECHELON_PROFILE_FUNCTION();
        m_LastDeltaTime = deltaTime;

        // ---- Match the render target to the Viewport panel size (set last frame) ----
        SyncViewportSize();

        if (m_IsPlaying) {
            // Animate entities in play mode
            auto registry = m_Scene->GetEntityRegistry().lock();
            if (registry) {
                auto meshView = registry->view<MeshComponent, TransformComponent, TagComponent>();
                for (auto&& [entity, mesh, tc, tag] : meshView.each()) {
                    if (tag.Tag == "Ground") continue;
                    tc.Rotation.y += 20.0f * deltaTime;
                }
            }
        } else {
            // ---- Editor camera. Active while Left Alt is held (momentary "fast" access)
            //      OR free-cam mode is toggled on (` key). While active the camera responds
            //      regardless of which panel has focus/hover, so you can fly the camera even
            //      with another panel selected. (Left-click without the camera active is a
            //      pick — see DrawViewportPanel.) ----
            float dx = m_MouseDeltaX;
            float dy = m_MouseDeltaY;
            m_MouseDeltaX = 0.0f;
            m_MouseDeltaY = 0.0f;

            const bool camActive = m_FreeCam || Input::IsKeyPressed(Key::LeftAlt);

            if (camActive) {
                const float panSpeed = 0.015f * (Input::IsKeyPressed(Key::LeftShift) ? 4.0f : 1.0f);

                // LMB → look (pan/tilt)
                if (Input::IsMouseButtonPressed(Mouse::ButtonLeft)) {
                    m_EditorRot.y -= dx * m_LookSensitivity;
                    m_EditorRot.x -= dy * m_LookSensitivity;
                    m_EditorRot.x  = glm::clamp(m_EditorRot.x, -89.0f, 89.0f);
                    m_EditorCamera.SetRotation(m_EditorRot);
                }

                // MMB → pan (truck / pedestal)
                if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle)) {
                    m_EditorPos -= m_EditorCamera.GetRight() * dx * panSpeed;
                    m_EditorPos += m_EditorCamera.GetUp()   * dy * panSpeed;
                }

                // Scroll → dolly (zoom)
                if (m_ScrollDelta != 0.0f) {
                    m_EditorPos += m_EditorCamera.GetForward() * m_ScrollDelta * m_MoveSpeed * 0.35f;
                }

                // WASD → fly through scene (Q/E for world-up/down).
                const float speed = m_MoveSpeed * (Input::IsKeyPressed(Key::LeftShift) ? 4.0f : 1.0f);
                glm::vec3 move(0.0f);
                if (Input::IsKeyPressed(Key::W)) move += m_EditorCamera.GetForward();
                if (Input::IsKeyPressed(Key::S)) move -= m_EditorCamera.GetForward();
                if (Input::IsKeyPressed(Key::A)) move -= m_EditorCamera.GetRight();
                if (Input::IsKeyPressed(Key::D)) move += m_EditorCamera.GetRight();
                if (Input::IsKeyPressed(Key::E)) move.y += 1.0f;
                if (Input::IsKeyPressed(Key::Q)) move.y -= 1.0f;

                if (glm::length(move) > 0.001f)
                    m_EditorPos += glm::normalize(move) * speed * deltaTime;
            }
            m_ScrollDelta = 0.0f;

            m_EditorCamera.SetPosition(m_EditorPos);
        }

        // ---- Render ----
        {
            ECHELON_PROFILE_SCOPE("Rendering Loop");
            auto* renderer = Renderer::Get().GetActive();
            if (!renderer) return;

            glm::mat4 viewMatrix(1.0f);
            glm::mat4 projMatrix(1.0f);

            if (m_IsPlaying) {
                ECHELON_PROFILE_SCOPE("Find Primary Camera");
                auto registry = m_Scene->GetEntityRegistry().lock();
                if (registry) {
                    auto camView = registry->view<CameraComponent, TransformComponent>();
                    for (auto&& [entity, cc, tc] : camView.each()) {
                        if (cc.Primary) {
                            cc.Cam.SetPosition(tc.Position);
                            cc.Cam.SetRotation(tc.Rotation);
                            viewMatrix = cc.Cam.GetViewMatrix();
                            projMatrix = cc.Cam.GetProjectionMatrix();
                            break;
                        }
                    }
                }
            } else {
                viewMatrix = m_EditorCamera.GetViewMatrix();
                projMatrix = m_EditorCamera.GetProjectionMatrix();
            }

            {
                ECHELON_PROFILE_SCOPE("Render Scene");
                ClearValue clear;
                clear.Color = { 0.1f, 0.1f, 0.12f, 1.0f };
                renderer->BeginFrame(viewMatrix, projMatrix, clear);
                renderer->BeginScene(m_Scene);
                renderer->RenderScene(m_Scene);
                renderer->EndScene();
                renderer->EndFrame();
            }
        }
    }

    virtual void OnEvent(Event& event) override {
        EventDispatcher dispatcher(event);

        // Accumulate mouse delta whenever the camera is active (free-cam toggle OR Alt),
        // regardless of which panel is hovered — so the camera can move no matter the
        // selected panel. Only in edit mode.
        dispatcher.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& e) {
            if (!m_IsPlaying && CameraActive()) {
                m_MouseDeltaX += e.GetX() - m_LastMouseX;
                m_MouseDeltaY += e.GetY() - m_LastMouseY;
            }
            m_LastMouseX = e.GetX();
            m_LastMouseY = e.GetY();
            return false;
        });

        // Accumulate scroll delta whenever the camera is active.
        dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e) {
            if (!m_IsPlaying && CameraActive())
                m_ScrollDelta += e.GetYOffset();
            return false;
        });

        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
            if (e.GetRepeatCount() != 0) return false;
            // P toggles play / edit (only with the viewport focused so it does not fire
            // while typing in another panel). Play runs on a throwaway copy of the scene.
            if (m_ViewportFocused && e.GetKeyCode() == Key::P)
                SetPlaying(!m_IsPlaying);
            // ` (grave) toggles free-camera mode — move the camera without holding Alt.
            if (e.GetKeyCode() == Key::GraveAccent)
                m_FreeCam = !m_FreeCam;
            return false;
        });
    }

    // ------------------------------------------------------------------
    // ImGui — the whole editor window is a dockspace hosting a scene viewport
    // ------------------------------------------------------------------

    virtual void OnImGUIBegin() override {
        // Fullscreen host window that owns the dockspace covering the entire window.
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Echelon##DockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                    if (auto project = Application::Get().GetProject()) project->SaveScene();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) Application::Get().Close();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
                ImGui::MenuItem("Inspector", nullptr, &m_ShowInspector);
                ImGui::MenuItem("Stats",     nullptr, &m_ShowStats);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        const ImGuiID dockspaceId = ImGui::GetID("EchelonDockSpace");

        // Build a sensible default layout the first time — but only if nothing was
        // restored from imgui.ini. This must run BEFORE DockSpace(), which itself
        // creates the node (so checking existence afterwards would always find one).
        if (!m_DockLayoutInit) {
            m_DockLayoutInit = true;
            if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
                BuildDefaultDockLayout(dockspaceId);
        }

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        ImGui::End();   // host window (dockspace only; panels are separate windows below)
    }

    virtual void OnImGUIRender() override {
        DrawViewportPanel();
        if (m_ShowHierarchy) DrawHierarchyPanel();
        if (m_ShowInspector) DrawInspectorPanel();
        if (m_ShowStats)     DrawStatsPanel();
    }

    virtual void OnImGUIEnd() override {}

private:
    // ---- Viewport panel: the scene, rendered into an offscreen texture ----
    void DrawViewportPanel() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        m_ViewportFocused = ImGui::IsWindowFocused();
        m_ViewportHovered = ImGui::IsWindowHovered();

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        m_ViewportSize = { avail.x, avail.y };

        auto* renderer = Renderer::Get().GetActive();
        Ref<Texture> tex = renderer ? renderer->GetViewportTexture() : nullptr;
        if (tex && avail.x > 0.0f && avail.y > 0.0f) {
            // GL textures have their origin at the bottom-left, so flip V.
            ImGui::Image(static_cast<ImTextureID>(tex->GetNativeHandle()),
                         avail, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

            // Left-click (without the camera active) picks the entity under the cursor by
            // reading the object-id pass's render target. The camera is driven by Alt /
            // free-cam, so a plain click is unambiguously a pick.
            if (renderer && !CameraActive()
                && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 rmin = ImGui::GetItemRectMin();
                const ImVec2 rsz  = ImGui::GetItemRectSize();
                const float localX = ImGui::GetMousePos().x - rmin.x;
                const float localY = ImGui::GetMousePos().y - rmin.y;
                if (rsz.x > 0.0f && rsz.y > 0.0f &&
                    localX >= 0.0f && localY >= 0.0f && localX < rsz.x && localY < rsz.y) {
                    const uint32_t gx = static_cast<uint32_t>(localX);
                    // The id target shares the scene's bottom-left GL origin; the image is
                    // shown V-flipped (upright), so flip the click's Y back to texel space.
                    const uint32_t gy = static_cast<uint32_t>(rsz.y - 1.0f - localY);
                    PickEntityAt(gx, gy);
                }
            }
        } else {
            ImGui::TextUnformatted("No viewport texture");
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    // ---- Scene hierarchy: list + select entities ----
    void DrawHierarchyPanel() {
        ImGui::Begin("Hierarchy", &m_ShowHierarchy);
        auto registry = m_Scene ? m_Scene->GetEntityRegistry().lock() : nullptr;
        if (registry) {
            auto view = registry->view<TagComponent>();
            for (auto entity : view) {
                const auto& tag = view.get<TagComponent>(entity);
                const bool selected = (entity == m_Selected);
                ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
                if (ImGui::Selectable(tag.Tag.c_str(), selected))
                    m_Selected = entity;
                ImGui::PopID();
            }
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && !ImGui::IsAnyItemHovered())
                m_Selected = entt::null;   // click empty space to deselect
        }
        ImGui::End();
    }

    // ---- Inspector: edit the selected entity's transform ----
    void DrawInspectorPanel() {
        ImGui::Begin("Inspector", &m_ShowInspector);
        auto registry = m_Scene ? m_Scene->GetEntityRegistry().lock() : nullptr;
        if (registry && m_Selected != entt::null && registry->valid(m_Selected)) {
            if (auto* tag = registry->try_get<TagComponent>(m_Selected))
                ImGui::Text("%s", tag->Tag.c_str());
            ImGui::Separator();
            if (auto* t = registry->try_get<TransformComponent>(m_Selected)) {
                ImGui::DragFloat3("Position", &t->Position.x, 0.05f);
                ImGui::DragFloat3("Rotation", &t->Rotation.x, 0.25f);
                ImGui::DragFloat3("Scale",    &t->Scale.x,    0.05f);
            }
            if (auto* l = registry->try_get<LightComponent>(m_Selected)) {
                ImGui::Separator();
                ImGui::ColorEdit3("Light Color", &l->Color.x);
                ImGui::DragFloat("Intensity", &l->Intensity, 0.1f, 0.0f, 1000.0f);
            }
        } else {
            ImGui::TextUnformatted("No entity selected");
        }
        ImGui::End();
    }

    // ---- Stats / controls ----
    void DrawStatsPanel() {
        ImGui::Begin("Stats", &m_ShowStats);
        ImGui::Text("%.1f FPS (%.2f ms)", m_LastDeltaTime > 0.0f ? 1.0f / m_LastDeltaTime : 0.0f,
                    m_LastDeltaTime * 1000.0f);
        if (auto* renderer = Renderer::Get().GetActive()) {
            const RenderStats s = renderer->GetStats();
            ImGui::Text("Draw calls: %u", s.DrawCalls);
        }
        ImGui::Separator();
        if (ImGui::Button(m_IsPlaying ? "Stop" : "Play"))
            SetPlaying(!m_IsPlaying);
        ImGui::SameLine();
        ImGui::Text("Mode: %s", m_IsPlaying ? "Play (scene copy)" : "Edit");
        ImGui::Checkbox("Free Camera (no Alt)", &m_FreeCam);
        ImGui::Text("Viewport: %.0f x %.0f", m_ViewportSize.x, m_ViewportSize.y);
        ImGui::Text("Cam: %.1f, %.1f, %.1f", m_EditorPos.x, m_EditorPos.y, m_EditorPos.z);
        ImGui::Separator();
        ImGui::TextWrapped("Camera: hold Left Alt (or toggle Free Camera with `). "
                           "LMB look, MMB pan, scroll dolly, WASD/QE fly. "
                           "Click a mesh (camera inactive) to select it. P toggles play.");
        ImGui::End();
    }

    // One-time default dock layout: Hierarchy left, Inspector/Stats right, Viewport center.
    void BuildDefaultDockLayout(ImGuiID dockspaceId) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

        ImGuiID center = dockspaceId;
        ImGuiID left   = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left,  0.18f, nullptr, &center);
        ImGuiID right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
        ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.5f, nullptr, &right);

        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Stats",     rightBottom);
        ImGui::DockBuilderDockWindow("Viewport",  center);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    // Resize the render target + camera to the Viewport panel size (measured last frame).
    void SyncViewportSize() {
        const uint32_t w = static_cast<uint32_t>(m_ViewportSize.x);
        const uint32_t h = static_cast<uint32_t>(m_ViewportSize.y);
        if (w == 0 || h == 0 || (w == m_LastViewportW && h == m_LastViewportH))
            return;

        m_LastViewportW = w;
        m_LastViewportH = h;

        Renderer::Get().OnResize(w, h);
        m_EditorCamera.SetViewportSize(w, h);

        if (auto registry = m_Scene ? m_Scene->GetEntityRegistry().lock() : nullptr) {
            auto camView = registry->view<CameraComponent>();
            for (auto&& [entity, cc] : camView.each())
                if (!cc.FixedAspect)
                    cc.Cam.SetViewportSize(w, h);
        }
    }

    // The camera is driven whenever Alt is held (momentary) or free-cam mode is on.
    bool CameraActive() const { return m_FreeCam || Input::IsKeyPressed(Key::LeftAlt); }

    // ---- Play / edit mode. Play runs on a deep copy so changes during play are
    //      discarded on Stop (the edit scene is restored untouched). ----
    void SetPlaying(bool play) {
        if (play == m_IsPlaying) return;
        m_Selected = entt::null;   // entity handles are registry-specific — reset on scene swap
        if (play) {
            m_RuntimeScene = DeepCopyScene(m_EditScene);
            m_Scene        = m_RuntimeScene ? m_RuntimeScene : m_EditScene;
        } else {
            m_Scene        = m_EditScene;   // restore the authoritative scene
            m_RuntimeScene = nullptr;       // discard play-time changes
        }
        m_IsPlaying = play;
        // The active scene changed size-relevant camera aspects only via SyncViewportSize;
        // force a resync next frame so the new registry's cameras get the viewport size.
        m_LastViewportW = m_LastViewportH = 0;
    }

    // Deep-copy every entity + its components into a fresh scene (editor-side, so the
    // engine's Scene stays untouched). Shared GPU refs (meshes/materials) are copied by
    // value, so the runtime scene renders immediately and edits to it stay isolated.
    static Ref<Scene> DeepCopyScene(const Ref<Scene>& src) {
        if (!src) return nullptr;
        Ref<Scene> dst = CreateRef<Scene>(src->GetName());

        auto srcReg = src->GetEntityRegistry().lock();
        auto dstReg = dst->GetEntityRegistry().lock();
        if (!srcReg || !dstReg) return dst;
        auto& s = *srcReg;
        auto& d = *dstReg;

        // entt views iterate newest-first; reverse to preserve creation order.
        std::vector<entt::entity> ents;
        { auto view = s.view<IDComponent>(); ents.assign(view.begin(), view.end()); }
        for (auto it = ents.rbegin(); it != ents.rend(); ++it) {
            const entt::entity e = *it;
            const UUID uuid = s.get<IDComponent>(e).ID;
            const std::string name = s.all_of<TagComponent>(e) ? s.get<TagComponent>(e).Tag
                                                               : std::string("Entity");
            Entity ne = dst->AddEntityWithUUID(uuid, name);   // adds ID/Tag/Transform/Relationship
            const entt::entity nh = static_cast<entt::entity>(ne);

            if (s.all_of<TransformComponent>(e))    d.get<TransformComponent>(nh)       = s.get<TransformComponent>(e);
            if (s.all_of<RelationshipComponent>(e)) d.get<RelationshipComponent>(nh)    = s.get<RelationshipComponent>(e);
            if (s.all_of<MeshComponent>(e))         d.emplace_or_replace<MeshComponent>(nh,     s.get<MeshComponent>(e));
            if (s.all_of<MaterialComponent>(e))     d.emplace_or_replace<MaterialComponent>(nh, s.get<MaterialComponent>(e));
            if (s.all_of<CameraComponent>(e))       d.emplace_or_replace<CameraComponent>(nh,   s.get<CameraComponent>(e));
            if (s.all_of<LightComponent>(e))        d.emplace_or_replace<LightComponent>(nh,    s.get<LightComponent>(e));
        }
        dst->MarkSceneGraphDirty();
        return dst;
    }

    // Read the object-id pass's target at a texel and select the entity there.
    void PickEntityAt(uint32_t x, uint32_t y) {
        auto* renderer = Renderer::Get().GetActive();
        if (!renderer) return;
        uint32_t raw = 0;
        if (!renderer->ReadTargetPixel(kIdResource, x, y, &raw, sizeof(raw)))
            return;
        // EntityID.slang writes (id + 1); the target is cleared to 0 = "nothing".
        m_Selected = (raw == 0u) ? entt::null : static_cast<entt::entity>(raw - 1u);
    }

    // The editor's object-id pass, expressed purely as generic pass-graph data and
    // injected via RendererAPI::SetAuxiliaryPipeline. A Graphics pass with a Shader
    // draws all scene geometry with that one shader (EntityID.slang) into an R32_UINT
    // target we clear to 0 and read back on click.
    static RenderPipelineDesc BuildIdPassDesc() {
        RenderPipelineDesc d;

        ResourceDesc id;
        id.Name       = kIdResource;
        id.Format     = TextureFormat::R32_UINT;
        id.SizePolicy = ResourceSizePolicy::SwapchainRelative;
        id.Scale      = 1.0f;
        d.Resources.push_back(id);

        ResourceDesc depth;
        depth.Name       = "editor_iddepth";
        depth.Format     = TextureFormat::D32_FLOAT;
        depth.SizePolicy = ResourceSizePolicy::SwapchainRelative;
        depth.Scale      = 1.0f;
        d.Resources.push_back(depth);

        PassDesc pass;
        pass.Name   = "editor_idpass";
        pass.Type   = PassType::Graphics;
        pass.Shader = "EntityID.slang";   // override shader: draws every mesh with the id shader

        AttachmentRef color;
        color.Resource   = kIdResource;
        color.Load       = LoadOp::Clear;
        color.Store      = StoreOp::Store;
        color.ColorClear = ClearColor{ 0.0f, 0.0f, 0.0f, 0.0f };   // integer clear → 0 = "no entity"
        pass.ColorOutputs.push_back(color);

        AttachmentRef dep;
        dep.Resource = "editor_iddepth";
        dep.Load     = LoadOp::Clear;
        dep.Store    = StoreOp::DontCare;
        pass.DepthOutput = dep;

        d.Passes.push_back(pass);
        return d;
    }

    static constexpr const char* kIdResource = "editor_entityid";

    Ref<Scene> m_Scene;         // active scene (edit scene, or the play-mode copy)
    Ref<Scene> m_EditScene;     // authoritative scene — never mutated during play
    Ref<Scene> m_RuntimeScene;  // throwaway deep copy used while playing

    // Play / edit mode  (P or the Stats button to toggle)
    bool m_IsPlaying = false;

    // Free-camera toggle (`) — control the camera without holding Alt.
    bool m_FreeCam = false;

    // Editor camera (not part of the scene ECS)
    Camera    m_EditorCamera;
    glm::vec3 m_EditorPos { 0.0f, 14.0f, 10.0f };
    glm::vec3 m_EditorRot { -52.0f, 0.0f, 0.0f }; // pitch, yaw, roll in degrees

    // Speed / sensitivity
    float m_MoveSpeed        = 10.0f;
    float m_LookSensitivity  = 0.08f;

    // Input state — accumulated in OnEvent, consumed in OnUpdate
    float m_LastMouseX  = 0.0f;
    float m_LastMouseY  = 0.0f;
    float m_MouseDeltaX = 0.0f;
    float m_MouseDeltaY = 0.0f;
    float m_ScrollDelta = 0.0f;
    float m_LastDeltaTime = 0.0f;

    // ImGui / viewport state
    ImVec2       m_ViewportSize    { 0.0f, 0.0f };
    bool         m_ViewportFocused = false;
    bool         m_ViewportHovered = false;
    uint32_t     m_LastViewportW   = 0;
    uint32_t     m_LastViewportH   = 0;
    bool         m_DockLayoutInit  = false;
    bool         m_ShowHierarchy   = true;
    bool         m_ShowInspector   = true;
    bool         m_ShowStats       = true;
    entt::entity m_Selected        { entt::null };
};
