#include "ImGuiManager.hpp"

#include "Echelon/Platform/Window.hpp"
#include "Echelon/Core/Log.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

// glad + GLFW both live in libEchelon (the OpenGL backend and the window do too),
// so calling them here uses the same, already-initialised instances.
#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace Echelon {

    void ImGuiManager::Init(Window& window) {
        if (m_Initialized) return;

        m_Window = static_cast<GLFWwindow*>(window.GetNativeHandle());
        if (!m_Window) {
            ECHELON_LOG_ERROR("ImGuiManager: window has no native GLFW handle; ImGui disabled");
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // keyboard navigation
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // required for the dockspace

        ImGui::StyleColorsDark();

        // Larger UI text. In 1.92+ fonts are dynamic, so scaling the main factor
        // re-rasterises crisply (FontGlobalScale was moved to style.FontScaleMain).
        ImGuiStyle& style = ImGui::GetStyle();
        style.FontScaleMain = 1.75f;
        style.ScaleAllSizes(1.35f);   // widen padding/spacing to match the bigger text

        // install_callbacks=true: ImGui installs GLFW callbacks that chain-call the
        // engine's previously-installed ones, so both ImGui and the engine event
        // system see input. (The window is created + callbacks installed before this.)
        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
        ImGui_ImplOpenGL3_Init(nullptr);   // null => backend auto-detects the GLSL version

        ECHELON_LOG_INFO("ImGui {} initialised (docking enabled)", IMGUI_VERSION);
        m_Initialized = true;
    }

    void ImGuiManager::Shutdown() {
        if (!m_Initialized) return;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_Initialized = false;
    }

    void ImGuiManager::BeginFrame() {
        if (!m_Initialized) return;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiManager::EndFrame() {
        if (!m_Initialized) return;

        ImGui::Render();

        // Composite the UI onto the window's default framebuffer. The engine never
        // clears FBO 0 (GraphicsAPI::BeginFrame is a no-op) and in editor viewport
        // mode the scene is rendered into an offscreen texture — so establish a known
        // background here before drawing the UI over it. A previous pass may have left
        // the scissor test enabled, which would clip the clear, so disable it first.
        int w = 0, h = 0;
        glfwGetFramebufferSize(m_Window, &w, &h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, w, h);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // The OpenGL3 backend saves/restores GL state around this call.
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

} // namespace Echelon
