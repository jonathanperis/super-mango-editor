#!/usr/bin/env python3
"""Verify generated WebAssembly artifacts and optional release zip contents."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_OUT = [
    "super-mango.html",
    "super-mango.js",
    "super-mango.wasm",
    "super-mango.data",
]
REQUIRED_ZIP = [
    "super-mango-wasm/super-mango.html",
    "super-mango-wasm/super-mango.js",
    "super-mango-wasm/super-mango.wasm",
    "super-mango-wasm/super-mango.data",
    "super-mango-wasm/README.txt",
    "super-mango-wasm/LICENSE",
]


def fail(message: str) -> int:
    print(f"wasm artifact check failed: {message}", file=sys.stderr)
    return 1


def require_nonempty(path: Path) -> int:
    if not path.is_file():
        return fail(f"missing {path.relative_to(ROOT)}")
    if path.stat().st_size <= 0:
        return fail(f"empty {path.relative_to(ROOT)}")
    return 0


def run_node_check(js_path: Path, wasm_path: Path) -> int:
    node = shutil.which("node")
    if not node:
        return fail("node is required for JavaScript syntax and WebAssembly.compile checks")

    syntax = subprocess.run([node, "--check", str(js_path)], cwd=ROOT)
    if syntax.returncode != 0:
        return syntax.returncode

    compile_script = (
        "const fs=require('fs');"
        f"WebAssembly.compile(fs.readFileSync({str(wasm_path)!r}))"
        ".then(()=>console.log('wasm compile ok'))"
        ".catch(err=>{console.error(err);process.exit(1);});"
    )
    compiled = subprocess.run([node, "-e", compile_script], cwd=ROOT)
    return compiled.returncode


def check_js_asset_references(js_path: Path) -> int:
    text = js_path.read_text(encoding="utf-8", errors="ignore")
    for basename in ["super-mango.wasm", "super-mango.data"]:
        if basename not in text:
            return fail(f"{js_path.relative_to(ROOT)} does not reference {basename}")
    return 0


def check_zip(zip_path: Path, required: bool) -> int:
    if not zip_path.exists():
        if required:
            return fail(f"missing {zip_path.relative_to(ROOT)}")
        print(f"wasm artifact check: {zip_path.relative_to(ROOT)} not present; skipping zip inspection")
        return 0
    with zipfile.ZipFile(zip_path) as archive:
        names = set(archive.namelist())
        missing = [name for name in REQUIRED_ZIP if name not in names]
    if missing:
        return fail(f"{zip_path.relative_to(ROOT)} missing entries: {', '.join(missing)}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", default="out", help="directory containing make web output")
    parser.add_argument("--zip", help="wasm release zip to inspect; fails if the explicitly supplied zip is missing")
    args = parser.parse_args()

    out_dir = ROOT / args.out_dir
    zip_path = ROOT / args.zip if args.zip else ROOT / "dist/super-mango-wasm.zip"

    for name in REQUIRED_OUT:
        rc = require_nonempty(out_dir / name)
        if rc != 0:
            return rc

    rc = check_js_asset_references(out_dir / "super-mango.js")
    if rc != 0:
        return rc
    rc = run_node_check(out_dir / "super-mango.js", out_dir / "super-mango.wasm")
    if rc != 0:
        return rc
    rc = check_zip(zip_path, required=args.zip is not None)
    if rc != 0:
        return rc

    print("wasm artifact check: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
