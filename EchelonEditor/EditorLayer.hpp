#pragma once

/**
 * @file EditorLayer.hpp
 * @brief Editor coordinator overlay: owns shared state, drives the scene render,
 *        hosts the dockspace, and translates bus commands into play-mode actions.
 *
 * The editor is split into independent Panel overlays (Toolbar/Viewport/Hierarchy/
 * Inspector/Stats). This layer is the one non-panel overlay: it owns the single
 * EditorContext, performs one-time setup (scene, renderer, aux id-pass, icons),
 * runs the editor camera + scene render each frame, builds the dockspace + menu,
 * and subscribes to the EventBus to execute transport commands and track selection.
 * Panels are pushed onto the layer stack in OnAttach and communicate only via the
 * shared context + the bus — never with each other directly.
 */

#include "Echelon/Echelon.hpp"
#include "Echelon/Asset/RenderPipeline/RenderPipelineDesc.hpp"

#include "EditorContext.hpp"
#include "Panels/ToolbarPanel.hpp"
#include "Panels/ViewportPanel.hpp"
#include "Panels/HierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/StatsPanel.hpp"
#include "Panels/ContentBrowserPanel.hpp"

#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder* for the first-run default layout

// stb_image: declarations only (implementation is compiled into libEchelon).
#include "stb_image.h"

#include <entt/entt.hpp>
#include <vector>
#include <string>
#include <filesystem>

using namespace Echelon;

class EditorLayer : public Overlay {
public:
    EditorLayer() : Overlay() {}
    virtual ~EditorLayer() {}

    virtual void OnAttach() override {
        m_Ctx = CreateRef<EditorContext>();

        auto& window = Application::Get().GetWindow();

        // ---- Fonts (must be loaded before the first BeginFrame builds the atlas) ----
        {
            ImGuiIO& io = ImGui::GetIO();
            io.Fonts->AddFontFromFileTTF("EditorResources/Fonts/opensans/OpenSans-Bold.ttf", 28.0f);
            io.FontDefault = io.Fonts->AddFontFromFileTTF(
                "EditorResources/Fonts/opensans/OpenSans-Regular.ttf", 28.0f);
        }

        // ---- Load or create scene ----
        auto project = Application::Get().GetProject();
        if (project) {
            m_Ctx->EditScene = project->GetCurrentScene();
            if (!m_Ctx->EditScene && !project->GetConfig().StartScene.empty())
                m_Ctx->EditScene = project->OpenScene(project->GetConfig().StartScene);
            if (!m_Ctx->EditScene)
                m_Ctx->EditScene = project->NewScene("Editor Scene");
        } else {
            m_Ctx->EditScene = CreateRef<Scene>("Editor Scene");
        }
        m_Ctx->ActiveScene = m_Ctx->EditScene;

        auto* renderer = Renderer::Get().GetActive();
        renderer->SetVSync(false);
        renderer->SetViewportTarget(true);           // render into an offscreen texture
        renderer->SetAuxiliaryPipeline(BuildIdPassDesc());   // object-id pass for picking

        // ---- Editor camera ----
        m_Ctx->EditorCamera.SetPerspective(60.0f, 0.1f, 1000.0f);
        m_Ctx->EditorCamera.SetViewportSize(window.GetWidth(), window.GetHeight());
        m_Ctx->EditorCamera.SetPosition(m_Ctx->EditorPos);
        m_Ctx->EditorCamera.SetRotation(m_Ctx->EditorRot);

        PopulateDemoSceneIfEmpty(window);

        // ---- Toolbar icons ----
        LoadIconSet(m_Ctx->DarkIcons,  "dark");
        LoadIconSet(m_Ctx->LightIcons, "light");

        // ---- Content-browser icons ----
        m_Ctx->DirIcon  = LoadIcon("EditorResources/Icons/DirectoryIcon.png");
        m_Ctx->FileIcon = LoadIcon("EditorResources/Icons/FileIcon.png");

        // ---- Bus subscriptions (RAII: auto-unsubscribe on destruction) ----
        m_CmdSub = OnMessage<EditorCommandEvent>([this](const EditorCommandEvent& e) {
            OnCommand(e.Action);
        });
        m_SelSub = OnMessage<EntitySelectedEvent>([this](const EntitySelectedEvent& e) {
            m_Ctx->Selection = e.Entity;
        });
        m_SceneLoadSub = OnMessage<SceneLoadRequestedEvent>([this](const SceneLoadRequestedEvent& e) {
            OnLoadScene(e.Path);
        });
        m_MeshSpawnSub = OnMessage<MeshSpawnRequestedEvent>([this](const MeshSpawnRequestedEvent& e) {
            OnSpawnMesh(e.MeshSource);
        });

        // ---- Spawn the panels as overlays (they share m_Ctx, talk via the bus) ----
        m_Toolbar        = CreateRef<ToolbarPanel>(m_Ctx);
        m_Viewport       = CreateRef<ViewportPanel>(m_Ctx);
        m_Hierarchy      = CreateRef<HierarchyPanel>(m_Ctx);
        m_Inspector      = CreateRef<InspectorPanel>(m_Ctx);
        m_Stats          = CreateRef<StatsPanel>(m_Ctx);
        m_ContentBrowser = CreateRef<ContentBrowserPanel>(m_Ctx);
        auto& app = Application::Get();
        app.PushOverlay(m_Viewport);
        app.PushOverlay(m_Hierarchy);
        app.PushOverlay(m_Inspector);
        app.PushOverlay(m_Stats);
        app.PushOverlay(m_ContentBrowser);
        app.PushOverlay(m_Toolbar);
    }

