# Best Practices for C++ Game Engine Development

## Using Premake + g++

This document outlines best practices for building and maintaining a **modular C++ game engine** using **Premake** as the build system generator and **GNU g++** as the compiler.

---

## Project Structure

* **Root-level folders** (as they exist in this repository)

  ```text
  /Echelon        → Core engine code, built as a shared library (libEchelon.so)
  /Ray            → Default renderer plugin (libRay.so), loaded at runtime
  /EchelonEditor  → Editor application (the default startup project)
  /DefaultProject → Example project + test assets (scenes, meshes, images)
  /Vendor         → Third-party dependencies (GLFW, glad, glm, entt, slang, stb, …)
  /bin, /bin-int  → Generated build output (ignored in git)
  ```

* Keep **modules decoupled** (Graphics, Audio, Physics, Scripting, etc.).
* The renderer is a **runtime-loaded plugin** (dlopen) behind the `RendererAPI`
  contract, so back-ends can be hot-swapped without rebuilding the engine.
* The graphics HAL (`Echelon/GraphicsAPI/`) is back-end agnostic; concrete
  back-ends live under `Echelon/Platform/Backends/<Backend>/`.

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

* Naming convention (as used throughout this codebase):

  * Types: `PascalCase` (`Renderer`, `SceneGraph`)
  * Methods & functions: `PascalCase` (`BeginFrame()`, `DispatchEvent()`)
  * Member variables: `m_PascalCase` (`m_Pipeline`); public POD fields on
    plain component/descriptor structs may be bare `PascalCase` (`Position`)
  * Constants/macros: `ALL_CAPS` (engine macros are prefixed `ECHELON_`)
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

* **Done**: Core loop, Events, Logging, Instrumentation, OpenGL back-end,
  entt-based ECS + scene graph, YAML scene serialization, UUID asset system with
  hot-reload, Slang→SPIR-V shaders with reflection-driven materials, RenderGraph
  (sort + batch), runtime-loaded renderer plugins, **textures (stb_image importer +
  sampler binding)**.
* **In progress / next**: lighting (Blinn-Phong → PBR), multipass render graph
  (shadow maps, post-processing), editor UI panels (ImGui).
* **Future**: Vulkan back-end, physics, audio, scripting.

---

## Summary

* Use **Premake for cross-platform builds**, keep scripts clean.
* Stick to **C++20**, enforce warnings as errors.
* Keep modules independent, build them as libraries where possible.
* Test with `/Sandbox` continuously.
* Document everything.
