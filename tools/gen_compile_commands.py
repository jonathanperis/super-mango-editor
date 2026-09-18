#!/usr/bin/env python3
"""Generate compile_commands.json for clangd / IDE IntelliSense.

Uses the pinned raylib build headers plus src/ and vendor/tomlc17/.
Output paths are machine-local — do not commit the JSON.

  make compile-commands
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_PATH = ROOT / "compile_commands.json"
SRCDIR = ROOT / "src"
VENDOR_DIR = ROOT / "vendor" / "tomlc17"
TESTDIR = ROOT / "tests"


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


def build_arguments(cc: str, raylib_include: Path, source: Path) -> list[str]:
    args = [
        cc,
        "-std=c11",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        f"-I{raylib_include}",
        f"-I{SRCDIR}",
        f"-I{VENDOR_DIR}",
    ]
    args.extend(["-c", str(source)])
    return args


def main() -> int:
    cc = os.environ.get("CC", "clang")
    raylib_build = Path(os.environ.get("RAYLIB_BUILD", str(ROOT / "out/raylib")))
    raylib_include = raylib_build.resolve() / "build/raylib/include"
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
                "arguments": build_arguments(cc, raylib_include, source),
            }
        )

    OUT_PATH.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT_PATH.relative_to(ROOT)} ({len(entries)} entries)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
