#!/usr/bin/env bash

set -e

BUILD_DEBUG=false
BUILD_RELEASE=false

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
                echo "Usage: ./build.sh [OPTIONS]"
                echo
                echo "Options:"
                echo "  -d, --debug     Build Debug configuration"
                echo "  -r, --release   Build Release configuration"
                echo "  -h, --help      Show this help message"
                echo
                echo "No options builds both Debug and Release."
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

echo "Generating project files..."
Vendor/premake5 gmake

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
