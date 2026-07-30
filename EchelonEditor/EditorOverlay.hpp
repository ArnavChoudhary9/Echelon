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
            // ---- Camera entity ----
            Entity cameraEntity = m_Scene->AddEntity("Camera");
            auto& camTransform = cameraEntity.GetComponent<TransformComponent>();
            camTransform.Position = { 0.0f, 0.0f, 5.0f };

            auto& cam = cameraEntity.AddComponent<CameraComponent>();
            cam.Primary = true;
            cam.Cam.SetPerspective(60.0f, 0.1f, 1000.0f);
            cam.Cam.SetViewportSize(window.GetWidth(), window.GetHeight());
            cam.Cam.SetPosition(camTransform.Position);

            // ---- Procedural built-in cube (from the internal shape repository) ----
            {
                Entity cube = m_Scene->AddEntity("Cube");
                cube.GetComponent<TransformComponent>().Position = { -1.2f, 0.0f, 0.0f };

                auto& mesh = cube.AddComponent<MeshComponent>();
                mesh.MeshSource = "Cube"; // resolved to the built-in primitive

                auto& mat = cube.AddComponent<MaterialComponent>();
                mat.ShaderName  = "Flat";
                mat.AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
            }

            // ---- OBJ-loaded monkey (exercises the .obj importer + registry) ----
            {
                Entity obj = m_Scene->AddEntity("Monkey");
                auto& t = obj.GetComponent<TransformComponent>();
                t.Position = { 1.2f, 0.0f, 0.0f };
                t.Scale    = { 0.7f, 0.7f, 0.7f };

                auto& mesh = obj.AddComponent<MeshComponent>();
                mesh.MeshSource = "Meshs/Monkey.obj"; // resolved via the OBJ importer

                auto& mat = obj.AddComponent<MaterialComponent>();
                mat.ShaderName  = "Flat";
                mat.AlbedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
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
                auto meshView = registry->view<MeshComponent, TransformComponent>();
                for (auto&& [entity, mesh, tc] : meshView.each()) {
                    tc.Rotation.y += 45.0f * deltaTime;
                    tc.Rotation.x += 20.0f * deltaTime;
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
