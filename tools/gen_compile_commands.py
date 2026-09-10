#!/usr/bin/env python3
"""Generate compile_commands.json for clangd / IDE IntelliSense.

Uses the same include flags as the Makefile (sdl2-config --cflags, -Isrc,
-Ivendor/tomlc17). Output paths are machine-local — do not commit the JSON.

  make compile-commands
"""

from __future__ import annotations

import json
import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_PATH = ROOT / "compile_commands.json"
SRCDIR = ROOT / "src"
VENDOR_DIR = ROOT / "vendor" / "tomlc17"
TESTDIR = ROOT / "tests"


def run_sdl2_cflags(sdl2cfg: str) -> list[str]:
    try:
        out = subprocess.check_output([sdl2cfg, "--cflags"], text=True).strip()
    except FileNotFoundError:
        sys.stderr.write(
            f"gen_compile_commands: '{sdl2cfg}' not found on PATH\n"
            "Install SDL2 dev packages or set SDL2CFG=/path/to/sdl2-config\n"
        )
        sys.exit(1)
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(f"gen_compile_commands: {sdl2cfg} --cflags failed: {exc}\n")
        sys.exit(1)
    return shlex.split(out) if out else []


def collect_sources() -> list[Path]:
    sources: list[Path] = []
    sources.extend(sorted(SRCDIR.rglob("*.c")))
    tomlc = VENDOR_DIR / "tomlc17.c"
    if tomlc.is_file():
        sources.append(tomlc)
    if TESTDIR.is_dir():
        sources.extend(sorted(TESTDIR.glob("*.c")))
    # Stable, unique, relative-friendly order
    seen: set[Path] = set()
    unique: list[Path] = []
    for path in sources:
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        unique.append(resolved)
    return unique


def build_arguments(cc: str, sdl_flags: list[str], source: Path) -> list[str]:
    args = [
        cc,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        *sdl_flags,
        f"-I{SRCDIR}",
        f"-I{VENDOR_DIR}",
    ]
    # Match Makefile TEST_CFLAGS: tests own main(); avoid SDL_main remap.
    if source.is_relative_to(TESTDIR):
        args = [a for a in args if a != "-Dmain=SDL_main"]
        if "-DSDL_MAIN_HANDLED" not in args:
            args.append("-DSDL_MAIN_HANDLED")
    args.extend(["-c", str(source)])
    return args


def main() -> int:
    sdl2cfg = os.environ.get("SDL2CFG", "sdl2-config")
    cc = os.environ.get("CC", "clang")
    sdl_flags = run_sdl2_cflags(sdl2cfg)
    sources = collect_sources()
    if not sources:
        sys.stderr.write("gen_compile_commands: no .c sources found\n")
        return 1

    entries = []
    for source in sources:
        entries.append(
            {
                "directory": str(ROOT),
                "file": str(source),
                "arguments": build_arguments(cc, sdl_flags, source),
            }
        )

    OUT_PATH.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT_PATH.relative_to(ROOT)} ({len(entries)} entries)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
