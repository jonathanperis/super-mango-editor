"""Release payload contract checks using small fixtures; no remote writes."""
from pathlib import Path
import sys
import tempfile
import zipfile
import shutil
import json
import os
import subprocess
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import package_release


def check_reproducible_zips(root):
    """The same files must zip to the same bytes, whatever their mtimes."""
    tree = root / "zip-fixture" / "bundle"
    (tree / "b").mkdir(parents=True)
    (tree / "b" / "z.txt").write_bytes(b"z")
    (tree / "a.txt").write_bytes(b"a")
    (tree / "run").write_bytes(b"#!/bin/sh\n")
    (tree / "run").chmod(0o755)
    first, second = root / "first.zip", root / "second.zip"
    with patch.dict(os.environ, {"SOURCE_DATE_EPOCH": ""}):
        package_release.zip_dir(tree, first)
        # New modification times (and a later run) must not change the archive.
        for path in tree.rglob("*"):
            os.utime(path, (1_900_000_000, 1_900_000_000))
        package_release.zip_dir(tree, second)
    assert first.read_bytes() == second.read_bytes()
    with zipfile.ZipFile(first) as bundle:
        names = bundle.namelist()
        assert names == sorted(names) == ["bundle/a.txt", "bundle/b/z.txt", "bundle/run"]
        assert all(info.date_time == (1980, 1, 1, 0, 0, 0) for info in bundle.infolist())
        assert bundle.getinfo("bundle/a.txt").external_attr >> 16 & 0o777 == 0o644
        if sys.platform != "win32":
            assert bundle.getinfo("bundle/run").external_attr >> 16 & 0o777 == 0o755
    # SOURCE_DATE_EPOCH (CI passes the commit time) dates every entry.
    with patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "1700000000"}):
        package_release.zip_dir(tree, second)
    with zipfile.ZipFile(second) as bundle:
        assert {info.date_time for info in bundle.infolist()} == {(2023, 11, 14, 22, 13, 20)}
    with patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "yesterday"}):
        try:
            package_release.zip_dir(tree, second)
        except SystemExit as error:
            assert "SOURCE_DATE_EPOCH" in str(error)
        else:
            raise AssertionError("a malformed SOURCE_DATE_EPOCH was accepted")


