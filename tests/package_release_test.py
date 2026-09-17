"""Release payload contract checks using small fixtures; no remote writes."""
from pathlib import Path
import sys
import tempfile
import zipfile
import shutil

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import package_release
from package_release import WASM_FILES, package_wasm, package_native


def main():
    (ROOT / "out").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="package-test-", dir=ROOT / "out") as temp:
        root = Path(temp)
        for name in WASM_FILES:
            (root / name).write_bytes(b"payload")
        archive = root / "release.zip"
        package_wasm("super-mango-wasm", archive, root)
        with zipfile.ZipFile(archive) as bundle:
            assert all(f"super-mango-wasm/{name}" in bundle.namelist() for name in WASM_FILES)
            assert b"CK Tan" in bundle.read("super-mango-wasm/licenses/tomlc17.txt")
            assert "super-mango-wasm/THIRD_PARTY_NOTICES.md" in bundle.namelist()
        (root / "super-mango-debug.data").unlink()
        try:
            package_wasm("super-mango-wasm", archive, root)
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
        for name in ("LICENSE", "THIRD_PARTY_NOTICES.md", "vendor/tomlc17/LICENSE"):
            shutil.copy2(ROOT / name, fixture / name)
        windows = sys.platform == "win32"
        platform = "super-mango-windows-x86_64" if windows else "super-mango-native"
        suffix = ".exe" if windows else ""
        for name in ("super-mango", "super-mango-editor"):
            (fixture / f"{name}{suffix}").write_bytes(b"native fixture")
        original_root = package_release.ROOT
        try:
            package_release.ROOT = fixture
            package_native(platform, fixture / "super-mango", archive, None)
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
    workflow = (ROOT / ".github/workflows/deploy.yml").read_text()
    assert "ref: ${{ github.event.workflow_run.head_sha }}" in workflow
    assert "github.event.workflow_run.head_repository.full_name == github.repository" in workflow
    print("package_release_test: ok")


if __name__ == "__main__":
    main()
