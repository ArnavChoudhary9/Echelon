# Game Engine PRD

## Overview

This project is a **modular C++ game engine** designed with flexibility, performance, and scalability in mind.
The engine will support multiple rendering backends, advanced graphics features, profiling tools, and modular subsystems.
The development will follow an **incremental MVP approach**, starting with a minimal core (OpenGL + ImGui + Scripting) and expanding.

---

## Goals

1. **Fully Modular Design**

   * Every system should be decoupled and swappable (Graphics, Audio, Physics, Scripting).
   * Dependency inversion wherever possible.

2. **Incremental Development (MVP → Full Engine)**

   * **MVP**: Core engine loop, OpenGL backend, ImGui integration, Event + Logging system, **Scripting Layer (Lua or Python)**.
   * Later iterations: Vulkan, PBR, Ray Tracing, Audio, Physics, Render Graph, Profiling, etc.

---

## MVP Scope (Phase 1)

### Core Requirements

* **Application Layer**

  * Main loop: update, render, scripting hooks, event handling.
  * Platform abstraction (Windows initially).
  * Layer system (push/pop layers for scenes, UI, debug).

* **OpenGL Renderer**

  * Basic shader management.
  * Vertex, index, uniform buffers.
  * Texture management.
  * Batch rendering + instancing.

* **ImGui Integration**

  * Debug overlay and tools.
  * UI for performance graphs, logs, profiling results.

* **Event System**

  * Input handling (keyboard, mouse).
  * Window resize, focus, close events.
  * Event dispatcher with subscription model.

* **Logging System**

  * Subscription-based logging.
  * Output to console, file, and ImGui panel.

* **Scripting Layer (Lua/Python)**

  * Embed Lua or Python interpreter.
  * Expose engine APIs (input, events, entity transforms, rendering commands).
  * Attach scripts to entities (via ECS or simple binding in MVP).
  * Hot-reload support for faster iteration.
  * Scripts define game loop behaviors (update, on-event).

---

## Incremental Roadmap

### Phase 2: Graphics Extensions

* Vulkan backend.
* Programmable render pipeline.
* Render graph system.

### Phase 3: Advanced Rendering

* PBR rendering.
* Path tracing (compute → Vulkan RT).
* Optimized instancing.

### Phase 4: Engine Systems

* RefCounting/GC toggle.
* Profiling + Tracing tools (CPU/GPU timers, flame charts).

### Phase 5: Subsystems

* Audio (OpenAL/FMOD).
* Physics (Bullet/PhysX).
* ECS (Entity Component System).

---

## System Breakdown

### 1. **Core Framework**

* Window/platform abstraction.
* Layer stack.
* Event handling.

### 2. **Graphics System**

* OpenGL backend.
* Shaders, buffers, textures, framebuffers.

### 3. **UI System**

* ImGui integration.

### 4. **Event System**

* Dispatcher, subscription, input handling.

### 5. **Logging System**

* Subscription channels, multiple outputs.

### 6. **Scripting System (Phase 1)**

* Embedded Lua or Python.
* API exposure:

  * Input (keyboard, mouse).
  * Events (subscribe/trigger).
  * Entities (create/destroy, basic transforms).
  * Rendering commands (draw sprite, text, mesh).
* Script lifecycle: `onStart()`, `onUpdate(dt)`, `onEvent(e)`.
* Hot-reload support.

---

## Deliverables

* **Phase 1 (MVP)**:

  * OpenGL renderer.
  * ImGui debug UI.
  * Event + logging system.
  * **Scripting integration (Lua or Python)**.
  * Core loop with scripting hooks.

---
