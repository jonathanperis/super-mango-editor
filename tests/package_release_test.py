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
from package_release import WASM_FILES, package_wasm, package_native


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
        for name in WASM_FILES:
            (root / name).write_bytes(b"payload")
        archive = root / "release.zip"
        package_wasm("super-mango-wasm", archive, root, raylib_build)
        with zipfile.ZipFile(archive) as bundle:
            assert all(f"super-mango-wasm/{name}" in bundle.namelist() for name in WASM_FILES)
            assert b"CK Tan" in bundle.read("super-mango-wasm/licenses/tomlc17.txt")
            assert "super-mango-wasm/THIRD_PARTY_NOTICES.md" in bundle.namelist()
            assert b"raylib" in bundle.read("super-mango-wasm/licenses/raylib.txt")
            assert b"codec" in bundle.read("super-mango-wasm/licenses/raylib-dependencies/codec.h")
        (root / "super-mango-debug.data").unlink()
        try:
            package_wasm("super-mango-wasm", archive, root, raylib_build)
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
            package_native(platform, fixture / "super-mango", archive, None, raylib_build)
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
    workflow = (ROOT / ".github/workflows/deploy.yml").read_text()
    assert "ref: ${{ github.event.workflow_run.head_sha }}" in workflow
    assert "github.event.workflow_run.head_repository.full_name == github.repository" in workflow
    print("package_release_test: ok")


if __name__ == "__main__":
    main()
