#!/usr/bin/env python3
"""Check that Mangolib, the engine layer, never includes game or editor code.

src/mangolib/ may include its own headers, raylib and the C library. Every
quoted #include in it must name a file in src/mangolib/ itself, so the game
and the editor depend on Mangolib and never the other way around.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MANGOLIB = ROOT / "src" / "mangolib"
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


def main() -> int:
    errors: list[str] = []
    files = sorted(p for p in MANGOLIB.iterdir() if p.suffix in {".c", ".h"})
    if not files:
        errors.append("src/mangolib/ has no sources")
    for path in files:
        rel = path.relative_to(ROOT).as_posix()
        for name in INCLUDE.findall(path.read_text(encoding="utf-8")):
            target = (path.parent / name).resolve()
            if target.parent != MANGOLIB or not target.is_file():
                errors.append(f'{rel}: #include "{name}" is outside src/mangolib/')
    for error in errors:
        print(f"layering: {error}", file=sys.stderr)
    if errors:
        return 1
    print(f"layering: ok ({len(files)} Mangolib files include only Mangolib)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
