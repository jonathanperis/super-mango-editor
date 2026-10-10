#!/usr/bin/env python3
"""Build the pinned SDL-free raylib dependency in project-local build storage."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
from pathlib import Path
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--platform", choices=("native", "web", "memory"), default="native")
    parser.add_argument("--mode", choices=("debug", "release"), default="debug")
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--sanitize", action="store_true")
    # Test-only: real GLFW/OpenGL graphics with miniaudio's null playback
    # device, for CI machines that have a display but no sound hardware.
    parser.add_argument("--null-audio", action="store_true")
    parser.add_argument("--archive", type=Path)
    args = parser.parse_args()
    pin = json.loads((ROOT / "vendor/raylib/manifest.json").read_text(encoding="utf-8"))
    # The commit names the folder inside the archive and the URL names the
    # archive, so a bump that changes one and not the other (a half-applied
    # Renovate update, say) would keep building the old source under the new
    # version. Refuse it here rather than trust the two to stay in step.
    if not pin["url"].endswith("/" + pin["commit"]):
        raise SystemExit("vendor/raylib/manifest.json: url is not the archive of the pinned commit")
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    # --archive lets several build directories (and CI caches) share one
    # download. It is still verified below, so a stale or corrupt shared copy
    # fails closed instead of building unpinned code.
    archive = args.archive or build / "source.tar.gz"
    if not archive.is_file():
        with urllib.request.urlopen(pin["url"], timeout=120) as response:
            payload = response.read()
        if hashlib.sha256(payload).hexdigest() != pin["sha256"]:
            raise SystemExit("raylib archive checksum mismatch")
        archive.parent.mkdir(parents=True, exist_ok=True)
        # Write then rename so a concurrent or interrupted build never sees
        # a truncated archive at the shared path.
        partial = archive.with_name(archive.name + f".{os.getpid()}.part")
        partial.write_bytes(payload)
        os.replace(partial, archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]:
        raise SystemExit("raylib archive checksum mismatch")
    source = build / ("raylib-" + pin["commit"])
    if not source.is_dir():
        with tarfile.open(archive, "r:gz") as package:
            package.extractall(build, filter="data")
    patches = json.loads((ROOT / "vendor/raylib/patches.json").read_text(encoding="utf-8"))
    for name, replacements in patches.items():
        path = source / name
        original = text = path.read_text(encoding="utf-8")
        for replacement in replacements:
            before, after = replacement["before"], replacement["after"]
            if text.count(before) == 1:
                text = text.replace(before, after, 1)
            elif text.count(after) != 1:
                raise SystemExit(f"raylib patch does not match pinned source: {name}")
        if text != original:
            path.write_text(text, encoding="utf-8", newline="\n")
    output = build / "build"
    generator = "MinGW Makefiles" if os.name == "nt" else "Unix Makefiles"
    command = ["cmake", "-G", generator, "-S", str(source), "-B", str(output),
               "-DBUILD_EXAMPLES=OFF", "-DBUILD_SHARED_LIBS=OFF",
               "-DUSE_EXTERNAL_GLFW=OFF", "-DGLFW_BUILD_WAYLAND=OFF",
               "-DCUSTOMIZE_BUILD=OFF",
               "-DCMAKE_BUILD_TYPE=" + ("Debug" if args.mode == "debug" else "Release")]
    if args.platform == "web":
        command = ["emcmake", *command, "-DPLATFORM=Web"]
    else:
        command += ["-DPLATFORM=" + ("Memory" if args.platform == "memory" else "Desktop"),
                    "-DCMAKE_C_COMPILER=" + args.cc]
        if args.mode == "release" and os.name != "nt":
            # Release executables link as PIE on Linux; the static library
            # must be position independent regardless of compiler defaults.
            command += ["-DCMAKE_POSITION_INDEPENDENT_CODE=ON"]
    # config.h guards individual flags. Keep all upstream defaults, especially
    # EndDrawing's automatic polling/pacing; CMake's customization mode enables
    # additional features unless every option is explicitly pinned.
    flags = ["-DSUPPORT_SCREEN_CAPTURE=0"]
    if args.platform == "memory" or args.null_audio:
        # Only miniaudio's null backend: playback "succeeds" without hardware.
        flags += ["-DMA_ENABLE_ONLY_SPECIFIC_BACKENDS", "-DMA_ENABLE_NULL"]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    if flags:
        command += ["-DCMAKE_C_FLAGS=" + " ".join(flags)]
    subprocess.run(command, check=True)
    subprocess.run(["cmake", "--build", str(output), "--parallel",
                    str(min(os.cpu_count() or 2, 8))], check=True)
    # Ordered text/shortcut commands chain the public callbacks of raylib's
    # GLFW backend. The Memory suite uses this header for test-owned callbacks.
    include = output / "raylib/include/GLFW"
    include.mkdir(exist_ok=True)
    shutil.copy2(source / "src/external/glfw/include/GLFW/glfw3.h", include / "glfw3.h")
    # libraylib.a is deliberately not touched here. When CMake found nothing
    # to rebuild, the library keeps its old timestamp, so the game objects that
    # depend on it are not recompiled. The Makefile records a successful run in
    # its own build-done.stamp file instead.


if __name__ == "__main__":
    main()
