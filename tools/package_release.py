#!/usr/bin/env python3
"""Create standalone Super Mango release archives.

The native game binary loads assets and levels from paths relative to the
process working directory, so shipping only the executable is not enough.
This helper builds a small runnable folder and compresses it into a zip file.
"""

from __future__ import annotations

import argparse
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


def copy_tree(src: Path, dst: Path) -> None:
    if not src.is_dir():
        raise SystemExit(f"required directory missing: {src}")
    shutil.copytree(src, dst, ignore=shutil.ignore_patterns(".DS_Store"))


def make_executable(path: Path) -> None:
    mode = path.stat().st_mode
    path.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def write_native_readme(bundle: Path, executable: str) -> None:
    (bundle / "README.txt").write_text(
        f"""Super Mango native release\n\nRun from this directory so the game can find assets/ and levels/.\n\nLinux/macOS:\n  ./{executable}\n\nWindows:\n  {executable}\n\nControls and development documentation:\n  https://jonathanperis.github.io/super-mango-editor/docs/\n\nSource and license:\n  https://github.com/jonathanperis/super-mango-editor\n""",
        encoding="utf-8",
    )


def collect_windows_dlls(binary: Path, dll_dir: Path) -> list[Path]:
    """Return runtime DLLs needed by an MSYS2-built Windows binary.

    In CI this runs inside the MSYS2 shell, where `ldd` prints resolved DLL
    paths. Keep the explicit SDL2*.dll glob as a fallback for environments
    where `ldd` output is incomplete.
    """
    dlls: set[Path] = set()
    if dll_dir.is_dir():
        dlls.update(path.resolve() for path in dll_dir.glob("SDL2*.dll"))

    ldd = shutil.which("ldd")
    if ldd:
        try:
            result = subprocess.run(
                [ldd, str(binary)],
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
        except OSError:
            result = None
        if result is not None:
            for line in result.stdout.splitlines():
                match = re.search(r"=>\s+([^\s]+\.dll)\b", line, re.IGNORECASE)
                if match:
                    candidate = Path(match.group(1))
                    if candidate.is_file():
                        dlls.add(candidate.resolve())
                    continue
                for token in line.split():
                    if token.lower().endswith(".dll"):
                        candidate = Path(token)
                        if candidate.is_file():
                            dlls.add(candidate.resolve())

    return sorted(dlls, key=lambda path: path.name.lower())


def zip_dir(src_dir: Path, output_zip: Path) -> None:
    output_zip.parent.mkdir(parents=True, exist_ok=True)
    if output_zip.exists():
        output_zip.unlink()
    with zipfile.ZipFile(output_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(src_dir.rglob("*")):
            if path.is_file():
                zf.write(path, path.relative_to(src_dir.parent))


def package_native(platform: str, binary: Path, output_zip: Path, dll_dir: Path | None) -> None:
    binary = binary.resolve()
    if not binary.is_file():
        raise SystemExit(f"binary missing: {binary}")

    with tempfile.TemporaryDirectory(prefix="super-mango-release-") as tmp:
        bundle = Path(tmp) / platform
        bundle.mkdir(parents=True)

        executable = "super-mango.exe" if platform.startswith("super-mango-windows") else "super-mango"
        bundled_binary = bundle / executable
        shutil.copy2(binary, bundled_binary)
        if not executable.endswith(".exe"):
            make_executable(bundled_binary)

        if dll_dir is not None:
            dlls = collect_windows_dlls(binary, dll_dir)
            if not dlls:
                raise SystemExit(f"no Windows runtime DLLs found for {binary} using {dll_dir}")
            for dll in dlls:
                shutil.copy2(dll, bundle / dll.name)

        copy_tree(ROOT / "assets", bundle / "assets")
        copy_tree(ROOT / "levels", bundle / "levels")
        shutil.copy2(ROOT / "LICENSE", bundle / "LICENSE")
        write_native_readme(bundle, executable)
        zip_dir(bundle, output_zip)


def package_wasm(platform: str, output_zip: Path) -> None:
    required = [
        ROOT / "out" / "super-mango.html",
        ROOT / "out" / "super-mango.js",
        ROOT / "out" / "super-mango.wasm",
        ROOT / "out" / "super-mango.data",
    ]
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
        shutil.copy2(ROOT / "LICENSE", bundle / "LICENSE")
        zip_dir(bundle, output_zip)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, help="Bundle directory/archive stem")
    parser.add_argument("--binary", type=Path, help="Native game binary to package")
    parser.add_argument("--output", type=Path, required=True, help="Output .zip path")
    parser.add_argument("--dll-dir", type=Path, help="Directory containing SDL2*.dll files for Windows")
    parser.add_argument("--wasm", action="store_true", help="Package out/super-mango WebAssembly artifacts")
    args = parser.parse_args(argv)

    if args.wasm:
        package_wasm(args.platform, args.output)
    else:
        if args.binary is None:
            parser.error("--binary is required unless --wasm is used")
        package_native(args.platform, args.binary, args.output, args.dll_dir)

    print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
