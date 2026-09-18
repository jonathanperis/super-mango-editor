#!/usr/bin/env python3
"""Create standalone Super Mango release archives.

The native game binary loads assets and levels from paths relative to the
process working directory, so shipping only the executable is not enough.
This helper builds a small runnable folder and compresses it into a zip file.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WASM_FILES = [f"{stem}.{extension}" for stem in ("super-mango", "super-mango-debug")
              for extension in ("html", "js", "wasm", "data")]


def copy_tree(src: Path, dst: Path, *, playable_assets: bool = False) -> None:
    if not src.is_dir():
        raise SystemExit(f"required directory missing: {src}")
    ignored = [".DS_Store", ".gitkeep"] + (["unused"] if playable_assets else [])
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns(*ignored))


def raylib_source(build: Path) -> Path:
    pin = json.loads((ROOT / "vendor/raylib/manifest.json").read_text())
    return build / ("raylib-" + pin["commit"])


def copy_notices(bundle: Path, source: Path, dlls: list[Path] | None = None) -> None:
    shutil.copy2(ROOT / "LICENSE", bundle / "LICENSE")
    shutil.copy2(ROOT / "THIRD_PARTY_NOTICES.md", bundle / "THIRD_PARTY_NOTICES.md")
    licenses = bundle / "licenses"
    licenses.mkdir()
    shutil.copy2(ROOT / "vendor/tomlc17/LICENSE", licenses / "tomlc17.txt")
    shutil.copy2(source / "LICENSE", licenses / "raylib.txt")
    dependencies = licenses / "raylib-dependencies"
    dependencies.mkdir()
    shutil.copy2(source / "src/external/glfw/LICENSE.md", dependencies / "glfw.txt")
    # Preserve complete upstream files rather than heuristically extracting
    # license paragraphs from third-party code with different notice layouts.
    for path in sorted((source / "src/external").iterdir()):
        if path.is_file() and path.suffix in {".h", ".c"}:
            shutil.copy2(path, dependencies / path.name)
    if dlls:
        copy_windows_notices(dlls, licenses / "msys2")


def make_executable(path: Path) -> None:
    mode = path.stat().st_mode
    path.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def write_native_readme(bundle: Path, executable: str) -> None:
    (bundle / "README.txt").write_text(
        f"""Super Mango builder release\n\nRun from this directory so both programs can find assets/ and levels/.\n\nGame: ./{executable}\nEditor: ./super-mango-editor{'.exe' if executable.endswith('.exe') else ''}\n\nraylib is linked statically. Native OS graphics/audio support is required. Linux dialogs require zenity.\nRequired non-system Windows runtime DLLs are included. Playtest starts the sibling game without saving a personal profile.\n\nLearning manual:\n  https://jonathanperis.github.io/super-mango-editor/docs/learning-path/\n\nSource:\n  https://github.com/jonathanperis/super-mango-editor\nSee LICENSE, THIRD_PARTY_NOTICES.md and licenses/.\n""",
        encoding="utf-8",
    )


def collect_windows_dlls(binaries: list[Path], dll_dir: Path) -> list[Path]:
    """Inspect both PE executables and recursively resolve non-system imports."""
    objdump = shutil.which("objdump")
    if not objdump or not dll_dir.is_dir():
        raise SystemExit("Windows packaging requires objdump and the UCRT64 DLL directory")
    available = {path.name.lower(): path.resolve() for path in dll_dir.iterdir()
                 if path.is_file() and path.suffix.lower() == ".dll"}
    system = Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32"
    pending = list(binaries)
    seen: set[Path] = set()
    dlls: set[Path] = set()
    while pending:
        binary = pending.pop()
        if binary in seen:
            continue
        seen.add(binary)
        result = subprocess.run([objdump, "-p", str(binary)], check=True,
                                text=True, stdout=subprocess.PIPE)
        for name in re.findall(r"DLL Name:\s*(\S+)", result.stdout, re.IGNORECASE):
            key = name.lower()
            if key.startswith("sdl"):
                raise SystemExit(f"unexpected SDL runtime dependency: {name}")
            if key.startswith(("api-ms-win-", "ext-ms-win-")) or (system / name).is_file():
                continue
            dependency = available.get(key)
            if dependency is None:
                raise SystemExit(f"unresolved runtime dependency for {binary.name}: {name}")
            dlls.add(dependency)
            pending.append(dependency)
    return sorted(dlls, key=lambda path: path.name.lower())