    virtual void OnDetach() override {
        // Saving on exit is NOT automatic: the exit-confirmation modal (see
        // DrawExitModal) is responsible for persisting the scene when the user asks.
        // Closing without saving must discard changes, so nothing is written here.
        m_CmdSub.Reset();
        m_SelSub.Reset();
        m_SceneLoadSub.Reset();
        m_MeshSpawnSub.Reset();
        m_Ctx = nullptr;
    }

    virtual void OnUpdate(float deltaTime) override {
        ECHELON_PROFILE_FUNCTION();
        m_Ctx->LastDeltaTime = deltaTime;

        SyncViewportSize();

        if (m_Ctx->IsPlaying && (!m_Ctx->IsPaused || m_Ctx->StepOneFrame)) {
            // Animate entities in play mode (also runs one frame when stepping).
            auto registry = m_Ctx->ActiveScene->GetEntityRegistry().lock();
            if (registry) {
                auto meshView = registry->view<MeshComponent, TransformComponent, TagComponent>();
                for (auto&& [entity, mesh, tc, tag] : meshView.each()) {
                    if (tag.Tag == "Ground") continue;
                    tc.Rotation.y += 20.0f * deltaTime;
                }
            }
            if (m_Ctx->IsPaused) m_Ctx->StepOneFrame = false;   // consumed
        } else if (!m_Ctx->IsPlaying) {
            UpdateEditorCamera(deltaTime);
        }

        RenderScene();
    }

