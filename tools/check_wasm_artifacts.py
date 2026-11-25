#!/usr/bin/env python3
"""Verify generated WebAssembly artifacts and optional release zip contents."""

from __future__ import annotations

import argparse
import gzip
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path
from package_release import WASM_FILES
from validate_levels import load_level, loaded_music_path
from web_csp import problems as csp_problems

try:  # Not in the standard library; used when installed, for information.
    import brotli  # type: ignore[import-not-found]
except ImportError:
    brotli = None

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_OUT = WASM_FILES
REQUIRED_ZIP = [f"super-mango-wasm/{name}" for name in WASM_FILES] + [
    "super-mango-wasm/README.txt",
    "super-mango-wasm/LICENSE",
    "super-mango-wasm/THIRD_PARTY_NOTICES.md",
    "super-mango-wasm/licenses/tomlc17.txt",
]


def display(path: Path) -> str:
    """Show a path relative to the repository when it is inside it.

    Path.relative_to() raises ValueError for a path elsewhere, such as an
    absolute --out-dir or --zip under /tmp, which would hide the real
    message behind a traceback; those paths are shown as given.
    """
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def fail(message: str) -> int:
    print(f"wasm artifact check failed: {message}", file=sys.stderr)
    return 1


# What a player downloads before the game starts, compressed as a web server
# sends it. gzip (level 9) is measured because Python always has it; GitHub
# Pages serves gzip. Each budget is the size when it was set plus about 10%,
# so ordinary changes pass and a large jump (a new dependency, a debug
# build, an asset that should not ship) fails. Raise one deliberately, with
# the reason in the commit message. Brotli sizes are printed when the
# brotli module is installed, but not checked.
GZIP_BUDGETS = {
    "super-mango.wasm": 185_000,  # 168.6 KB when set
    "super-mango.js": 35_000,     # 31.1 KB (47.0 KB before Closure, see the Makefile)
    "super-mango.data": 225_000,  # 204.7 KB (unplayed music left out)
}


def compressed_sizes(path: Path) -> tuple[int, int, int | None]:
    """(raw, gzip -9, brotli or None) sizes of one file, in bytes."""
    data = path.read_bytes()
    packed = len(gzip.compress(data, compresslevel=9, mtime=0))
    return len(data), packed, len(brotli.compress(data)) if brotli else None


def check_size_budgets(out_dir: Path) -> int:
    for name, budget in GZIP_BUDGETS.items():
        raw, packed, br = compressed_sizes(out_dir / name)
        shown = f", brotli {br:,}" if br is not None else ""
        print(f"wasm artifact size: {name}: {raw:,} bytes, gzip {packed:,} (budget {budget:,}){shown}")
        if packed > budget:
            return fail(f"{display(out_dir / name)} is {packed:,} bytes gzipped, over its "
                        f"{budget:,}-byte budget (GZIP_BUDGETS in tools/check_wasm_artifacts.py)")
    return 0


def played_music(levels_dir: Path) -> set[str]:
    """Every music file a level in levels/ (labs included) plays."""
    tracks = set()
    for level in sorted(levels_dir.glob("*.toml")) + sorted(levels_dir.glob("labs/*.toml")):
        music = load_level(level).get("music_path", "")
        if music:
            tracks.add(loaded_music_path(music))
    return tracks


def check_packaged_music(js_path: Path, levels_dir: Path) -> int:
    """The Makefile leaves unplayed music out of super-mango.data; make sure
    it never leaves out a track a level plays. super-mango.js lists every
    packaged file by its path from the root, "/assets/...". """
    text = js_path.read_text(encoding="utf-8", errors="ignore")
    for track in sorted(played_music(levels_dir)):
        if f"/{track}" not in text:
            return fail(f"{display(js_path)} does not package {track}, which a level plays")
    return 0


def require_nonempty(path: Path) -> int:
    if not path.is_file():
        return fail(f"missing {display(path)}")
    if path.stat().st_size <= 0:
        return fail(f"empty {display(path)}")
    return 0


def run_node_check(js_path: Path, wasm_path: Path) -> int:
    node = shutil.which(os.environ.get("NODE", "node"))
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
    # This non-Asyncify app returns each frame to the browser. raylib's Web
    # WindowShouldClose pulls in this unsupported sleep even though the files
    # compile and download correctly; reject that dependency before shipping.
    if "_emscripten_sleep" in text:
        return fail(f"{display(js_path)} depends on unsupported emscripten_sleep")
    for basename in [f"{js_path.stem}.wasm", f"{js_path.stem}.data"]:
        if basename not in text:
            return fail(f"{display(js_path)} does not reference {basename}")
    for interface in ["SuperMangoTouch", "SuperMangoKeyboard", "_game_web_input_touch", "super-mango-profile-v2", "navigator.locks"]:
        if interface not in text:
            return fail(f"{display(js_path)} is missing host interface {interface}")
    return 0


