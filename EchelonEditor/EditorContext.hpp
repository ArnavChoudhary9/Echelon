#pragma once

/**
 * @file EditorContext.hpp
 * @brief Shared editor state + the message types panels exchange over the EventBus.
 *
 * Architecture: each editor panel is an independent Overlay on the layer stack
 * (see Panel.hpp). Panels never reference each other directly. Instead:
 *   - Shared STATE (active scene, selection, editor camera, viewport metrics,
 *     play flags, toolbar icons) lives here in one EditorContext, owned by the
 *     EditorLayer coordinator and read by panels each frame (immediate-mode idiom).
 *   - Discrete SIGNALS / COMMANDS flow through the core EventBus as the message
 *     structs below (EntitySelectedEvent, EditorCommandEvent, PlayStateChangedEvent).
 *
 * Example: HierarchyPanel publishes EntitySelectedEvent on click; EditorLayer
 * subscribes and writes ctx->Selection; InspectorPanel reads ctx->Selection.
 */

#include "Echelon/Echelon.hpp"
#include "Echelon/Asset/Texture/TextureAsset.hpp"

struct ImFont;  // forward-declare so EditorContext can hold ImFont* without pulling imgui.h

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <cstdint>

using namespace Echelon;

// Name of the object-id render target the editor's aux id-pass writes to (built by
// EditorLayer, read back by ViewportPanel for click-to-pick). Shared so both agree.
inline constexpr const char* kEditorIdResource = "editor_entityid";

// ------------------------------------------------------------------
// EventBus message types (any-type pub/sub; no base class required)
// ------------------------------------------------------------------

// Selection changed. Entity == entt::null means "deselect".
struct EntitySelectedEvent {
    entt::entity Entity{ entt::null };
};

// Play-mode transport commands, emitted by the toolbar (and Stats panel) and
// executed by the EditorLayer coordinator. Keeps the UI decoupled from play logic.
enum class EditorAction { Play, Pause, Resume, Stop, Restart, Step };
struct EditorCommandEvent {
    EditorAction Action;
};

// Broadcast by the coordinator after a transport change so any panel can react.
struct PlayStateChangedEvent {
    bool Playing;
    bool Paused;
};

// ------------------------------------------------------------------
// Toolbar icon sets (dark = white icons, light = black icons)
// ------------------------------------------------------------------
struct EditorIcons {
    Ref<TextureAsset> Play, Pause, Stop, Restart, Step;
};

// ------------------------------------------------------------------
// Shared editor state — single source of truth, read by all panels
// ------------------------------------------------------------------
struct EditorContext {
    // Scenes
    Ref<Scene> ActiveScene;    // edit scene, or the play-mode copy
    Ref<Scene> EditScene;      // authoritative scene — never mutated during play
    Ref<Scene> RuntimeScene;   // throwaway deep copy used while playing

    // Play / edit transport
    bool IsPlaying    = false;
    bool IsPaused     = false;
    bool StepOneFrame = false;

    // Selection (registry-specific handle into ActiveScene)
    entt::entity Selection{ entt::null };

    // Editor camera (not part of the scene ECS)
    Camera    EditorCamera;
    glm::vec3 EditorPos{ 0.0f, 14.0f, 10.0f };
    glm::vec3 EditorRot{ -52.0f, 0.0f, 0.0f };   // pitch, yaw, roll (degrees)
    float     MoveSpeed       = 10.0f;
    float     LookSensitivity = 0.08f;
    bool      FreeCam         = false;           // control camera without holding Alt

    // Viewport metrics (written by ViewportPanel, consumed by EditorLayer)
    glm::vec2 ViewportSize{ 0.0f, 0.0f };
    bool      ViewportFocused = false;
    bool      ViewportHovered = false;

    // Frame timing (for the Stats panel)
    float LastDeltaTime = 0.0f;

    // Toolbar icons
    EditorIcons DarkIcons;
    EditorIcons LightIcons;

    // Content-browser icons (loaded once, shared across frames)
    Ref<TextureAsset> DirIcon;
    Ref<TextureAsset> FileIcon;

    // Extra font for content-browser labels (larger than the 24px default).
    // Loaded in EditorLayer::OnAttach before the atlas is built.
    ImFont* FontLarge = nullptr;

    // The camera is driven whenever Alt is held (momentary) or free-cam is on.
    bool CameraActive() const {
        return FreeCam || Input::IsKeyPressed(Key::LeftAlt);
    }
};
