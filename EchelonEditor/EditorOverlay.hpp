#pragma once

#include "Echelon/Echelon.hpp"

using namespace Echelon;

class EditorOverlay : public Overlay {
public:
    EditorOverlay() : Overlay() {}

    virtual ~EditorOverlay() {}

    virtual void OnAttach() override {
        auto& window = Application::Get().GetWindow();

        // ---- Load or create a scene from the project ----
        auto project = Application::Get().GetProject();
        if (project) {
            // Try to load the current scene from the project
            m_Scene = project->GetCurrentScene();

            // If no current scene, try to load the start scene
            if (!m_Scene && !project->GetConfig().StartScene.empty()) {
                m_Scene = project->OpenScene(project->GetConfig().StartScene);
            }

            // If still no scene, create a default one
            if (!m_Scene) {
                m_Scene = project->NewScene("Editor Scene");
            }
        } else {
            // Fallback: create a standalone scene if no project
            m_Scene = CreateRef<Scene>("Editor Scene");
        }

        auto* renderer = Renderer::Get().GetActive();
        renderer->SetVSync(false); // disable VSync for editor overlay

        // ---- Populate a demo scene if it is empty ----
        // Only renderer-independent structure is created here: entities reference
        // meshes by source (a built-in name or an asset path). The AssetManager
        // resolves them to GPU-ready meshes on demand and rebuilds those buffers
        // automatically on renderer hot-swap — the editor owns no GPU resources.
        auto registry = m_Scene->GetEntityRegistry().lock();
        bool hasCamera = false;
        if (registry) {
            auto camView = registry->view<CameraComponent>();
            hasCamera = !camView.empty();
        }

        if (!hasCamera) {
            // ---- Down-looking camera over the scene ----
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
                e.AddComponent<MeshComponent>().MeshSource     = meshSrc;
                e.AddComponent<MaterialComponent>().MaterialSource = material;
            };

            // ---- Ground plane + a row of PBR spheres + cube + monkey ----
            addMesh("Ground", "Plane", "Materials/Ground.ehmaterial", { 0, 0, 0 }, { 30, 1, 30 });
            addMesh("Sphere_Metal",   "Sphere", "Materials/Metal.ehmaterial",   { -6, 0.6f, -1 }, { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Gold",    "Sphere", "Materials/Gold.ehmaterial",    { -3, 0.6f, -1 }, { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Plastic", "Sphere", "Materials/Plastic.ehmaterial", {  0, 0.6f, -1 }, { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Orange",  "Sphere", "Materials/Orange.ehmaterial",  {  3, 0.6f, -1 }, { 1.2f, 1.2f, 1.2f });
            addMesh("Sphere_Textured","Sphere", "Materials/Lit.ehmaterial",     {  6, 0.6f, -1 }, { 1.2f, 1.2f, 1.2f });
            addMesh("Cube",   "Cube",            "Materials/Metal.ehmaterial",  { -2.5f, 0.6f, 3 }, { 1.2f, 1.2f, 1.2f }, { 0, 25, 0 });
            addMesh("Monkey", "Meshs/Monkey.obj","Materials/Orange.ehmaterial", {  2.5f, 1.0f, 3 }, { 1, 1, 1 },        { 0, -35, 0 });

            // ---- All three light types; sun/spot/point cast shadows ----
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
        if (project) {
            project->SaveScene();
        }

        m_Scene = nullptr;
        // The engine owns the renderer + asset lifetimes — do not release them here.
    }

    virtual void OnUpdate(float deltaTime) override {
        ECHELON_PROFILE_FUNCTION();

        // Rotate all mesh entities for the demo
        {
            auto registry = m_Scene->GetEntityRegistry().lock();
            if (registry) {
                auto meshView = registry->view<MeshComponent, TransformComponent, TagComponent>();
                for (auto&& [entity, mesh, tc, tag] : meshView.each()) {
                    if (tag.Tag == "Ground") continue;   // the plane stays put
                    tc.Rotation.y += 20.0f * deltaTime;   // gentle spin to show off shading
                }
            }
        }

        {
            ECHELON_PROFILE_SCOPE("Rendering Loop");
            auto* renderer = Renderer::Get().GetActive();
            if (!renderer) return;

            // Find the primary camera in the scene
            glm::mat4 viewMatrix(1.0f);
            glm::mat4 projMatrix(1.0f);

            {
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
            }

            {
                ECHELON_PROFILE_SCOPE("Render Scene");
                ClearValue clear;
                clear.Color = { 0.1f, 0.1f, 0.12f, 1.0f };

                renderer->BeginFrame(viewMatrix, projMatrix, clear);
                renderer->BeginScene(m_Scene);

                // Render all mesh entities in the scene via the render graph
                renderer->RenderScene(m_Scene);

                renderer->EndScene();
                renderer->EndFrame();
            }
        }
    }

    virtual void OnEvent(Event& event) override {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
            // Forward through the service so the cached size stays in sync for
            // any subsequent renderer swap.
            Renderer::Get().OnResize(e.GetWidth(), e.GetHeight());

            // Update camera viewport
            auto registry = m_Scene->GetEntityRegistry().lock();
            if (registry) {
                auto camView = registry->view<CameraComponent>();
                for (auto&& [entity, cc] : camView.each()) {
                    if (!cc.FixedAspect) {
                        cc.Cam.SetViewportSize(e.GetWidth(), e.GetHeight());
                    }
                }
            }

            return false;
        });
    }

    virtual void OnImGUIBegin() override {}
    virtual void OnImGUIRender() override {}
    virtual void OnImGUIEnd() override {}

private:
    Ref<Scene> m_Scene;
};