    virtual void OnEvent(Event& event) override {
        EventDispatcher dispatcher(event);

        // Intercept the window-close request: veto the immediate shutdown (return
        // true = Handled) and raise the confirm modal instead. The app cancels the
        // OS close when it sees the event was handled (Application::OnWindowClose).
        dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&) {
            m_ExitRequested = true;
            return true;
        });

        // Accumulate mouse delta whenever the camera is active (Alt or free-cam),
        // regardless of which panel is hovered. Edit mode only.
        dispatcher.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& e) {
            if (!m_Ctx->IsPlaying && m_Ctx->CameraActive()) {
                m_MouseDeltaX += e.GetX() - m_LastMouseX;
                m_MouseDeltaY += e.GetY() - m_LastMouseY;
            }
            m_LastMouseX = e.GetX();
            m_LastMouseY = e.GetY();
            return false;
        });

        dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e) {
            if (!m_Ctx->IsPlaying && m_Ctx->CameraActive())
                m_ScrollDelta += e.GetYOffset();
            return false;
        });

        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
            if (e.GetRepeatCount() != 0) return false;
            const bool ctrl = Input::IsKeyPressed(Key::LeftControl) ||
                              Input::IsKeyPressed(Key::RightControl);
            // Ctrl+S saves the active scene (matches the File-menu accelerator).
            if (ctrl && e.GetKeyCode() == Key::S) {
                if (auto project = Application::Get().GetProject()) project->SaveScene();
                return false;
            }
            // P toggles play/edit — viewport must be focused so it does not fire while
            // typing in another panel. Routed through the bus like the toolbar.
            if (m_Ctx->ViewportFocused && e.GetKeyCode() == Key::P)
                PublishEvent(EditorCommandEvent{ m_Ctx->IsPlaying ? EditorAction::Stop
                                                                  : EditorAction::Play });
            // ` (grave) toggles free-camera mode — gated on viewport focus (like P) so
            // it does not fire while typing a `` ` `` into a text field.
            if (m_Ctx->ViewportFocused && e.GetKeyCode() == Key::GraveAccent)
                m_Ctx->FreeCam = !m_Ctx->FreeCam;
            return false;
        });
    }

    // ---- Dockspace host + menu bar (runs before any panel's OnImGUIRender) ----
    virtual void OnImGUIBegin() override {
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
                if (ImGui::MenuItem("Exit")) m_ExitRequested = true;   // routed through the confirm modal
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (m_Hierarchy)      ImGui::MenuItem("Hierarchy",        nullptr, m_Hierarchy->OpenPtr());
                if (m_Inspector)      ImGui::MenuItem("Inspector",        nullptr, m_Inspector->OpenPtr());
                if (m_Stats)          ImGui::MenuItem("Stats",            nullptr, m_Stats->OpenPtr());
                if (m_ContentBrowser) ImGui::MenuItem("Content Browser",  nullptr, m_ContentBrowser->OpenPtr());
                if (m_Toolbar)        ImGui::MenuItem("Toolbar",          nullptr, m_Toolbar->OpenPtr());
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        const ImGuiID dockspaceId = ImGui::GetID("EchelonDockSpace");

        // Build a default layout the first time — only if nothing was restored from
        // imgui.ini. Must run BEFORE DockSpace(), which itself creates the node.
        if (!m_DockLayoutInit) {
            m_DockLayoutInit = true;
            if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
                BuildDefaultDockLayout(dockspaceId);
        }

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();
    }

    virtual void OnImGUIRender() override { DrawExitModal(); }
    virtual void OnImGUIEnd()    override {}