def copy_windows_notices(dlls: list[Path], destination: Path) -> None:
    """Copy license files only from the packages owning bundled runtime DLLs."""
    packages: set[str] = set()
    for dll in dlls:
        posix = subprocess.check_output(["cygpath", "-u", str(dll)], text=True).strip()
        packages.add(subprocess.check_output(["pacman", "-Qoq", posix], text=True).strip())
    for package in sorted(packages):
        paths = subprocess.check_output(["pacman", "-Qlq", package], text=True).splitlines()
        for name in paths:
            if "/share/licenses/" not in name or name.endswith("/"):
                continue
            native = subprocess.check_output(["cygpath", "-m", name], text=True).strip()
            relative = name.split("/share/licenses/", 1)[1]
            target = destination / package / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(native, target)


def zip_dir(src_dir: Path, output_zip: Path) -> None:
    output_zip.parent.mkdir(parents=True, exist_ok=True)
    if output_zip.exists():
        output_zip.unlink()
    with zipfile.ZipFile(output_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(src_dir.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(src_dir.parent))


def package_native(platform: str, binary: Path, output_zip: Path, dll_dir: Path | None,
                   raylib_build: Path | None = None) -> None:
    binary = binary.resolve()
    if platform.startswith("super-mango-windows") and binary.suffix != ".exe":
        binary = binary.with_name(binary.name + ".exe")
    if not binary.is_file():
        raise SystemExit(f"binary missing: {binary}")
    editor = binary.with_name("super-mango-editor" + binary.suffix)
    if not editor.is_file():
        raise SystemExit(f"editor missing: {editor}; run make builder")

    with tempfile.TemporaryDirectory(prefix="super-mango-release-") as tmp:
        bundle = Path(tmp) / platform
        bundle.mkdir(parents=True)

        executable = "super-mango.exe" if platform.startswith("super-mango-windows") else "super-mango"
        bundled_binary = bundle / executable
        shutil.copy2(binary, bundled_binary)
        bundled_editor = bundle / editor.name
        shutil.copy2(editor, bundled_editor)
        if not executable.endswith(".exe"):
            make_executable(bundled_binary)
            make_executable(bundled_editor)

        dlls = []
        if dll_dir is not None:
            dlls = collect_windows_dlls([binary, editor], dll_dir)
            for dll in dlls:
                shutil.copy2(dll, bundle / dll.name)

        copy_tree(ROOT / "assets", bundle / "assets", playable_assets=True)
        copy_tree(ROOT / "levels", bundle / "levels")
        copy_notices(bundle, raylib_source(raylib_build or binary.parent / "raylib"), dlls)
        write_native_readme(bundle, executable)
        zip_dir(bundle, output_zip)


def package_wasm(platform: str, output_zip: Path, out_dir: Path = ROOT / "out",
                 raylib_build: Path | None = None) -> None:
    required = [out_dir / name for name in WASM_FILES]
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise SystemExit("missing WebAssembly artifacts:\n" + "\n".join(missing))

    with tempfile.TemporaryDirectory(prefix="super-mango-wasm-") as tmp:
        bundle = Path(tmp) / platform
        bundle.mkdir(parents=True)
        for artifact in required:
            shutil.copy2(artifact, bundle / artifact.name)
        (bundle / "README.txt").write_text(
            """Super Mango WebAssembly release\n\nServe this directory with any static HTTP server, then open super-mango.html.\n\nExample:\n  python3 -m http.server 8000\n\nThen browse to http://localhost:8000/super-mango.html\n""",
            encoding="utf-8",
        )
        copy_notices(bundle, raylib_source(raylib_build or out_dir / "raylib-web"))
        zip_dir(bundle, output_zip)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, help="Bundle directory/archive stem")
    parser.add_argument("--binary", type=Path, help="Native game binary to package")
    parser.add_argument("--output", type=Path, required=True, help="Output .zip path")
    parser.add_argument("--dll-dir", type=Path, help="UCRT64 runtime DLL directory for Windows")
    parser.add_argument("--raylib-build", type=Path, help="Pinned raylib dependency build directory")
    parser.add_argument("--wasm", action="store_true", help="Package out/super-mango WebAssembly artifacts")
    parser.add_argument("--out-dir", type=Path, default=ROOT / "out", help="WebAssembly build directory")
    args = parser.parse_args(argv)

    if args.wasm:
        package_wasm(args.platform, args.output, args.out_dir, args.raylib_build)
    else:
        if args.binary is None:
            parser.error("--binary is required unless --wasm is used")
        package_native(args.platform, args.binary, args.output, args.dll_dir, args.raylib_build)

    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
