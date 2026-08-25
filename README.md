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
python3 scripts/setup.py            # fetch Slang once (build scripts do this for you)
Vendor/premake5 gmake2 --renderer=MyRenderer
make config=release
```

The chosen name is baked in as the runtime default (`ECHELON_DEFAULT_RENDERER`) and only that library is compiled and copied. If the renderer library is missing or incompatible at runtime, the engine logs an error and falls back to the last working renderer.

## Getting the source

The vendored libraries are git submodules, and the Slang shader SDK is fetched per-platform
at build time (it is **not** committed):

```bash
git clone <repo-url> Echelon
cd Echelon
git submodule update --init --recursive
```

The build scripts run `scripts/setup.py` automatically to download the correct Slang SDK for
your OS/architecture the first time you build (needs network access; re-runs are no-ops). You
can also fetch it by hand at any time: `python3 scripts/setup.py` (`--force` to refetch).

## Prerequisites

### Prerequisites — Windows

| Tool                              | Notes                          |
| --------------------------------- | ------------------------------ |
| Visual Studio 2022 or MinGW-w64   | C++20 compiler                 |
| GNU Make (via MinGW or Chocolatey)| `choco install make`           |
| Python 3                          | Fetches the Slang SDK          |

`Vendor/premake5.exe` is included; no separate install needed.

### Prerequisites — Linux

| Tool                  | Install                                                                              |
| --------------------- | ------------------------------------------------------------------------------------ |
| GCC 12+ or Clang 14+  | `sudo apt install build-essential`                                                   |
| GNU Make              | Included with `build-essential`                                                      |
| Python 3              | `sudo apt install python3` (fetches the Slang SDK)                                   |
| X11 dev headers       | `sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev` |
| OpenGL dev headers    | `sudo apt install libgl-dev`                                                         |

`Vendor/premake5` (Linux x86-64 binary) is included; no separate install needed.

### Prerequisites — macOS

| Tool                        | Install                     |
| --------------------------- | --------------------------- |
| Xcode Command Line Tools    | `xcode-select --install`    |
| premake5                    | `brew install premake`      |
| Python 3                    | Preinstalled / `brew install python` |

No vendored macOS premake binary is included; the system-installed one is used.

## Building

The build scripts can be run from anywhere — they `cd` to the project root themselves, fetch
the Slang SDK (`scripts/setup.py`), generate project files, and build.

### Build — Windows

```bat
scripts\build.bat [OPTIONS]
```

### Build — Linux / macOS

```bash
chmod +x scripts/build.sh
scripts/build.sh [OPTIONS]
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
scripts/build.sh

# Debug only
scripts/build.sh --debug

# Release only
scripts/build.sh --release
```

## Output layout

After a successful build the binaries land under `bin/`:

```text
bin/<config>-<os>-x86_64/
├── EchelonEditor/
│   ├── EchelonEditor          (or .exe)
│   ├── libEchelon.so          (or .dll / .dylib)       ← copied by post-build
│   ├── libRay.so              (or Ray.dll / libRay.dylib) ← copied by post-build
│   ├── libslang-*.so          (Slang runtime)          ← copied by post-build
│   ├── Shaders/               renderer + editor .slang + PBR.ehmaterialtype
│   ├── DefaultProject/        starter project (seeded only if absent)
│   ├── EditorResources/       icons + fonts
│   └── imgui.ini              default layout (seeded only if absent)
├── Echelon/
│   └── libEchelon.so
└── Ray/                       (or <renderer> when built with --renderer=<name>)
    └── libRay.so
```

The post-build copy ensures the editor finds the shared libraries, shaders, and Slang runtime
without any environment variable changes. The copy logic lives in reusable helpers in
`Dependencies.lua` (`SlangRuntimeCopy`, `CopyShaders`, `CopyGlob`), so it is not duplicated
per platform and picks up new files by glob.

## Project structure

```text
Echelon/
├── scripts/            build.sh / build.bat (cross-platform) + setup.py (fetches Slang)
├── docs/               PRD, best practices, TODO
├── Echelon/            Engine core (shared library)
│   ├── Application/
│   ├── GraphicsAPI/    Abstract GPU abstraction layer
│   ├── Platform/       GLFW window + input backends
│   ├── Renderer/       RendererLoader (dlopen/LoadLibrary)
│   └── Scene/
├── EchelonEditor/      Editor host application
├── Ray/                Ray PBR renderer plugin (shared library)
├── Vendor/             Third-party libraries (submodules)
│   ├── entt/  glad/  GLFW/  glm/  spdlog/  yaml/  …
│   └── slang/          Slang SDK — fetched by scripts/setup.py (not committed)
├── Dependencies.lua    Shared dep definitions + postbuild copy helpers
└── premake5.lua        Workspace definition
```

## License

See [LICENSE](LICENSE).