private:
    // ------------------------------------------------------------------
    // Exit confirmation (replaces the old silent save-on-exit)
    // ------------------------------------------------------------------
    void DrawExitModal() {
        static constexpr const char* kPopup = "Exit Echelon?";
        if (!m_ExitRequested) return;

        if (!ImGui::IsPopupOpen(kPopup))
            ImGui::OpenPopup(kPopup);

        const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(kPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Save changes to the scene before exiting?");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            auto doClose = [] { Application::Get().Close(); };

            if (ImGui::Button("Save & Exit", ImVec2(150, 0))) {
                if (auto project = Application::Get().GetProject()) project->SaveScene();
                m_ExitRequested = false;
                ImGui::CloseCurrentPopup();
                doClose();
            }
            ImGui::SameLine();
            if (ImGui::Button("Exit without Saving", ImVec2(190, 0))) {
                m_ExitRequested = false;
                ImGui::CloseCurrentPopup();
                doClose();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_ExitRequested = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // ------------------------------------------------------------------
    // Transport (bus command handler)
    // ------------------------------------------------------------------
    void OnCommand(EditorAction action) {
        switch (action) {
            case EditorAction::Play:    SetPlaying(true);            break;
            case EditorAction::Stop:    SetPlaying(false);           break;
            case EditorAction::Pause:   SetPaused(true);             break;
            case EditorAction::Resume:  SetPaused(false);            break;
            case EditorAction::Restart: SetPlaying(false); SetPlaying(true); break;
            case EditorAction::Step:    if (m_Ctx->IsPaused) m_Ctx->StepOneFrame = true; break;
        }
    }

    void OnLoadScene(const std::string& path) {
        if (m_Ctx->IsPlaying) return;
        auto project = Application::Get().GetProject();
        if (!project) return;
        auto scene = project->OpenScene(path);
        if (!scene) {
            ECHELON_LOG_WARN("[Editor] Failed to load dropped scene: {}", path);
            return;
        }
        m_Ctx->EditScene   = scene;
        m_Ctx->ActiveScene = scene;
        m_Ctx->Selection   = entt::null;
        m_LastViewportW = m_LastViewportH = 0;
    }

    void OnSpawnMesh(const std::string& meshSource) {
        if (!m_Ctx->EditScene || m_Ctx->IsPlaying) return;

        // Derive a display name: last path component without extension.
        std::string stem = meshSource;
        const auto slash = stem.find_last_of("/\\");
        if (slash != std::string::npos) stem = stem.substr(slash + 1);
        const auto dot = stem.rfind('.');
        if (dot != std::string::npos) stem = stem.substr(0, dot);
        if (stem.empty()) stem = "Mesh";

        Entity e = m_Ctx->EditScene->AddEntity(stem);
        e.AddComponent<MeshComponent>().MeshSource = meshSource;
        auto& mat = e.AddComponent<MaterialComponent>();

        // Auto-detect a .ehmaterial with the same base name adjacent to the mesh file.
        if (auto project = Application::Get().GetProject()) {
            const std::string dir   = [&] {
                const auto s = meshSource.find_last_of("/\\");
                return s != std::string::npos ? meshSource.substr(0, s + 1) : std::string();
            }();
            const std::string guess = dir + stem + ".ehmaterial";
            const fs::path absGuess = project->GetRootDirectory() / "Assets" / guess;
            if (std::filesystem::exists(absGuess))
                mat.MaterialSource = guess;
        }

        const entt::entity handle = static_cast<entt::entity>(e);
        m_Ctx->Selection = handle;
        PublishEvent(EntitySelectedEvent{ handle });
    }

    // Play runs on a deep copy so play-time changes are discarded on Stop.
    void SetPlaying(bool play) {
        if (play == m_Ctx->IsPlaying) return;
        m_Ctx->IsPaused     = false;
        m_Ctx->StepOneFrame = false;
        m_Ctx->Selection    = entt::null;   // handles are registry-specific
        if (play) {
            m_Ctx->RuntimeScene = DeepCopyScene(m_Ctx->EditScene);
            m_Ctx->ActiveScene  = m_Ctx->RuntimeScene ? m_Ctx->RuntimeScene : m_Ctx->EditScene;
        } else {
            m_Ctx->ActiveScene  = m_Ctx->EditScene;   // restore authoritative scene
            m_Ctx->RuntimeScene = nullptr;            // discard play-time changes
        }
        m_Ctx->IsPlaying = play;
        m_LastViewportW = m_LastViewportH = 0;   // force camera resync for new registry
        PublishEvent(PlayStateChangedEvent{ m_Ctx->IsPlaying, m_Ctx->IsPaused });
    }

    void SetPaused(bool paused) {
        if (!m_Ctx->IsPlaying) return;
        m_Ctx->IsPaused     = paused;
        m_Ctx->StepOneFrame = false;
        PublishEvent(PlayStateChangedEvent{ m_Ctx->IsPlaying, m_Ctx->IsPaused });
    }

    // ------------------------------------------------------------------
    // Per-frame camera + render
    // ------------------------------------------------------------------
    void UpdateEditorCamera(float deltaTime) {
        float dx = m_MouseDeltaX;
        float dy = m_MouseDeltaY;
        m_MouseDeltaX = 0.0f;
        m_MouseDeltaY = 0.0f;

        if (m_Ctx->CameraActive()) {
            auto& cam = m_Ctx->EditorCamera;
            const float panSpeed = 0.015f * (Input::IsKeyPressed(Key::LeftShift) ? 4.0f : 1.0f);

            if (Input::IsMouseButtonPressed(Mouse::ButtonLeft)) {
                m_Ctx->EditorRot.y -= dx * m_Ctx->LookSensitivity;
                m_Ctx->EditorRot.x -= dy * m_Ctx->LookSensitivity;
                m_Ctx->EditorRot.x  = glm::clamp(m_Ctx->EditorRot.x, -89.0f, 89.0f);
                cam.SetRotation(m_Ctx->EditorRot);
            }
            if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle)) {
                m_Ctx->EditorPos -= cam.GetRight() * dx * panSpeed;
                m_Ctx->EditorPos += cam.GetUp()    * dy * panSpeed;
            }
            if (m_ScrollDelta != 0.0f)
                m_Ctx->EditorPos += cam.GetForward() * m_ScrollDelta * m_Ctx->MoveSpeed * 0.35f;

            const float speed = m_Ctx->MoveSpeed * (Input::IsKeyPressed(Key::LeftShift) ? 4.0f : 1.0f);
            glm::vec3 move(0.0f);
            if (Input::IsKeyPressed(Key::W)) move += cam.GetForward();
            if (Input::IsKeyPressed(Key::S)) move -= cam.GetForward();
            if (Input::IsKeyPressed(Key::A)) move -= cam.GetRight();
            if (Input::IsKeyPressed(Key::D)) move += cam.GetRight();
            if (Input::IsKeyPressed(Key::E)) move.y += 1.0f;
            if (Input::IsKeyPressed(Key::Q)) move.y -= 1.0f;
            if (glm::length(move) > 0.001f)
                m_Ctx->EditorPos += glm::normalize(move) * speed * deltaTime;
        }
        m_ScrollDelta = 0.0f;
        m_Ctx->EditorCamera.SetPosition(m_Ctx->EditorPos);
    }

    void RenderScene() {
        ECHELON_PROFILE_SCOPE("Rendering Loop");
        auto* renderer = Renderer::Get().GetActive();
        if (!renderer) return;

        glm::mat4 viewMatrix(1.0f);
        glm::mat4 projMatrix(1.0f);

        if (m_Ctx->IsPlaying) {
            auto registry = m_Ctx->ActiveScene->GetEntityRegistry().lock();
            if (registry) {
                auto camView = registry->view<CameraComponent, TransformComponent>();
                for (auto&& [entity, cc, tc] : camView.each()) {
                    if (cc.Primary) {
                        // TransformComponent is LOCAL — resolve the camera's world TRS so
                        // a camera parented under a moving pivot renders from the right
                        // viewpoint (a root camera decomposes back to its own transform).
                        glm::vec3 wpos, wrot, wscale;
                        Entity camEntity = m_Ctx->ActiveScene->FindEntityByUUID(
                            registry->get<IDComponent>(entity).ID);
                        if (m_Ctx->ActiveScene->GetWorldTRS(camEntity, wpos, wrot, wscale)) {
                            cc.Cam.SetPosition(wpos);
                            cc.Cam.SetRotation(wrot);
                        } else {
                            cc.Cam.SetPosition(tc.Position);
                            cc.Cam.SetRotation(tc.Rotation);
                        }
                        viewMatrix = cc.Cam.GetViewMatrix();
                        projMatrix = cc.Cam.GetProjectionMatrix();
                        break;
                    }
                }
            }
        } else {
            viewMatrix = m_Ctx->EditorCamera.GetViewMatrix();
            projMatrix = m_Ctx->EditorCamera.GetProjectionMatrix();
        }

        ClearValue clear;
        clear.Color = { 0.1f, 0.1f, 0.12f, 1.0f };
        renderer->BeginFrame(viewMatrix, projMatrix, clear);
        renderer->BeginScene(m_Ctx->ActiveScene);
        renderer->RenderScene(m_Ctx->ActiveScene);
        renderer->EndScene();
        renderer->EndFrame();
    }

    // Resize the render target + cameras to the Viewport panel size (from last frame).
    void SyncViewportSize() {
        const uint32_t w = static_cast<uint32_t>(m_Ctx->ViewportSize.x);
        const uint32_t h = static_cast<uint32_t>(m_Ctx->ViewportSize.y);
        if (w == 0 || h == 0 || (w == m_LastViewportW && h == m_LastViewportH))
            return;

        m_LastViewportW = w;
        m_LastViewportH = h;

        Renderer::Get().OnResize(w, h);
        m_Ctx->EditorCamera.SetViewportSize(w, h);

        if (auto registry = m_Ctx->ActiveScene ? m_Ctx->ActiveScene->GetEntityRegistry().lock() : nullptr) {
            auto camView = registry->view<CameraComponent>();
            for (auto&& [entity, cc] : camView.each())
                if (!cc.FixedAspect)
                    cc.Cam.SetViewportSize(w, h);
        }
    }

    // One-time default dock layout: Hierarchy left, Inspector/Stats right, Viewport center.
    void BuildDefaultDockLayout(ImGuiID dockspaceId) {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

        ImGuiID center = dockspaceId;
        ImGuiID left   = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left,  0.18f, nullptr, &center);
        ImGuiID right  = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down,  0.28f, nullptr, &center);
        ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.5f, nullptr, &right);

        ImGui::DockBuilderDockWindow("Hierarchy",       left);
        ImGui::DockBuilderDockWindow("Inspector",       right);
        ImGui::DockBuilderDockWindow("Stats",           rightBottom);
        ImGui::DockBuilderDockWindow("Content Browser", bottom);
        ImGui::DockBuilderDockWindow("Viewport",        center);
        // Toolbar is intentionally left floating (on top); the user can dock it.
        ImGui::DockBuilderFinish(dockspaceId);
    }

    // ------------------------------------------------------------------
    // One-time setup helpers
    // ------------------------------------------------------------------
    void PopulateDemoSceneIfEmpty(Window& window) {
        auto registry = m_Ctx->ActiveScene->GetEntityRegistry().lock();
        bool hasCamera = false;
        if (registry) {
            auto camView = registry->view<CameraComponent>();
            hasCamera = !camView.empty();
        }
        if (hasCamera) return;

        Scene& scene = *m_Ctx->ActiveScene;
        Entity cameraEntity = scene.AddEntity("Camera");
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
            Entity e = scene.AddEntity(tag);
            auto& t = e.GetComponent<TransformComponent>();
            t.Position = pos; t.Scale = scale; t.Rotation = rot;
            e.AddComponent<MeshComponent>().MeshSource         = meshSrc;
            e.AddComponent<MaterialComponent>().MaterialSource = material;
        };

        addMesh("Ground",         "Plane",            "Materials/Ground.ehmaterial",  { 0, 0, 0 },       { 30, 1, 30 });
        addMesh("Sphere_Metal",   "Sphere",           "Materials/Metal.ehmaterial",   { -6, 0.6f, -1 },  { 1.2f, 1.2f, 1.2f });
        addMesh("Sphere_Gold",    "Sphere",           "Materials/Gold.ehmaterial",    { -3, 0.6f, -1 },  { 1.2f, 1.2f, 1.2f });
        addMesh("Sphere_Plastic", "Sphere",           "Materials/Plastic.ehmaterial", {  0, 0.6f, -1 },  { 1.2f, 1.2f, 1.2f });
        addMesh("Sphere_Orange",  "Sphere",           "Materials/Orange.ehmaterial",  {  3, 0.6f, -1 },  { 1.2f, 1.2f, 1.2f });
        addMesh("Sphere_Rough",   "Sphere",           "Materials/Rough.ehmaterial",   {  6, 0.6f, -1 },  { 1.2f, 1.2f, 1.2f });
        addMesh("Cube",           "Cube",             "Materials/Lit.ehmaterial",     { -2.5f, 0.6f, 3 },{ 1.2f, 1.2f, 1.2f }, { 0, 25, 0 });
        addMesh("Monkey",         "Meshes/Monkey.obj","Materials/Orange.ehmaterial",  {  2.5f, 1.0f, 3 },{ 1, 1, 1 },          { 0, -35, 0 });

        {
            Entity sun = scene.AddEntity("Sun");
            sun.GetComponent<TransformComponent>().Rotation = { -50.0f, -35.0f, 0.0f };
            auto& l = sun.AddComponent<LightComponent>();
            l.Type = LightType::Directional; l.Color = { 1.0f, 0.96f, 0.9f };
            l.Intensity = 2.5f; l.Range = 40.0f; l.CastsShadows = true;
        }
        {
            Entity spot = scene.AddEntity("Spot");
            auto& t = spot.GetComponent<TransformComponent>();
            t.Position = { 0.0f, 9.0f, 3.0f }; t.Rotation = { -72.0f, 0.0f, 0.0f };
            auto& l = spot.AddComponent<LightComponent>();
            l.Type = LightType::Spot; l.Color = { 0.3f, 0.7f, 1.0f };
            l.Intensity = 60.0f; l.Range = 25.0f; l.InnerAngle = 18.0f; l.OuterAngle = 30.0f;
            l.CastsShadows = true;
        }
        {
            Entity pt = scene.AddEntity("PointWarm");
            pt.GetComponent<TransformComponent>().Position = { -5.0f, 3.0f, 2.0f };
            auto& l = pt.AddComponent<LightComponent>();
            l.Type = LightType::Point; l.Color = { 1.0f, 0.5f, 0.2f };
            l.Intensity = 40.0f; l.Range = 18.0f; l.CastsShadows = true; l.ShadowBias = 0.05f;
        }
        {
            Entity pt = scene.AddEntity("PointFill");
            pt.GetComponent<TransformComponent>().Position = { 6.0f, 3.0f, -4.0f };
            auto& l = pt.AddComponent<LightComponent>();
            l.Type = LightType::Point; l.Color = { 0.4f, 0.5f, 1.0f };
            l.Intensity = 20.0f; l.Range = 15.0f; l.CastsShadows = false;
        }
    }

    // Deep-copy every entity + components into a fresh scene (editor-side). Shared GPU
    // refs (meshes/materials) are copied by value so play-mode edits stay isolated.
    static Ref<Scene> DeepCopyScene(const Ref<Scene>& src) {
        if (!src) return nullptr;
        Ref<Scene> dst = CreateRef<Scene>(src->GetName());

        auto srcReg = src->GetEntityRegistry().lock();
        auto dstReg = dst->GetEntityRegistry().lock();
        if (!srcReg || !dstReg) return dst;
        auto& s = *srcReg;
        auto& d = *dstReg;

        std::vector<entt::entity> ents;
        { auto view = s.view<IDComponent>(); ents.assign(view.begin(), view.end()); }
        for (auto it = ents.rbegin(); it != ents.rend(); ++it) {
            const entt::entity e = *it;
            const UUID uuid = s.get<IDComponent>(e).ID;
            const std::string name = s.all_of<TagComponent>(e) ? s.get<TagComponent>(e).Tag
                                                               : std::string("Entity");
            Entity ne = dst->AddEntityWithUUID(uuid, name);
            const entt::entity nh = static_cast<entt::entity>(ne);

            if (s.all_of<TransformComponent>(e))    d.get<TransformComponent>(nh)          = s.get<TransformComponent>(e);
            if (s.all_of<RelationshipComponent>(e)) d.get<RelationshipComponent>(nh)       = s.get<RelationshipComponent>(e);
            if (s.all_of<MeshComponent>(e))         d.emplace_or_replace<MeshComponent>(nh,     s.get<MeshComponent>(e));
            if (s.all_of<MaterialComponent>(e))     d.emplace_or_replace<MaterialComponent>(nh, s.get<MaterialComponent>(e));
            if (s.all_of<CameraComponent>(e))       d.emplace_or_replace<CameraComponent>(nh,   s.get<CameraComponent>(e));
            if (s.all_of<LightComponent>(e))        d.emplace_or_replace<LightComponent>(nh,    s.get<LightComponent>(e));
        }
        dst->MarkSceneGraphDirty();
        return dst;
    }

    // Load a PNG from disk and upload it to the GPU as a TextureAsset.
    static Ref<TextureAsset> LoadIcon(const char* path) {
        int w, h, ch;
        unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
        if (!data) {
            ECHELON_LOG_WARN("[Editor] Icon not found: {}", path);
            return nullptr;
        }
        std::vector<uint8_t> pixels(data, data + w * h * 4);
        stbi_image_free(data);
        auto asset = CreateRef<TextureAsset>();
        asset->SetData(std::move(pixels), static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                       TextureFormat::RGBA8_UNORM, false);
        asset->UploadGPU(Renderer::Get().GetActive());
        return asset;
    }

    static void LoadIconSet(EditorIcons& icons, const char* suffix) {
        auto p = [&](const char* name) {
            return std::string("EditorResources/Icons/") + name + "_" + suffix + ".png";
        };
        icons.Play    = LoadIcon(p("play")   .c_str());
        icons.Pause   = LoadIcon(p("pause")  .c_str());
        icons.Stop    = LoadIcon(p("stop")   .c_str());
        icons.Restart = LoadIcon(p("restart").c_str());
        icons.Step    = LoadIcon(p("step")   .c_str());
    }

    // The editor's object-id pass as generic pass-graph data (injected via
    // RendererAPI::SetAuxiliaryPipeline). Draws all geometry with EntityID.slang
    // into an R32_UINT target cleared to 0, read back on click for picking.
    static RenderPipelineDesc BuildIdPassDesc() {
        RenderPipelineDesc d;

        ResourceDesc id;
        id.Name = kEditorIdResource; id.Format = TextureFormat::R32_UINT;
        id.SizePolicy = ResourceSizePolicy::SwapchainRelative; id.Scale = 1.0f;
        d.Resources.push_back(id);

        ResourceDesc depth;
        depth.Name = "editor_iddepth"; depth.Format = TextureFormat::D32_FLOAT;
        depth.SizePolicy = ResourceSizePolicy::SwapchainRelative; depth.Scale = 1.0f;
        d.Resources.push_back(depth);

        PassDesc pass;
        pass.Name   = "editor_idpass";
        pass.Type   = PassType::Graphics;
        pass.Shader = "EntityID.slang";

        AttachmentRef color;
        color.Resource   = kEditorIdResource;
        color.Load       = LoadOp::Clear;
        color.Store      = StoreOp::Store;
        color.ColorClear = ClearColor{ 0.0f, 0.0f, 0.0f, 0.0f };
        pass.ColorOutputs.push_back(color);

        AttachmentRef dep;
        dep.Resource = "editor_iddepth";
        dep.Load     = LoadOp::Clear;
        dep.Store    = StoreOp::DontCare;
        pass.DepthOutput = dep;

        d.Passes.push_back(pass);
        return d;
    }

    Ref<EditorContext> m_Ctx;

    // Panels (owned via the layer stack; kept here for the View menu toggles).
    Ref<ToolbarPanel>        m_Toolbar;
    Ref<ViewportPanel>       m_Viewport;
    Ref<HierarchyPanel>      m_Hierarchy;
    Ref<InspectorPanel>      m_Inspector;
    Ref<StatsPanel>          m_Stats;
    Ref<ContentBrowserPanel> m_ContentBrowser;

    // Bus subscriptions (RAII).
    ScopedSubscription m_CmdSub;
    ScopedSubscription m_SelSub;
    ScopedSubscription m_SceneLoadSub;
    ScopedSubscription m_MeshSpawnSub;

    // Input accumulation (OnEvent → OnUpdate).
    float m_LastMouseX  = 0.0f;
    float m_LastMouseY  = 0.0f;
    float m_MouseDeltaX = 0.0f;
    float m_MouseDeltaY = 0.0f;
    float m_ScrollDelta = 0.0f;

    // Viewport resize tracking.
    uint32_t m_LastViewportW = 0;
    uint32_t m_LastViewportH = 0;
    bool     m_DockLayoutInit = false;

    // Exit-confirmation modal state (set by the menu "Exit" or a window-close veto).
    bool     m_ExitRequested = false;
};
