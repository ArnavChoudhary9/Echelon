#!/usr/bin/env bash
# Echelon build (Linux / macOS) — fetch dependencies, generate project files, build.
# Runs from anywhere: it cd's to the repo root (parent of this script).

set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DEBUG=false
BUILD_RELEASE=false

if [[ $# -eq 0 ]]; then
    BUILD_DEBUG=true
    BUILD_RELEASE=true
else
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -d|--debug)   BUILD_DEBUG=true ;;
            -r|--release) BUILD_RELEASE=true ;;
            -h|--help)
                echo "Usage: scripts/build.sh [OPTIONS]"
                echo
                echo "Options:"
                echo "  -d, --debug     Build Debug configuration"
                echo "  -r, --release   Build Release configuration"
                echo "  -h, --help      Show this help message"
                echo
                echo "No options builds both Debug and Release."
                echo "Slang is fetched automatically (scripts/setup.py) before generating projects."
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                exit 1
                ;;
        esac
        shift
    done
fi

# ---- Toolchain checks ----
if [[ "$(uname)" == "Darwin" ]] && ! xcode-select -p &>/dev/null; then
    echo "Error: Xcode Command Line Tools not installed. Run: xcode-select --install"
    exit 1
fi

PYTHON="$(command -v python3 || command -v python || true)"
if [[ -z "$PYTHON" ]]; then
    echo "Error: python3 not found (required to fetch the Slang SDK)."
    exit 1
fi

# premake: prefer the vendored binary; fall back to one on PATH (e.g. `brew install premake`).
if [[ -x Vendor/premake5 ]]; then
    PREMAKE=Vendor/premake5
elif command -v premake5 &>/dev/null; then
    PREMAKE=premake5
else
    echo "Error: premake5 not found (expected Vendor/premake5 or premake5 on PATH)."
    exit 1
fi

# ---- Fetch dependencies (Slang SDK for this platform) ----
echo "Fetching dependencies..."
"$PYTHON" scripts/setup.py

# ---- Generate project files ----
echo "Generating project files..."
"$PREMAKE" gmake

# ---- Build ----
if $BUILD_DEBUG; then
    echo
    echo "Building Debug configuration..."
    make config=debug
fi

if $BUILD_RELEASE; then
    echo
    echo "Building Release configuration..."
    make config=release
fi

echo
echo "Build complete!"
