#!/usr/bin/env python3
"""Generate compile_commands.json for clangd, IDEs and clang-tidy.

  make compile-commands

The Makefile passes the flags it really compiles with, so the database
describes this build and not a guess at it: the mode flags (-g -O0 or -O2
and the hardening flags), -DMANGO_RAYLIB_MEMORY on the Memory backend,
EXTRA_CFLAGS (CI's -Werror, the sanitizer flags), the raylib include path of
the chosen build directory and, for test code, TEST_CFLAGS and the
per-object -D renames that swap raylib calls for test doubles.

What gets an entry:

  src/**/*.c, tools/*.c   game flags and the project include paths
  vendor/tomlc17/*.c      game flags only (the Makefile gives vendored code
                          no project include paths)
  tests/*.c               test flags
  --test-sources          the game sources the Makefile compiles a second
                          time into the test tree. They get a second entry
                          with the test flags, after the game one, so the
                          code inside `#ifdef MANGO_TESTING` is analysed too.
                          clangd uses the first entry it finds for a file,
                          which is the shipped build.

The .c files in tests/editor_ui/ and tests/editor_validation/ are not
programs of their own. tests/editor_ui_test.c and
tests/editor_validation_test.c #include them, so they are compiled (and
analysed) as part of those two files and get no entry. A .c file in a tests/
subfolder that nothing includes is treated as a real source file and listed.

Paths in the output are machine-local; do not commit the JSON.
"""

from __future__ import annotations

import argparse
import json
import re
import shlex
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# `#include "editor_ui/canvas.c"` in a test: that file is a fragment.
INCLUDED_C_FILE = re.compile(r'^\s*#\s*include\s+"([^"\n]+\.c)"', re.M)


def included_fragments(root: Path) -> set[Path]:
    """The .c files that a tests/*.c file pulls in with #include."""
    tests = (root / "tests").resolve()
    fragments: set[Path] = set()
    for test in sorted(tests.glob("*.c")):
        text = test.read_text(encoding="utf-8", errors="replace")
        for name in INCLUDED_C_FILE.findall(text):
            path = (test.parent / name).resolve()
            # A test may also include a game source (../src/shared/graphics.c);
            # that file keeps its own entry, so only files under tests/ count.
            if tests in path.parents:
                fragments.add(path)
    return fragments


def game_sources(root: Path) -> list[Path]:
    """Sources built with the game flags, in a stable order."""
    sources = sorted((root / "src").rglob("*.c"))
    sources += sorted((root / "vendor" / "tomlc17").glob("*.c"))
    # C helpers built by the Makefile (tools/level_check.c for validate-levels).
    sources += sorted((root / "tools").glob("*.c"))
    return [path.resolve() for path in sources]


def test_sources(root: Path) -> list[Path]:
    """Test programs: tests/*.c, plus subfolder files that are not fragments."""
    tests = root / "tests"
    if not tests.is_dir():
        return []
    fragments = included_fragments(root)
    sources = sorted(tests.glob("*.c"))
    sources += [path for path in sorted(tests.rglob("*.c"))
                if path.parent != tests and path.resolve() not in fragments]
    return [path.resolve() for path in sources]


def is_vendored(root: Path, source: Path) -> bool:
    return (root / "vendor").resolve() in source.parents


def relative(root: Path, source: Path) -> str:
    return source.relative_to(root.resolve()).as_posix()


def entry(root: Path, source: Path, cc: str, flags: list[str], includes: list[str],
          output: str) -> dict:
    """One compile command, shaped like the Makefile's compile recipes."""
    arguments = [cc, *flags]
    if not is_vendored(root, source):
        arguments += includes
    arguments += ["-c", "-o", output, relative(root, source)]
    return {
        # Relative -I paths and the source path resolve against this folder.
        "directory": str(root.resolve()),
        "file": str(source),
        "arguments": arguments,
        "output": output,
    }


def build_entries(root: Path, cc: str, cflags: list[str], includes: list[str],
                  test_cflags: list[str], object_flags: dict[str, list[str]],
                  test_copies: list[str], objdir: str) -> list[dict]:
    """Every compile command of the build: game entries first, then tests."""
    entries = []
    for source in game_sources(root):
        rel = relative(root, source)
        entries.append(entry(root, source, cc, cflags, includes,
                             f"{objdir}/{rel[:-2]}.o"))
    # Test programs, then the game sources compiled again for the tests.
    seen: set[Path] = set()
    wanted = test_sources(root) + [(root / name).resolve() for name in test_copies]
    for source in wanted:
        if source in seen or not source.is_file():
            continue
        seen.add(source)
        rel = relative(root, source)
        # TEST_OBJ_FLAGS_<source path without .c> in the Makefile.
        flags = test_cflags + object_flags.get(rel[:-2], [])
        entries.append(entry(root, source, cc, flags, includes,
                             f"{objdir}/tests/{rel[:-2]}.o"))
    return entries


def parse_object_flags(values: list[str]) -> dict[str, list[str]]:
    """["src/shared/audio=-DA=b -DC=d"] -> {"src/shared/audio": ["-DA=b", "-DC=d"]}."""
    table: dict[str, list[str]] = {}
    for value in values:
        name, separator, flags = value.partition("=")
        if not separator or not name:
            raise SystemExit(f"gen_compile_commands: --object-flags needs PATH=FLAGS, got {value!r}")
        table[name] = shlex.split(flags)
    return table


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--cc", default="clang", help="the compiler the build uses")
    parser.add_argument("--cflags", help="the Makefile's CFLAGS, as one shell string")
    parser.add_argument("--includes", default="", help="the Makefile's PROJECT_INCLUDES")
    parser.add_argument("--test-cflags", help="the Makefile's TEST_CFLAGS")
    parser.add_argument("--object-flags", action="append", default=[], metavar="PATH=FLAGS",
                        help="extra test flags for one source (TEST_OBJ_FLAGS_<PATH>)")
    parser.add_argument("--test-sources", default="",
                        help="game sources the Makefile also compiles with the test flags")
    parser.add_argument("--objdir", default="out/obj", help="the Makefile's OBJDIR")
    parser.add_argument("--root", type=Path, default=ROOT, help=argparse.SUPPRESS)
    parser.add_argument("--output", type=Path, help="default: compile_commands.json in the repository")
    args = parser.parse_args(argv)
    if args.cflags is None or args.test_cflags is None:
        sys.stderr.write("gen_compile_commands: run `make compile-commands`; the Makefile "
                         "passes the build's own flags (--cflags, --test-cflags)\n")
        return 2

    root = args.root
    entries = build_entries(
        root, args.cc, shlex.split(args.cflags), shlex.split(args.includes),
        shlex.split(args.test_cflags), parse_object_flags(args.object_flags),
        args.test_sources.split(), args.objdir)
    if not entries:
        sys.stderr.write("gen_compile_commands: no .c sources found\n")
        return 1

    output = args.output or root / "compile_commands.json"
    output.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
    fragments = len(included_fragments(root))
    print(f"wrote {output.name} ({len(entries)} entries; "
          f"{fragments} #included test fragments are covered by the files that include them)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
