#include "Echelon.h"

using namespace Echelon;

class EditorOverlay : public Overlay {
public:
    EditorOverlay() : Overlay() {
        // Initialization code for the editor overlay
    }

    virtual ~EditorOverlay() {
        // Cleanup code for the editor overlay
    }

    virtual void OnAttach() override {
        // Code to execute when the overlay is attached
    }

    virtual void OnDetach() override {
        // Code to execute when the overlay is detached
    }

    virtual void OnUpdate(float deltaTime) override {
        // Update logic for the overlay
    }

    virtual void OnEvent(Event& event) override {
        // Event handling logic for the overlay
    }

    virtual void OnImGUIBegin() override {
        // ImGui frame begin logic
    }

    virtual void OnImGUIRender() override {
        // ImGui rendering logic
    }

    virtual void OnImGUIEnd() override {
        // ImGui frame end logic
    }
};
