#!/usr/bin/env bash

set -e

BUILD_DEBUG=false
BUILD_RELEASE=false

# ---- Dependency checks ----

if ! command -v premake5 &>/dev/null; then
    echo "Error: premake5 not found."
    echo "Install it with: brew install premake"
    exit 1
fi

if ! xcode-select -p &>/dev/null; then
    echo "Error: Xcode Command Line Tools not installed."
    echo "Install them with: xcode-select --install"
    exit 1
fi

# ---- Argument parsing ----

if [[ $# -eq 0 ]]; then
    BUILD_DEBUG=true
    BUILD_RELEASE=true
else
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -d|--debug)
                BUILD_DEBUG=true
                ;;
            -r|--release)
                BUILD_RELEASE=true
                ;;
            -h|--help)
                echo "Usage: ./build/build_mac.sh [OPTIONS]"
                echo
                echo "Options:"
                echo "  -d, --debug     Build Debug configuration"
                echo "  -r, --release   Build Release configuration"
                echo "  -h, --help      Show this help message"
                echo
                echo "No options builds both Debug and Release."
                echo
                echo "Prerequisites:"
                echo "  brew install premake"
                echo "  xcode-select --install"
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

# ---- Generate project files ----

echo "Generating project files..."
premake5 gmake

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
