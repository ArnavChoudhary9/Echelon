#pragma once

#include "Echelon/Echelon.hpp"

using namespace Echelon;

class EditorOverlay : public Overlay {
public:
    EditorOverlay() : Overlay() {}

    virtual ~EditorOverlay() {}

    virtual void OnAttach() override {
        // ---- Load and initialise the renderer plugin ----
        auto& window = Application::Get().GetWindow();

        if (!m_RendererLoader.Load()) {
            return;
        }

        auto* renderer = m_RendererLoader.Get();
        if (!renderer->Init(window.GetNativeHandle(),
                            window.GetWidth(),
                            window.GetHeight())) {
            return;
        }

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

        // ---- Setup default camera if scene is empty ----
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
            camTransform.Position = { 0.0f, 0.0f, 3.0f };

            auto& cam = cameraEntity.AddComponent<CameraComponent>();
            cam.Primary = true;
            cam.Cam.SetPerspective(60.0f, 0.1f, 1000.0f);
            cam.Cam.SetViewportSize(window.GetWidth(), window.GetHeight());
            cam.Cam.SetPosition(camTransform.Position);

            // ---- Triangle entity ----
            Entity triangleEntity = m_Scene->AddEntity("Triangle");

            // Build triangle mesh data: vec3 position + vec3 color
            float triangleVertices[] = {
                // positions          // colors
                 0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // top    (red)
                -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // left   (green)
                 0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // right  (blue)
            };

            BufferDesc vbDesc;
            vbDesc.Size        = sizeof(triangleVertices);
            vbDesc.Usage       = BufferUsage::VertexBuffer;
            vbDesc.Memory      = MemoryUsage::GPUOnly;
            vbDesc.InitialData = triangleVertices;
            vbDesc.DebugName   = "TriangleVB";

            auto vb = renderer->GetDevice()->CreateBuffer(vbDesc);

            // Attach MeshComponent to the entity
            auto& mesh        = triangleEntity.AddComponent<MeshComponent>();
            mesh.VertexBuffer = vb;
            mesh.VertexCount  = 3;
            mesh.MeshSource   = "Triangle";
            mesh.Invalidate();

            // Attach MaterialComponent (uses default flat pipeline from renderer)
            auto& mat        = triangleEntity.AddComponent<MaterialComponent>();
            mat.ShaderName   = "Flat";
            mat.PipelineRef  = renderer->GetDefaultPipeline();
            mat.AlbedoColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
            mat.Invalidate();
        }
    }

    virtual void OnDetach() override {
        auto project = Application::Get().GetProject();
        if (project) {
            project->SaveScene();
        }

        m_Scene = nullptr;
        m_RendererLoader.Unload();
    }

    virtual void OnUpdate(float deltaTime) override {
        ECHELON_PROFILE_FUNCTION();
        (void)deltaTime;
        
        // Update ECS

        {
            ECHELON_PROFILE_SCOPE("Rendering Loop");
            auto* renderer = m_RendererLoader.Get();
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
            auto* renderer = m_RendererLoader.Get();
            if (renderer) {
                renderer->OnResize(e.GetWidth(), e.GetHeight());
            }

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
    RendererLoader m_RendererLoader;
    Ref<Scene>     m_Scene;
};
