#!/usr/bin/env python3
"""Build the pinned SDL-free raylib dependency in project-local build storage."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
from pathlib import Path
import platform
import re
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
    # This build directory's own download is only a cache: after the pin
    # changes (a raylib upgrade) it holds the previous archive, so fetch the
    # pinned one again instead of failing. A shared --archive is someone
    # else's copy and still fails closed below.
    if (not args.archive and archive.is_file() and
            hashlib.sha256(archive.read_bytes()).hexdigest() != pin["sha256"]):
        archive.unlink()
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
    source = extract_source(archive, build, pin["commit"])
    prune_old_sources(build, source.name)
    patches = json.loads((ROOT / "vendor/raylib/patches.json").read_text(encoding="utf-8"))
    for name, replacements in patches.items():
        path = source / name
        original = text = path.read_text(encoding="utf-8")
        for replacement in replacements:
            before, after = replacement["before"], replacement["after"]
            # A tree extracted by an earlier run is already patched. Look for
            # the result first: an "after" that keeps its "before" (a hunk
            # that only appends lines) would otherwise apply again each run.
            if text.count(after) == 1:
                continue
            if text.count(before) == 1:
                text = text.replace(before, after, 1)
            else:
                raise SystemExit(f"raylib patch does not match pinned source: {name}")
        if text != original:
            path.write_text(text, encoding="utf-8", newline="\n")
    write_gamepad_mappings(source, pin["gamecontrollerdb"])
    output = build / "build"
    # A new pin extracts a new raylib-<commit> folder, and CMake refuses to
    # reuse a build tree configured from another source folder. Start that
    # tree afresh. Only the folder name is compared: CMake may spell the
    # absolute path differently from Python (C:/ against /c/ under MSYS2).
    cache = output / "CMakeCache.txt"
    if cache.is_file():
        homes = [line.split("=", 1)[1] for line in
                 cache.read_text(encoding="utf-8", errors="replace").splitlines()
                 if line.startswith("CMAKE_HOME_DIRECTORY:")]
        if not homes or homes[0].replace("\\", "/").rstrip("/").rsplit("/", 1)[-1] != source.name:
            shutil.rmtree(output)
    generator = "MinGW Makefiles" if os.name == "nt" else "Unix Makefiles"
    command = ["cmake", "-G", generator, "-S", str(source), "-B", str(output),
               "-DBUILD_EXAMPLES=OFF", "-DBUILD_SHARED_LIBS=OFF",
               "-DUSE_EXTERNAL_GLFW=OFF", "-DGLFW_BUILD_WAYLAND=OFF",
               "-DCUSTOMIZE_BUILD=OFF",
               "-DCMAKE_BUILD_TYPE=" + ("Debug" if args.mode == "debug" else "Release")]
    if args.platform == "web":
        # Size over speed for the download, like the Makefile's WEB_OPT. CMake
        # appends its Release flags (-O3) after CMAKE_C_FLAGS, so set them.
        command = ["emcmake", *command, "-DPLATFORM=Web", "-DCMAKE_C_FLAGS_RELEASE=-Os -DNDEBUG"]
    else:
        command += ["-DPLATFORM=" + ("Memory" if args.platform == "memory" else "Desktop"),
                    "-DCMAKE_C_COMPILER=" + args.cc]
        if args.mode == "release" and os.name != "nt":
            # Release executables link as PIE on Linux; the static library
            # must be position independent regardless of compiler defaults.
            command += ["-DCMAKE_POSITION_INDEPENDENT_CODE=ON"]
    # config.h wraps every SUPPORT_* default in #ifndef and the sources test
    # them with #if, so a -D...=0 here switches one feature off and leaves the
    # other upstream defaults alone, especially EndDrawing's automatic
    # polling/pacing. CUSTOMIZE_BUILD stays OFF: that mode drops config.h and
    # turns on extra features unless every option is pinned explicitly.
    #
    # The game and editor are 2D, draw with the built-in font, load only PNG
    # images and QOA (or WAV) sounds, and use GenImageColor (outside the generation
    # block) as their only generated image. Everything else is compiled out:
    # 3D models, other file formats, image export and procedural images, the
    # compression/hash helpers, input recording, gestures, the 3D camera,
    # clipboard images, and the F12 screenshot shortcut.
    flags = ["-DSUPPORT_SCREEN_CAPTURE=0", "-DSUPPORT_MODULE_RMODELS=0",
             *(f"-DSUPPORT_FILEFORMAT_{name}=0" for name in
               ("BMP", "GIF", "QOI", "DDS", "TTF", "FNT", "OGG", "MP3", "XM", "MOD")),
             "-DSUPPORT_IMAGE_EXPORT=0", "-DSUPPORT_IMAGE_GENERATION=0",
             "-DSUPPORT_COMPRESSION_API=0", "-DSUPPORT_AUTOMATION_EVENTS=0",
             "-DSUPPORT_GESTURES_SYSTEM=0", "-DSUPPORT_CAMERA_SYSTEM=0",
             "-DSUPPORT_CLIPBOARD_IMAGE=0"]
    if args.platform == "memory" or args.null_audio:
        # Only miniaudio's null backend: playback "succeeds" without hardware.
        flags += ["-DMA_ENABLE_ONLY_SPECIFIC_BACKENDS", "-DMA_ENABLE_NULL"]
    if args.mode == "debug":
        # Debug builds debug this project, not the pinned raylib: keep its
        # symbols and asserts (CMake's Debug type) but optimize it. Unoptimized,
        # the Memory backend's software rasterizer and miniaudio's resampler
        # take over half the CPU time of every test, smoke and replay run.
        flags += ["-O2"]
    if args.mode == "release" and args.platform != "web" and platform.system() != "Darwin":
        # One section per function and per global, so the Makefile's
        # -Wl,--gc-sections can drop the raylib code the game never calls
        # (see GC_CFLAGS there). Apple's linker strips unused code per
        # symbol (-dead_strip) and needs no compile flag for it.
        flags += ["-ffunction-sections", "-fdata-sections"]
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


# What the build reads from the source archive: the library sources (with the
# bundled GLFW and the other single-file dependencies under src/external/),
# the CMake helper scripts, and the files at the top of the archive
# (CMakeLists.txt, CMakeOptions.txt, LICENSE and friends). The archive also
# carries examples/ (72 MB unpacked) and projects/ (9 MB), which nothing here
# compiles: BUILD_EXAMPLES is OFF.
BUILT_FOLDERS = ("src", "cmake")
OLD_SOURCE_NAME = re.compile(r"raylib-[0-9a-f]{40}")


def is_built_member(name: str, top: str, is_dir: bool) -> bool:
    """True when an archive member is part of what CMake builds.

    `top` is the archive's single top folder, raylib-<commit>. A member is
    kept when it is that folder itself, a file directly inside it, or
    anything in one of BUILT_FOLDERS. The other folders (examples/,
    projects/, parser/ ...) are not even created.
    """
    parts = name.rstrip("/").split("/")
    if parts[0] != top:
        return False
    if len(parts) == 1:
        return True
    if len(parts) == 2 and not is_dir:
        return True
    return parts[1] in BUILT_FOLDERS


def extract_source(archive: Path, build: Path, commit: str) -> Path:
    """Unpack the parts of the pinned archive that are built; return the tree.

    The tree is build/raylib-<commit>. An existing one is reused as it is
    (the caller re-applies the patches, which is a no-op when present). A new
    one is unpacked into a scratch folder first and renamed into place at the
    end, so an interrupted run never leaves a half-unpacked tree that the
    next run would take for a complete one.

    filter="data" is tarfile's own safety check: it refuses absolute paths,
    "..", links that point outside the destination and device files.
    """
    top = "raylib-" + commit
    source = build / top
    if source.is_dir():
        return source
    scratch = build / f".extract-{os.getpid()}"
    shutil.rmtree(scratch, ignore_errors=True)
    try:
        with tarfile.open(archive, "r:gz") as package:
            members = [member for member in package.getmembers()
                       if is_built_member(member.name, top, member.isdir())]
            package.extractall(scratch, members=members, filter="data")
        if not (scratch / top / "CMakeLists.txt").is_file() or not (scratch / top / "src").is_dir():
            raise SystemExit(f"raylib archive has no {top}/CMakeLists.txt and {top}/src")
        os.replace(scratch / top, source)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    return source


def prune_old_sources(build: Path, keep: str) -> list[str]:
    """Delete the source trees of other pins from this build directory.

    Every raylib upgrade unpacks a new raylib-<commit> folder and the old
    one was never removed, so a long-lived build directory (or a CI cache)
    kept growing. Only folders directly inside `build` whose name is
    raylib-<40 hex digits> are touched; anything else a person put there
    stays. Returns the names removed, for the caller's log and the tests.
    """
    removed = []
    for entry in sorted(build.iterdir()):
        if entry.name == keep or not OLD_SOURCE_NAME.fullmatch(entry.name):
            continue
        if entry.is_symlink() or not entry.is_dir():
            continue
        shutil.rmtree(entry)
        removed.append(entry.name)
    for name in removed:
        print(f"build_raylib: removed the unpacked source of an older pin, {name}")
    return removed


MAPPING_PLATFORMS = (("WIN32", "platform:Windows"), ("COCOA", "platform:Mac OS X"),
                     ("LINUX", "platform:Linux"))


def write_gamepad_mappings(source: Path, pin: dict) -> None:
    """Regenerate GLFW's built-in gamepad mappings from the pinned database.

    Our input only reads controllers GLFW has a gamepad mapping for, so the
    mappings compiled into GLFW decide which pads work at all. This is GLFW's
    own update_mappings step (CMake/GenerateMappings.cmake) in Python, fed the
    checksum-pinned copy of SDL_GameControllerDB in vendor/raylib/ instead of
    a live download: Windows, macOS and Linux lines go, in file order, into
    the placeholders of the archive's own mappings.h.in, whose notices and
    built-in XInput entries stay as upstream wrote them.
    """
    database = ROOT / "vendor/raylib/gamecontrollerdb/gamecontrollerdb.txt"
    payload = database.read_bytes()
    if hashlib.sha256(payload).hexdigest() != pin["sha256"]:
        raise SystemExit(f"{database.relative_to(ROOT)} does not match its pinned checksum")
    groups: dict[str, list[str]] = {name: [] for name, _ in MAPPING_PLATFORMS}
    for line in payload.decode("utf-8").splitlines():
        if not line[:1] or line[0] not in "0123456789abcdefABCDEF":
            continue
        if '"' in line or "\\" in line:
            raise SystemExit("gamecontrollerdb.txt: a mapping would not be a plain C string")
        for name, marker in MAPPING_PLATFORMS:
            if marker in line:
                groups[name].append(f'"{line}",')
                break
    folder = source / "src/external/glfw/src"
    text = (folder / "mappings.h.in").read_text(encoding="utf-8")
    for name, lines in groups.items():
        placeholder = f"@GLFW_{name}_MAPPINGS@"
        if text.count(placeholder) != 1 or not lines:
            raise SystemExit(f"GLFW mappings.h.in: cannot fill {placeholder}")
        text = text.replace(placeholder, "\n".join(lines))
    target = folder / "mappings.h"
    if target.read_text(encoding="utf-8") != text:
        target.write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
