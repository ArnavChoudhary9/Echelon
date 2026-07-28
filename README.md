# Echelon

A modular C++ game engine with a plugin-based renderer. The graphics backend (Ray) is a separately compiled shared library loaded at runtime, so it can be swapped without rebuilding the engine.

## Architecture

```text
EchelonEditor  ──links──▶  Echelon (libEchelon.so / .dll / .dylib)
                                │
                          Renderer service (singleton)
                                │ dlopen / LoadLibrary  (RTLD_LOCAL, per-plugin)
                                ▼
                       Ray (libRay.so / Ray.dll / libRay.dylib)
                           RayRenderer (engine GraphicsAPI → OpenGL)
```

The engine core (`Echelon`) owns the OpenGL backend + glad; renderer plugins link only against `Echelon`. The engine loads the default renderer at startup through a global `Renderer` singleton and exposes it to user code via `Renderer::Get()`. Additional renderers can be loaded, unloaded, and hot-swapped at runtime; only the active renderer is GL-initialized at any time. After building, the post-build step copies the engine and the selected renderer next to the editor binary so they are found at launch without any `LD_LIBRARY_PATH` or `PATH` changes.

### Selecting the default renderer

`Ray` is the default. To build/ship a different renderer as the default (and omit Ray from the package), put it in a top-level folder named after it (with its own `premake5.lua` whose `targetname` equals the folder name, exporting `CreateRenderer`/`DestroyRenderer`), then generate with:

```bash
Vendor/premake5 gmake2 --renderer=MyRenderer
```

The chosen name is baked in as the runtime default (`ECHELON_DEFAULT_RENDERER`) and only that library is compiled and copied. If the renderer library is missing or incompatible at runtime, the engine logs an error and falls back to the last working renderer.

## Prerequisites

### Prerequisites — Windows

| Tool                              | Notes                          |
| --------------------------------- | ------------------------------ |
| Visual Studio 2022 or MinGW-w64   | C++20 compiler                 |
| GNU Make (via MinGW or Chocolatey)| `choco install make`           |

`Vendor/premake5.exe` is included; no separate install needed.

### Prerequisites — Linux

| Tool                  | Install                                                                              |
| --------------------- | ------------------------------------------------------------------------------------ |
| GCC 12+ or Clang 14+  | `sudo apt install build-essential`                                                   |
| GNU Make              | Included with `build-essential`                                                      |
| X11 dev headers       | `sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev` |
| OpenGL dev headers    | `sudo apt install libgl-dev`                                                         |

`Vendor/premake5` (Linux x86-64 binary) is included; no separate install needed.

### Prerequisites — macOS

| Tool                        | Install                     |
| --------------------------- | --------------------------- |
| Xcode Command Line Tools    | `xcode-select --install`    |
| premake5                    | `brew install premake`      |

No vendored macOS premake binary is included; the system-installed one is used.

## Building

All build scripts must be run from the **project root**.

### Build — Windows

```bat
build\build.bat [OPTIONS]
```

### Build — Linux

```bash
chmod +x build/build.sh
./build/build.sh [OPTIONS]
```

### Build — macOS

```bash
chmod +x build/build_mac.sh
./build/build_mac.sh [OPTIONS]
```

### Build options

| Flag               | Description                        |
| ------------------ | ---------------------------------- |
| `-d`, `--debug`    | Build Debug configuration only     |
| `-r`, `--release`  | Build Release configuration only   |
| `-h`, `--help`     | Show help and exit                 |

Passing no flags builds both Debug and Release.

### Examples

```bash
# Build both configurations
./build/build.sh

# Debug only
./build/build.sh --debug

# Release only
./build/build.sh --release
```

## Output layout

After a successful build the binaries land under `bin/`:

```text
bin/<config>-<os>-x86_64/
├── EchelonEditor/
│   ├── EchelonEditor          (or .exe)
│   ├── libEchelon.so          (or .dll / .dylib) ← copied by post-build
│   └── libRay.so              (or Ray.dll / libRay.dylib) ← copied by post-build
├── Echelon/
│   └── libEchelon.so
└── Ray/                       (or <renderer> when built with --renderer=<name>)
    └── libRay.so
```

The post-build copy ensures the editor can find both shared libraries without any environment variable changes.

## Project structure

```text
Echelon/
├── build/              Build scripts (Windows, Linux, macOS)
├── Echelon/            Engine core (shared library)
│   ├── Application/
│   ├── GraphicsAPI/    Abstract GPU abstraction layer
│   ├── Platform/       GLFW window + input backends
│   ├── Renderer/       RendererLoader (dlopen/LoadLibrary)
│   └── Scene/
├── EchelonEditor/      Editor host application
├── Ray/                Ray PBR renderer plugin (shared library)
├── Vendor/             Third-party libraries
│   ├── entt/
│   ├── glad/
│   ├── GLFW/
│   ├── glm/
│   ├── spdlog/
│   └── yaml/
├── Dependencies.lua    Shared include-path definitions
└── premake5.lua        Workspace definition
```

## License

See [LICENSE](LICENSE).