def main():
    (ROOT / "out").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "out") as temp:
        root = Path(temp)
        pin = json.loads((ROOT / "vendor/raylib/manifest.json").read_text())
        raylib_build = root / "raylib-build"
        source = raylib_build / ("raylib-" + pin["commit"])
        (source / "src/external/glfw").mkdir(parents=True)
        (source / "LICENSE").write_text("raylib license fixture")
        (source / "src/external/glfw/LICENSE.md").write_text("GLFW license fixture")
        (source / "src/external/codec.h").write_text("codec license fixture")
        for name in package_release.WASM_FILES:
            (root / name).write_bytes(b"payload")
        archive = root / "release.zip"
        package_release.package_wasm("super-mango-wasm", archive, root, raylib_build)
        with zipfile.ZipFile(archive) as bundle:
            assert all(f"super-mango-wasm/{name}" in bundle.namelist() for name in package_release.WASM_FILES)
            assert b"CK Tan" in bundle.read("super-mango-wasm/licenses/tomlc17.txt")
            assert "super-mango-wasm/THIRD_PARTY_NOTICES.md" in bundle.namelist()
            assert b"raylib" in bundle.read("super-mango-wasm/licenses/raylib.txt")
            assert b"codec" in bundle.read("super-mango-wasm/licenses/raylib-dependencies/codec.h")
        check_reproducible_zips(root)
        (root / "super-mango-debug.data").unlink()
        try:
            package_release.package_wasm("super-mango-wasm", archive, root, raylib_build)
        except SystemExit:
            pass
        else:
            raise AssertionError("incomplete debug payload was packaged")
        # A small fixture exercises the real native packager without copying
        # tens of megabytes into every host-contract run.
        fixture = root / "project"
        (fixture / "assets/sounds/unused").mkdir(parents=True)
        (fixture / "assets/sounds/used.wav").write_bytes(b"used")
        (fixture / "assets/sounds/unused/reserve.wav").write_bytes(b"reserve")
        (fixture / "levels/labs").mkdir(parents=True)
        (fixture / "levels/labs/example.toml").write_text("format_version = 1\n")
        (fixture / "vendor/tomlc17").mkdir(parents=True)
        (fixture / "vendor/raylib").mkdir(parents=True)
        for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "vendor/tomlc17/LICENSE", "vendor/raylib/manifest.json"):
            shutil.copy2(ROOT / name, fixture / name)
        windows = sys.platform == "win32"
        platform = "super-mango-windows-x86_64" if windows else "super-mango-native"
        suffix = ".exe" if windows else ""
        for name in ("super-mango", "super-mango-editor"):
            (fixture / f"{name}{suffix}").write_bytes(b"native fixture")
        original_root = package_release.ROOT
        try:
            package_release.ROOT = fixture
            package_release.package_native(platform, fixture / "super-mango", archive, None, raylib_build)
        finally:
            package_release.ROOT = original_root
        with zipfile.ZipFile(archive) as bundle:
            names = bundle.namelist()
            for name in ("super-mango", "super-mango-editor"):
                binary = bundle.getinfo(f"{platform}/{name}{suffix}")
                # Windows executability comes from the .exe format, not POSIX mode bits.
                if not windows:
                    assert binary.external_attr >> 16 & 0o111
            assert f"{platform}/levels/labs/example.toml" in names
            assert f"{platform}/assets/sounds/used.wav" in names
            assert not any("/unused/" in name for name in names)
            assert b"CK Tan" in bundle.read(f"{platform}/licenses/tomlc17.txt")
            assert b"raylib" in bundle.read(f"{platform}/licenses/raylib.txt")
        dll_dir = root / "dlls"
        dll_dir.mkdir()
        for name in ("game-only.dll", "editor-only.dll", "shared.dll", "SDL2.dll"):
            (dll_dir / name).write_bytes(b"DLL fixture")
        system = root / "windows/System32"
        system.mkdir(parents=True)
        (system / "KERNEL32.dll").write_bytes(b"system DLL fixture")
        imports = {"game.exe": ["game-only.dll"], "editor.exe": ["editor-only.dll"],
                   "game-only.dll": ["shared.dll"], "editor-only.dll": ["shared.dll"],
                   "shared.dll": ["KERNEL32.dll"]}

        def inspect(command, **kwargs):
            assert command[1] == "-p"
            text = "\n".join("DLL Name: " + name for name in imports[Path(command[2]).name])
            return subprocess.CompletedProcess(command, 0, text)

        with patch.object(package_release.shutil, "which", return_value="objdump"), \
             patch.object(package_release.subprocess, "run", side_effect=inspect), \
             patch.dict(os.environ, {"SystemRoot": str(system.parent)}):
            binaries = [root / "game.exe", root / "editor.exe"]
            names = {p.name for p in package_release.collect_windows_dlls(binaries, dll_dir)}
            assert names == {"game-only.dll", "editor-only.dll", "shared.dll"}
            imports["shared.dll"] = ["SDL2.dll"]
            try:
                package_release.collect_windows_dlls(binaries, dll_dir)
            except SystemExit as error:
                assert "unexpected SDL runtime dependency" in str(error)
            else:
                raise AssertionError("transitive SDL dependency was accepted")
            imports["game.exe"] = imports["editor.exe"] = ["KERNEL32.dll"]
            assert package_release.collect_windows_dlls(binaries, dll_dir) == []
    # Pages publishing lives in build.yml: the WASM artifact must come from
    # the same run (no run-id), only pages-deploy may hold Pages/OIDC rights,
    # and no workflow may consume another run's artifact via workflow_run.
    workflows = ROOT / ".github/workflows"
    assert not (workflows / "deploy.yml").exists()
    for path in workflows.glob("*.yml"):
        text = path.read_text()
        assert "\n  workflow_run:" not in text, path.name          # trigger
        assert "github.event.workflow_run" not in text, path.name  # its payload
    build = (workflows / "build.yml").read_text()
    pages_build = build.split("  pages-build:", 1)[1].split("  pages-deploy:", 1)[0]
    pages_deploy = build.split("  pages-deploy:", 1)[1]
    assert "name: super-mango-wasm" in pages_build and "run-id" not in pages_build
    assert "pages: write" not in pages_build and "id-token: write" not in pages_build
    assert "pages: write" in pages_deploy and "id-token: write" in pages_deploy
    assert "actions/checkout" not in pages_deploy and "bun " not in pages_deploy
    print("package_release_test: ok")


if __name__ == "__main__":
    main()