# The script tag emcc writes for the program, with or without the "?v=<commit>"
# that tools/web_shell.py adds, quoted or not (emcc minifies the page).
MAIN_SCRIPT = re.compile(r"<script\b[^>]*\bsrc\s*=\s*[\"']?super-mango\.js(?:\?v=[0-9A-Za-z._-]+)?[\"']?[\s>]", re.I)
# Every game file a tag of the page loads (src= or href=), with its optional
# version. The bare names inside the inline boot script are not addresses:
# the script joins them with assetQuery when it runs.
ASSET_REFERENCE = re.compile(
    r"\b(?:src|href)\s*=\s*[\"']?super-mango\.(?:js|wasm|data)(\?v=[0-9A-Za-z._-]+)?", re.I)
LINK_TAG = re.compile(r"<link\b[^>]*>", re.I)
ASSET_QUERY_SETTING = re.compile(r"assetQuery\s*:\s*[\"'](\?v=[0-9A-Za-z._-]+)?[\"']")


def check_debug_page(html_path: Path) -> int:
    # The debug page shares the normal page's program: it must load
    # super-mango.js and set the debug marker before that script runs.
    text = html_path.read_text(encoding="utf-8")
    marker = text.find("__superMangoDebug = true")
    program = MAIN_SCRIPT.search(text)
    if marker < 0 or not program or marker > program.start():
        return fail(f"{display(html_path)} must set the debug marker before loading super-mango.js")
    if "super-mango-debug.js" in text:
        return fail(f"{display(html_path)} still loads a separate debug program")
    return 0


def check_asset_version(html_path: Path) -> int:
    """One version for every game file a page names.

    The page asks for super-mango.js, .wasm and .data as <name>?v=<commit>
    so that a browser holding an older deploy cannot mix old and new files.
    That only works when the script tag, the two preload links and the
    assetQuery the boot script appends all carry the same text; a preload
    with another address would also be downloaded and then thrown away.
    """
    text = html_path.read_text(encoding="utf-8")
    queries = {match.group(1) or "" for match in ASSET_REFERENCE.finditer(text)}
    setting = ASSET_QUERY_SETTING.search(text)
    if not setting:
        return fail(f"{display(html_path)} does not pass assetQuery to the boot script")
    queries.add(setting.group(1) or "")
    if len(queries) != 1:
        shown = ", ".join(sorted(query or "(none)" for query in queries))
        return fail(f"{display(html_path)} names game files with different versions: {shown}")
    preloads = [tag for tag in LINK_TAG.findall(text) if re.search(r"\brel\s*=\s*[\"']?preload", tag, re.I)]
    for name in ("super-mango.wasm", "super-mango.data"):
        if not any(name in tag for tag in preloads):
            return fail(f"{display(html_path)} does not preload {name}")
    return 0


def check_zip(zip_path: Path, required: bool) -> int:
    if not zip_path.exists():
        if required:
            return fail(f"missing {display(zip_path)}")
        print(f"wasm artifact check: {display(zip_path)} not present; skipping zip inspection")
        return 0
    with zipfile.ZipFile(zip_path) as archive:
        names = set(archive.namelist())
        missing = [name for name in REQUIRED_ZIP if name not in names]
    if missing:
        return fail(f"{display(zip_path)} missing entries: {', '.join(missing)}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", default="out", help="directory containing make web output")
    parser.add_argument("--zip", help="wasm release zip to inspect; fails if the explicitly supplied zip is missing")
    parser.add_argument("--levels", default="levels", help="levels the build packages (for the music check)")
    args = parser.parse_args()

    out_dir = ROOT / args.out_dir
    zip_path = ROOT / args.zip if args.zip else ROOT / "dist/super-mango-wasm.zip"

    for name in REQUIRED_OUT:
        rc = require_nonempty(out_dir / name)
        if rc != 0:
            return rc

    for name in ("super-mango.html", "super-mango-debug.html"):
        html = out_dir / name
        for problem in csp_problems(html.read_text(encoding="utf-8")):
            return fail(f"{display(html)}: {problem}")
    rc = check_debug_page(out_dir / "super-mango-debug.html")
    if rc != 0:
        return rc
    for name in ("super-mango.html", "super-mango-debug.html"):
        rc = check_asset_version(out_dir / name)
        if rc != 0:
            return rc
    rc = check_js_asset_references(out_dir / "super-mango.js")
    if rc != 0:
        return rc
    rc = check_packaged_music(out_dir / "super-mango.js", ROOT / args.levels)
    if rc != 0:
        return rc
    rc = check_size_budgets(out_dir)
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
