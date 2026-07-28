# Echelon

A modular C++ game engine with a plugin-based renderer. The graphics backend (Ray) is a separately compiled shared library loaded at runtime, so it can be swapped without rebuilding the engine.

## Architecture

```text
EchelonEditor  ──links──▶  Echelon (libEchelon.so / .dll / .dylib)
                                │
                          RendererLoader
                                │ dlopen / LoadLibrary
                                ▼
                       Ray (libRenderer.so / .dll / .dylib)
                           RayRenderer (OpenGL + glad)
```

The engine core (`Echelon`) and the renderer (`Ray`) are both shared libraries. After building, the post-build step copies both next to the editor binary so they are found at launch without any `LD_LIBRARY_PATH` or `PATH` changes.

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
│   └── libRenderer.so         (or Renderer.dll / libRenderer.dylib) ← copied by post-build
├── Echelon/
│   └── libEchelon.so
└── Renderer/
    └── libRenderer.so
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
