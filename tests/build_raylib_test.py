#!/usr/bin/env python3
"""tools/build_raylib.py unpacks only what is built and prunes older pins.

No network, CMake or compiler is used: the test builds a tiny fake source
archive with the same layout as raylib's and calls the two helper functions
that the build script runs before CMake.
"""

from __future__ import annotations

import io
import sys
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import build_raylib  # noqa: E402

COMMIT = "a" * 40
OLD_COMMIT = "b" * 40
TOP = "raylib-" + COMMIT


def add_file(package: tarfile.TarFile, name: str, text: str = "x\n") -> None:
    payload = text.encode("utf-8")
    info = tarfile.TarInfo(name)
    info.size = len(payload)
    package.addfile(info, io.BytesIO(payload))


def add_dir(package: tarfile.TarFile, name: str) -> None:
    info = tarfile.TarInfo(name)
    info.type = tarfile.DIRTYPE
    info.mode = 0o755
    package.addfile(info)


def fake_archive(path: Path, extra=None) -> None:
    """A tar.gz shaped like GitHub's raylib archive, a few bytes per file."""
    with tarfile.open(path, "w:gz") as package:
        for folder in ("", "/src", "/src/external", "/src/external/glfw", "/cmake",
                       "/examples", "/examples/core", "/projects", "/logo"):
            add_dir(package, TOP + folder)
        add_file(package, TOP + "/CMakeLists.txt", "project(raylib)\n")
        add_file(package, TOP + "/CMakeOptions.txt")
        add_file(package, TOP + "/LICENSE", "zlib\n")
        add_file(package, TOP + "/src/raylib.h", "/* raylib */\n")
        add_file(package, TOP + "/src/external/glfw/LICENSE.md")
        add_file(package, TOP + "/cmake/LibraryConfigurations.cmake")
        add_file(package, TOP + "/examples/core/core_basic_window.c")
        add_file(package, TOP + "/projects/README.md")
        add_file(package, TOP + "/logo/raylib.png")
        if extra:
            extra(package)


def expect_exit(action, label: str) -> None:
    try:
        action()
    except (SystemExit, tarfile.TarError):
        return
    raise AssertionError(label)


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        archive = root / "source.tar.gz"
        fake_archive(archive)

        # Only what CMake builds is unpacked.
        build = root / "build-dir"
        build.mkdir()
        source = build_raylib.extract_source(archive, build, COMMIT)
        assert source == build / TOP
        for kept in ("CMakeLists.txt", "CMakeOptions.txt", "LICENSE", "src/raylib.h",
                     "src/external/glfw/LICENSE.md", "cmake/LibraryConfigurations.cmake"):
            assert (source / kept).is_file(), f"{kept} was not unpacked"
        for skipped in ("examples", "projects", "logo"):
            assert not (source / skipped).exists(), f"{skipped}/ was unpacked"
        assert not list(build.glob(".extract-*")), "the scratch folder was left behind"

        # A tree that is already there is reused, not unpacked over: the
        # patches applied to it by an earlier run must survive.
        (source / "src/raylib.h").write_text("/* patched */\n", encoding="utf-8")
        assert build_raylib.extract_source(archive, build, COMMIT) == source
        assert (source / "src/raylib.h").read_text(encoding="utf-8") == "/* patched */\n"

        # Older pins are removed; this pin and unrelated folders stay.
        old = build / ("raylib-" + OLD_COMMIT)
        (old / "src").mkdir(parents=True)
        (old / "src/raylib.h").write_text("old\n", encoding="utf-8")
        for name in ("build", "raylib-notes", "raylib-" + "c" * 39):
            (build / name).mkdir()
        (build / ("raylib-" + "d" * 40)).write_text("a file, not a tree\n", encoding="utf-8")
        outside = root / ("raylib-" + "e" * 40)
        outside.mkdir()
        removed = build_raylib.prune_old_sources(build, source.name)
        assert removed == [old.name], removed
        assert not old.exists() and source.is_dir()
        for name in ("build", "raylib-notes", "raylib-" + "c" * 39, "raylib-" + "d" * 40):
            assert (build / name).exists(), f"{name} is not an old source tree but was removed"
        assert outside.is_dir(), "pruning left the build directory"
        assert build_raylib.prune_old_sources(build, source.name) == []

        # An archive of another commit has no folder for this pin: fail
        # instead of leaving an empty tree for CMake to trip over.
        other = root / "other"
        other.mkdir()
        expect_exit(lambda: build_raylib.extract_source(archive, other, OLD_COMMIT),
                    "an archive without the pinned folder was accepted")
        assert not (other / ("raylib-" + OLD_COMMIT)).exists()
        assert not list(other.glob(".extract-*"))

        # tarfile's "data" filter still guards the members that are unpacked:
        # a file that would land outside the destination stops the build.
        hostile = root / "hostile.tar.gz"
        fake_archive(hostile, lambda package: add_file(package, TOP + "/src/../../../escaped.txt"))
        target = root / "hostile-build"
        target.mkdir()
        expect_exit(lambda: build_raylib.extract_source(hostile, target, COMMIT),
                    "a path that leaves the build directory was unpacked")
        assert not (root / "escaped.txt").exists() and not (target / TOP).exists()

    # The member filter itself, including names that only look alike.
    keep = build_raylib.is_built_member
    assert keep(TOP, TOP, True) and keep(TOP + "/LICENSE", TOP, False)
    assert keep(TOP + "/src", TOP, True) and keep(TOP + "/src/rcore.c", TOP, False)
    assert not keep(TOP + "/examples", TOP, True)
    assert not keep(TOP + "/examples/core/x.c", TOP, False)
    assert not keep(TOP + "/srcs/x.c", TOP, False)
    assert not keep("other/src/x.c", TOP, False)
    print("build_raylib_test: ok")


if __name__ == "__main__":
    main()
