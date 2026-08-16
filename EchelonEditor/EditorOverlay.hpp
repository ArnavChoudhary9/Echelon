#pragma once

#include "Echelon/Echelon.hpp"

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
            m_Scene = project->GetCurrentScene();
            if (!m_Scene && !project->GetConfig().StartScene.empty())
                m_Scene = project->OpenScene(project->GetConfig().StartScene);
            if (!m_Scene)
                m_Scene = project->NewScene("Editor Scene");
        } else {
            m_Scene = CreateRef<Scene>("Editor Scene");
        }

        auto* renderer = Renderer::Get().GetActive();
        renderer->SetVSync(false);

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
            addMesh("Sphere_Metal",    "Sphere",            "Materials/Metal.ehmaterial",    { -6, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Gold",     "Sphere",            "Materials/Gold.ehmaterial",     { -3, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Plastic",  "Sphere",            "Materials/Plastic.ehmaterial",  {  0, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Orange",   "Sphere",            "Materials/Orange.ehmaterial",   {  3, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Textured", "Sphere",            "Materials/Lit.ehmaterial",      {  6, 0.6f, -1 },     { 1.2f, 1.2f, 1.2f });
            addMesh("Cube",            "Cube",              "Materials/Metal.ehmaterial",    { -2.5f, 0.6f, 3 },   { 1.2f, 1.2f, 1.2f }, { 0, 25, 0 });
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
        auto project = Application::Get().GetProject();
        if (project)
            project->SaveScene();
        m_Scene = nullptr;
    }

    virtual void OnUpdate(float deltaTime) override {
        ECHELON_PROFILE_FUNCTION();

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
            // ---- Editor camera — all controls gated on Left Alt ----
            float dx = m_MouseDeltaX;
            float dy = m_MouseDeltaY;
            m_MouseDeltaX = 0.0f;
            m_MouseDeltaY = 0.0f;

            if (Input::IsKeyPressed(Key::LeftAlt)) {
                float speed    = m_MoveSpeed * (Input::IsKeyPressed(Key::LeftShift) ? 4.0f : 1.0f);
                float panSpeed = 0.015f       * (Input::IsKeyPressed(Key::LeftShift) ? 4.0f : 1.0f);

                // LMB + Alt → look (pan/tilt)
                if (Input::IsMouseButtonPressed(Mouse::ButtonLeft)) {
                    m_EditorRot.y -= dx * m_LookSensitivity;
                    m_EditorRot.x -= dy * m_LookSensitivity;
                    m_EditorRot.x  = glm::clamp(m_EditorRot.x, -89.0f, 89.0f);
                    m_EditorCamera.SetRotation(m_EditorRot);
                }

                // MMB + Alt → pan (truck / pedestal)
                if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle)) {
                    m_EditorPos -= m_EditorCamera.GetRight() * dx * panSpeed;
                    m_EditorPos += m_EditorCamera.GetUp()   * dy * panSpeed;
                }

                // Scroll + Alt → dolly (zoom)
                if (m_ScrollDelta != 0.0f) {
                    m_EditorPos += m_EditorCamera.GetForward() * m_ScrollDelta * m_MoveSpeed * 0.35f;
                    m_ScrollDelta = 0.0f;
                }

                // WASD + Alt → fly through scene (Q/E for world-up/down)
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

        // Accumulate mouse delta — only while Alt is held to avoid snap on press
        dispatcher.Dispatch<MouseMovedEvent>([this](MouseMovedEvent& e) {
            if (!m_IsPlaying && Input::IsKeyPressed(Key::LeftAlt)) {
                m_MouseDeltaX += e.GetX() - m_LastMouseX;
                m_MouseDeltaY += e.GetY() - m_LastMouseY;
            }
            m_LastMouseX = e.GetX();
            m_LastMouseY = e.GetY();
            return false;
        });

        // Accumulate scroll delta — only while Alt is held
        dispatcher.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e) {
            if (!m_IsPlaying && Input::IsKeyPressed(Key::LeftAlt))
                m_ScrollDelta += e.GetYOffset();
            return false;
        });

        // P key toggles play / edit mode
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) {
            if (e.GetKeyCode() == Key::P && e.GetRepeatCount() == 0)
                m_IsPlaying = !m_IsPlaying;
            return false;
        });

        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
            Renderer::Get().OnResize(e.GetWidth(), e.GetHeight());
            m_EditorCamera.SetViewportSize(e.GetWidth(), e.GetHeight());
            auto registry = m_Scene->GetEntityRegistry().lock();
            if (registry) {
                auto camView = registry->view<CameraComponent>();
                for (auto&& [entity, cc] : camView.each()) {
                    if (!cc.FixedAspect)
                        cc.Cam.SetViewportSize(e.GetWidth(), e.GetHeight());
                }
            }
            return false;
        });
    }

    virtual void OnImGUIBegin()  override {}
    virtual void OnImGUIRender() override {}
    virtual void OnImGUIEnd()    override {}

private:
    Ref<Scene> m_Scene;

    // Play / edit mode  (P to toggle)
    bool m_IsPlaying = false;

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
};
