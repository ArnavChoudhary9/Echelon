# Best Practices for C++ Game Engine Development

## Using Premake + g++

This document outlines best practices for building and maintaining a **modular C++ game engine** using **Premake** as the build system generator and **GNU g++** as the compiler.

---

## Project Structure

* **Root-level folders**

  ```text
  /Engine        → Core engine code (modules, systems)
  /Sandbox       → Example app(s) using the engine
  /Vendor        → Third-party dependencies (GLFW, ImGui, Lua/Python, etc.)
  /Build         → Generated build files (ignored in git)
  /Scripts       → Premake scripts, utility scripts
  /Assets        → Test assets (textures, models, audio)
  ```

* Keep **modules decoupled** (Graphics, Audio, Physics, Scripting, etc.).
* Each module should be self-contained with its own include and source directories.

---

## Build System (Premake)

* Use a **`premake5.lua`** in root for project configuration.
* Generate per-platform projects:

  ```bash
  premake5 gmake2   # for GNU make
  premake5 vs2022   # for Visual Studio (if needed)
  ```

* Always commit **premake scripts**, never generated build files.
* Support **Debug** and **Release** configurations:

  * `Debug`: full symbols, assertions, runtime checks.
  * `Release`: optimizations (`-O2` or `-O3`), no asserts.

---

## Compiler & Toolchain (g++)

* Target **C++20** (modules, coroutines, concepts can be used gradually).
* Recommended compiler flags:

  ```bash
  -std=c++20 -Wall -Wextra -Wpedantic -Werror
  -Wno-unused-parameter -Wno-unused-variable   # selectively disable noise
  -g   # Debug info
  -O2  # Release optimization
  ```

* Consider enabling **link-time optimization (LTO)** in Release.
* Use `-fPIC` for shared libs (if building modular DLLs/so).

---

## Code Organization

* Each **system** has:

  * `SystemName.h` in `Engine/include/Engine/SystemName/`
  * `SystemName.cpp` in `Engine/src/SystemName/`
* Public APIs go in `include/Engine/`.
* Keep `.h` headers clean — forward declare where possible, avoid unnecessary includes.

---

## Dependencies

* Store external dependencies in `/Vendor`.
* Use `premake` modules or custom scripts to include them cleanly.
* Prefer **single-header or compiled libraries** over header-only “dump everything” style.
* Examples:

  * **GLFW** (windowing/input)
  * **Glad** (OpenGL loader)
  * **ImGui** (UI)
  * **Lua / Python** (scripting)
  * **STB** libs (images, audio helpers)

---

## Memory & Safety

* Prefer `std::unique_ptr` / `std::shared_ptr` where ownership semantics matter.
* For performance-critical paths, use custom allocators.
* Integrate a lightweight **assert & logging system** early.
* Add leak detection (Valgrind, ASAN) in Debug builds.

---

## Coding Standards

* Naming convention:

  * Classes: `PascalCase` (`Renderer`, `SceneGraph`)
  * Methods: `camelCase` (`renderFrame()`, `dispatchEvent()`)
  * Constants/macros: `ALL_CAPS`
* Use namespaces for subsystems:

  ```cpp
  namespace Engine::Graphics { ... }
  ```

* Avoid macros — use `constexpr`, `inline`, and templates instead.

---

## Testing & Examples

* Maintain a `/Sandbox` project as a **playground** for engine testing.
* Write minimal test apps to validate each new subsystem.
* Consider adding unit tests (e.g., Catch2, GoogleTest) for critical core logic.

---

## Git & Version Control

* Ignore generated build files (`/Build/**`, `/bin`, `/obj`).
* Commit only **source code, premake scripts, and vendor code**.
* Use `.editorconfig` or clang-format for consistent style.

---

## Documentation

* Maintain:

  * `README.md` → project overview
  * `PRD.md` → product requirements
  * `BestPractices.md` → coding/build guidelines
  * `docs/` → subsystem-level design notes
* Use Doxygen-compatible comments for API docs.

---

## Performance & Profiling

* Integrate lightweight profiling early (ImGui + timers).
* Always measure before optimizing.
* Keep profiling tools **enabled in Debug builds**.

---

## Roadmap Reminder

* **Phase 1**: Core loop, OpenGL, ImGui, Events, Logging, Scripting (Lua/Python).
* Future: Vulkan, PBR, Physics, Audio, ECS, Render Graph.

---

## Summary

* Use **Premake for cross-platform builds**, keep scripts clean.
* Stick to **C++20**, enforce warnings as errors.
* Keep modules independent, build them as libraries where possible.
* Test with `/Sandbox` continuously.
* Document everything.
