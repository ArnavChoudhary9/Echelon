#!/usr/bin/env python3
"""
Echelon dependency setup — fetches the prebuilt Slang SDK for the host platform.

Slang is a large prebuilt SDK (~40 MB of shared libraries per platform). Instead of
committing three platforms' worth of binaries, this script downloads the pinned release
for whatever OS/arch it runs on and lays the files out under Vendor/slang/ the way the
premake build expects:

    Vendor/slang/include/     C++ headers (slang.h, …)            — used by ShaderImporter
    Vendor/slang/lib/         link-time libs (libslang.so / slang.lib, incl. symlinks)
    Vendor/slang/runtime/     exactly the shared libs to ship next to the binaries
                              (real files, no symlinks, no llvm/gfx/cmake/pkgconfig extras)

The build (Dependencies.lua) links against lib/ and copies runtime/ next to the output
binaries — so runtime/ is the curated "what actually ships" set, kept version-agnostic:
premake globs it rather than naming versioned files.

Run automatically by scripts/build.sh / scripts/build.bat before generating project files,
or by hand:  python3 scripts/setup.py [--force] [--archive PATH]
"""

import argparse
import os
import platform
import shutil
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
from pathlib import Path

# Single source of truth for the Slang version. Bump this to upgrade the SDK.
SLANG_VERSION = "2026.14.1"

REPO_ROOT = Path(__file__).resolve().parent.parent
SLANG_DIR = REPO_ROOT / "Vendor" / "slang"

# Shared libs whose name contains any of these are link/runtime bloat we never ship
# (LLVM backend + the gfx sample layer). The engine only needs the libslang-* core.
EXCLUDE = ("llvm", "gfx")


def detect_platform():
    """Return (os_tag, arch_tag, archive_ext) for the Slang release naming scheme."""
    system = platform.system()
    machine = platform.machine().lower()

    os_tag = {"Linux": "linux", "Darwin": "macos", "Windows": "windows"}.get(system)
    if os_tag is None:
        sys.exit(f"error: unsupported OS '{system}'")

    if machine in ("x86_64", "amd64"):
        arch_tag = "x86_64"
    elif machine in ("aarch64", "arm64"):
        arch_tag = "aarch64"
    else:
        sys.exit(f"error: unsupported architecture '{machine}'")

    # Windows releases ship as .zip; every other platform as .tar.gz.
    ext = "zip" if os_tag == "windows" else "tar.gz"
    return os_tag, arch_tag, ext


def already_installed():
    """True if the pinned version is already laid out (idempotent no-op)."""
    tag = SLANG_DIR / "include" / "slang-tag-version.h"
    runtime = SLANG_DIR / "runtime"
    if not tag.is_file() or not runtime.is_dir() or not any(runtime.iterdir()):
        return False
    return f'"{SLANG_VERSION}"' in tag.read_text(errors="ignore")


def download(url, dest):
    print(f"  downloading {url}")
    with urllib.request.urlopen(url) as resp, open(dest, "wb") as out:
        shutil.copyfileobj(resp, out)


def extract(archive, dest):
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
    else:
        with tarfile.open(archive) as tf:
            tf.extractall(dest)


def reset_dir(path):
    if path.exists():
        shutil.rmtree(path)
    path.mkdir(parents=True)


def curate_unix(extracted, ext):
    """Populate lib/ (link, symlinks preserved) and runtime/ (real ship files) on Linux/macOS."""
    lib_src = extracted / "lib"
    lib_dst = SLANG_DIR / "lib"
    run_dst = SLANG_DIR / "runtime"

    for entry in sorted(lib_src.iterdir()):
        name = entry.name
        if not name.startswith("libslang"):
            continue
        if any(x in name for x in EXCLUDE):
            continue
        if entry.is_symlink():
            # Keep the link name (e.g. libslang.so → …) so the linker resolves -lslang.
            os.symlink(os.readlink(entry), lib_dst / name)
        else:
            shutil.copy2(entry, lib_dst / name)   # link-time (rpath-link finds the SONAME here)
            shutil.copy2(entry, run_dst / name)   # ship this exact real file at runtime


def curate_windows(extracted):
    """Populate lib/ (import .libs) and runtime/ (.dll) on Windows."""
    lib_dst = SLANG_DIR / "lib"
    run_dst = SLANG_DIR / "runtime"

    for f in sorted((extracted / "lib").glob("*.lib")):
        if not any(x in f.name.lower() for x in EXCLUDE):
            shutil.copy2(f, lib_dst / f.name)
    for f in sorted((extracted / "bin").glob("*.dll")):
        if not any(x in f.name.lower() for x in EXCLUDE):
            shutil.copy2(f, run_dst / f.name)


def main():
    ap = argparse.ArgumentParser(description="Fetch the Slang SDK for this platform.")
    ap.add_argument("--force", action="store_true", help="re-download even if already present")
    ap.add_argument("--archive", metavar="PATH",
                    help="use a local Slang release archive instead of downloading")
    args = ap.parse_args()

    if already_installed() and not args.force:
        print(f"Slang {SLANG_VERSION} already present — skipping (use --force to refetch).")
        return

    os_tag, arch_tag, ext = detect_platform()
    asset = f"slang-{SLANG_VERSION}-{os_tag}-{arch_tag}.{ext}"
    print(f"Setting up Slang {SLANG_VERSION} for {os_tag}-{arch_tag}...")

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        archive = Path(args.archive) if args.archive else tmp / asset
        if not args.archive:
            url = (f"https://github.com/shader-slang/slang/releases/download/"
                   f"v{SLANG_VERSION}/{asset}")
            download(url, archive)

        extracted = tmp / "extracted"
        extracted.mkdir()
        extract(archive, extracted)

        # Lay out a clean tree every run so upgrades/downgrades don't leave stragglers.
        reset_dir(SLANG_DIR / "include")
        reset_dir(SLANG_DIR / "lib")
        reset_dir(SLANG_DIR / "runtime")

        for item in (extracted / "include").iterdir():
            shutil.copy2(item, SLANG_DIR / "include" / item.name)

        if os_tag == "windows":
            curate_windows(extracted)
        else:
            curate_unix(extracted, "dylib" if os_tag == "macos" else "so")

    run_dst = SLANG_DIR / "runtime"
    shipped = sorted(p.name for p in run_dst.iterdir())
    print(f"Slang ready — will ship: {', '.join(shipped)}")


if __name__ == "__main__":
    main()
