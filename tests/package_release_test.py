"""Release payload contract checks; no remote writes or native packaging."""
from pathlib import Path
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from package_release import WASM_FILES, package_wasm


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
        (root / "super-mango-debug.data").unlink()
        try:
            package_wasm("super-mango-wasm", archive, root)
        except SystemExit:
            pass
        else:
            raise AssertionError("incomplete debug payload was packaged")
    workflow = (ROOT / ".github/workflows/deploy.yml").read_text()
    assert "ref: ${{ github.event.workflow_run.head_sha }}" in workflow
    assert "github.event.workflow_run.head_repository.full_name == github.repository" in workflow
    print("package_release_test: ok")


if __name__ == "__main__":
    main()
