# Echelon

A fully modular C++ game engine with swappable systems, starting with OpenGL and ImGui, designed for flexibility, performance, and advanced rendering experiments like PBR and path tracing.

## Building

Build scripts are located in the `build/` directory and must be run from the **project root**.

### Windows

```bat
build\build.bat [OPTIONS]
```

### Linux / macOS

Make the script executable first, then run from the project root:

```bash
chmod +x build/build.sh
./build/build.sh [OPTIONS]
```

### Options

|Flag|Description|
|----|-----------|
|`-d`, `--debug`|Build Debug configuration|
|`-r`, `--release`|Build Release configuration|
|`-h`, `--help`|Show help message|

Passing no options builds both Debug and Release.

### Examples

```bash
# Build both configurations
./build/build.sh

# Debug only
./build/build.sh --debug

# Release only
./build/build.sh --release
```
