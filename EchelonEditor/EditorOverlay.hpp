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

        // ---- Setup default camera + demo triangle if scene is empty ----
        // Only the renderer-independent structure is created here; the GPU
        // resources (vertex buffer, pipeline) are built in the renderer-change
        // listener below so they load in one place and rebuild on hot-swap.
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

            // ---- Triangle entity (structure only; GPU data built on renderer change) ----
            Entity triangleEntity = m_Scene->AddEntity("Triangle");

            auto& mesh       = triangleEntity.AddComponent<MeshComponent>();
            mesh.VertexCount = 3;
            mesh.MeshSource  = "Triangle";

            auto& mat        = triangleEntity.AddComponent<MaterialComponent>();
            mat.ShaderName   = "Flat";
            mat.AlbedoColor  = { 1.0f, 1.0f, 1.0f, 1.0f };
        }

        // ---- Register the renderer-change listener ----
        // Fires immediately for the currently-active renderer (replay-on-subscribe)
        // and again on every hot-swap, so all GPU resources are (re)built in one place.
        m_RendererChangedListener = Renderer::Get().AddChangeListener(
            [this](RendererAPI* renderer) { BuildSceneGPUResources(renderer); });
    }

    virtual void OnDetach() override {
        Renderer::Get().RemoveChangeListener(m_RendererChangedListener);

        auto project = Application::Get().GetProject();
        if (project) {
            project->SaveScene();
        }

        m_Scene = nullptr;
        // The engine owns the renderer lifetime — do not unload it here.
    }

    virtual void OnUpdate(float deltaTime) override {
        ECHELON_PROFILE_FUNCTION();
        (void)deltaTime;

        // Update ECS

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
    /**
     * @brief (Re)build all GPU-backed scene resources for the given renderer.
     *
     * Called once at startup and again on every renderer hot-swap. GPU handles
     * created by a previous renderer are invalid for the new one, so vertex
     * buffers are recreated from each mesh's source and pipelines are reassigned
     * from the active renderer's default pipeline.
     */
    void BuildSceneGPUResources(RendererAPI* renderer) {
        if (!renderer || !m_Scene) return;

        auto device          = renderer->GetDevice();
        auto defaultPipeline = renderer->GetDefaultPipeline();
        if (!device) return;

        auto registry = m_Scene->GetEntityRegistry().lock();
        if (!registry) return;

        auto view = registry->view<MeshComponent, MaterialComponent>();
        for (auto&& [entity, mesh, mat] : view.each()) {
            // Reconstruct the vertex buffer from the mesh source tag.
            if (mesh.MeshSource == "Triangle") {
                // positions (vec3) + colors (vec3)
                float triangleVertices[] = {
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

                mesh.VertexBuffer = device->CreateBuffer(vbDesc);
                mesh.VertexCount  = 3;
                mesh.Invalidate();
            }

            // Reassign the pipeline from the active renderer.
            mat.PipelineRef = defaultPipeline;
            mat.Invalidate();
        }
    }

    uint32_t   m_RendererChangedListener = 0;
    Ref<Scene> m_Scene;
};
