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

        // ---- Create a triangle vertex buffer ----
        // Flat shader layout: vec3 position + vec3 color
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

        m_TriangleVB = renderer->GetDevice()->CreateBuffer(vbDesc);
    }

    virtual void OnDetach() override {
        m_TriangleVB = nullptr;
        m_RendererLoader.Unload();
    }

    virtual void OnUpdate(float deltaTime) override {
        (void)deltaTime;

        auto* renderer = m_RendererLoader.Get();
        if (!renderer) return;

        // Identity matrices — NDC coordinates
        glm::mat4 identity(1.0f);
        ClearValue clear;
        clear.Color = { 0.1f, 0.1f, 0.12f, 1.0f };

        renderer->BeginFrame(identity, identity, clear);
        renderer->BeginScene(nullptr);

        // Draw the triangle using the renderer's default flat pipeline
        if (m_TriangleVB) {
            renderer->Draw(m_TriangleVB,
                           renderer->GetDefaultPipeline(),
                           identity,
                           3);
        }

        renderer->EndScene();
        renderer->EndFrame();
    }

    virtual void OnEvent(Event& event) override {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& e) {
            auto* renderer = m_RendererLoader.Get();
            if (renderer) {
                renderer->OnResize(e.GetWidth(), e.GetHeight());
            }
            return false;
        });
    }

    virtual void OnImGUIBegin() override {}
    virtual void OnImGUIRender() override {}
    virtual void OnImGUIEnd() override {}

private:
    RendererLoader m_RendererLoader;
    Ref<Buffer>    m_TriangleVB;
};
