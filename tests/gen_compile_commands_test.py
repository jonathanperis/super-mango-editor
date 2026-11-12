#!/usr/bin/env python3
"""tools/gen_compile_commands.py writes the flags the Makefile compiles with.

The first half builds a small fake source tree and checks which files get an
entry and with which flags. The second half asks the real Makefile to run
its `compile-commands` recipe (make -n prints it without running it) and
checks that the recipe hands the script the build's own variables.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import gen_compile_commands as gen  # noqa: E402


def write(path: Path, text: str = "int x;\n") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def fake_tree(root: Path) -> None:
    write(root / "src/main.c")
    write(root / "src/shared/audio.c")
    write(root / "src/shared/graphics.c")
    write(root / "vendor/tomlc17/tomlc17.c")
    write(root / "tools/level_check.c")
    write(root / "tests/rail_test.c", '#include "../src/shared/graphics.c"\n')
    write(root / "tests/editor_ui_test.c",
          '#include "editor_ui/canvas.c"\n  #  include "editor_ui/palette.c"\n')
    write(root / "tests/editor_ui/canvas.c")
    write(root / "tests/editor_ui/palette.c")
    # In a subfolder, but nothing includes it: a real source file.
    write(root / "tests/helpers/standalone.c")


def by_output(entries: list[dict]) -> dict[str, dict]:
    return {item["output"]: item for item in entries}


def check_fake_tree() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp).resolve()
        fake_tree(root)
        output = root / "db.json"
        code = gen.main([
            "--root", str(root), "--output", str(output), "--cc", "my-cc",
            "--cflags=-std=c11 -O2 -Iout/raylib/build/raylib/include -DMANGO_RAYLIB_MEMORY -Werror",
            "--includes=-Isrc -Ivendor/tomlc17",
            "--test-cflags=-std=c11 -O2 -DMANGO_TEST_OUTDIR='\"out dir\"' -DMANGO_TESTING",
            "--object-flags", "src/shared/audio=-DSetMusicVolume=test_SetMusicVolume -DA=b",
            "--test-sources", "src/shared/audio.c src/shared/missing.c",
            "--objdir", "build/obj",
        ])
        assert code == 0
        entries = json.loads(output.read_text(encoding="utf-8"))
        table = by_output(entries)
        assert len(table) == len(entries), "two entries share one object file"

        game = table["build/obj/src/shared/audio.o"]
        assert game["arguments"][0] == "my-cc" and game["directory"] == str(root)
        assert game["file"] == str(root / "src/shared/audio.c")
        for flag in ("-O2", "-DMANGO_RAYLIB_MEMORY", "-Werror", "-Isrc", "-Ivendor/tomlc17",
                     "-Iout/raylib/build/raylib/include"):
            assert flag in game["arguments"], f"game entry lost {flag}"
        assert "-DMANGO_TESTING" not in game["arguments"]
        assert game["arguments"][-4:] == ["-c", "-o", "build/obj/src/shared/audio.o",
                                          "src/shared/audio.c"]

        # Vendored code is compiled without the project include paths.
        vendor = table["build/obj/vendor/tomlc17/tomlc17.o"]
        assert "-Isrc" not in vendor["arguments"] and "-Werror" in vendor["arguments"]
        assert "build/obj/tools/level_check.o" in table

        # The test copy of a game source: test flags plus its own renames,
        # listed after the game entry so clangd shows the shipped build.
        copy = table["build/obj/tests/src/shared/audio.o"]
        for flag in ("-DMANGO_TESTING", "-DSetMusicVolume=test_SetMusicVolume", "-DA=b", "-Isrc",
                     '-DMANGO_TEST_OUTDIR="out dir"'):
            assert flag in copy["arguments"], f"test copy lost {flag}"
        assert entries.index(game) < entries.index(copy)
        assert "build/obj/tests/src/shared/missing.o" not in table, "listed a source that is absent"

        # Test programs get the test flags and no other file's renames.
        rail = table["build/obj/tests/tests/rail_test.o"]
        assert "-DMANGO_TESTING" in rail["arguments"] and "-DA=b" not in rail["arguments"]
        # Fragments ride with the file that includes them; a lone file does not.
        files = {item["file"] for item in entries}
        assert str(root / "tests/editor_ui/canvas.c") not in files
        assert str(root / "tests/editor_ui/palette.c") not in files
        assert str(root / "tests/helpers/standalone.c") in files
        # graphics.c is included by a test and is still a game source.
        assert "build/obj/src/shared/graphics.o" in table
        assert gen.included_fragments(root) == {root / "tests/editor_ui/canvas.c",
                                                root / "tests/editor_ui/palette.c"}

        # Without the Makefile's flags there is nothing truthful to write.
        assert gen.main(["--root", str(root), "--output", str(output)]) == 2
    try:
        gen.parse_object_flags(["no-equals-sign"])
    except SystemExit:
        pass
    else:
        raise AssertionError("accepted --object-flags without PATH=FLAGS")


def check_makefile_recipe() -> None:
    """`make -n compile-commands` passes CFLAGS, TEST_CFLAGS and the rest."""
    make = shutil.which("make")
    if not make:
        print("gen_compile_commands_test: make not on PATH; skipped the recipe check")
        return
    # This test usually runs inside `make test`, sometimes inside another
    # make (`make coverage`, `make sanitize`). A parent make hands its
    # command-line variables (OUTDIR, RAYLIB_BUILD, EXTRA_CFLAGS ...) to
    # child makes through the MAKEFLAGS environment variable, and CI sets a
    # few more in the environment. The probe must see only the values it
    # passes itself, so those are removed first.
    inherited = {"MAKEFLAGS", "MFLAGS", "MAKELEVEL", "MAKEOVERRIDES", "RAYLIB_BUILD", "RAYLIB_AUDIO",
                 "EXTRA_CFLAGS", "EXTRA_LDFLAGS", "BUILD_MODE", "OUTDIR", "CC", "RAYLIB_PLATFORM"}
    env = {name: value for name, value in os.environ.items() if name not in inherited}
    result = subprocess.run(
        [make, "-n", "compile-commands", "CC=probe-cc", "OUTDIR=out/probe-dir",
         "RAYLIB_PLATFORM=memory", "BUILD_MODE=release", "EXTRA_CFLAGS=-DPROBE_EXTRA"],
        cwd=ROOT, env=env, capture_output=True, text=True, check=False)
    assert result.returncode == 0, result.stdout + result.stderr
    recipe = result.stdout
    for needle in ("tools/gen_compile_commands.py", "probe-cc", "-O2", "-DMANGO_RAYLIB_MEMORY",
                   "-DPROBE_EXTRA", "-Iout/probe-dir/raylib/build/raylib/include",
                   "-Isrc", "-DMANGO_TESTING", "-DMANGO_MEMORY_TESTS", "--test-sources",
                   "src/shared/audio=", "-DSetMusicVolume=test_SetMusicVolume",
                   "--objdir", "out/probe-dir/obj"):
        assert needle in recipe, f"the compile-commands recipe does not pass {needle}\n{recipe}"


def main() -> None:
    check_fake_tree()
    check_makefile_recipe()
    print("gen_compile_commands_test: ok")


if __name__ == "__main__":
    main()
