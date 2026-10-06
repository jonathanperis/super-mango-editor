#!/usr/bin/env python3
"""Pin a built Emscripten shell's inline boot script in its CSP.

web/shell.html carries a Content-Security-Policy meta tag whose script-src
holds a placeholder hash. emcc minifies the shell while linking, so the inline
script's final bytes (and therefore its SHA-256) are only known afterwards.
This tool rewrites the placeholder with the hash of every inline script in a
built page, or with --check verifies that a built page is already pinned.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import re
import sys
from pathlib import Path

PLACEHOLDER = "'sha256-MANGO_INLINE_SCRIPT_HASHES'"
# Scripts with a src attribute are covered by 'self'; only inline bodies need
# hashes. Data blocks such as JSON-LD never execute, so they need none.
INLINE_SCRIPT = re.compile(r"<script((?![^>]*\bsrc\s*=)[^>]*)>(.*?)</script\s*>", re.S | re.I)
SCRIPT_TYPE = re.compile(r"\btype\s*=\s*[\"']?([^\"'\s>]+)", re.I)
JS_TYPES = {"module", "text/javascript", "application/javascript"}
CSP_META = re.compile(r"<meta\b[^>]*\bhttp-equiv\s*=\s*\"?Content-Security-Policy\"?[^>]*>", re.I)


def inline_script_hashes(html: str) -> list[str]:
    """Return CSP source expressions for each inline script, in page order."""
    hashes = []
    for attrs, body in INLINE_SCRIPT.findall(html):
        script_type = SCRIPT_TYPE.search(attrs)
        if script_type and script_type.group(1).lower() not in JS_TYPES:
            continue
        digest = hashlib.sha256(body.encode("utf-8")).digest()
        hashes.append("'sha256-" + base64.b64encode(digest).decode("ascii") + "'")
    return hashes


def problems(html: str) -> list[str]:
    """Describe why a built page's CSP does not exactly cover its inline scripts."""
    metas = CSP_META.findall(html)
    if len(metas) != 1:
        return [f"expected one Content-Security-Policy meta tag, found {len(metas)}"]
    if PLACEHOLDER in html:
        return ["CSP placeholder hash was not replaced"]
    errors = []
    for expected in inline_script_hashes(html):
        if expected not in metas[0]:
            errors.append(f"CSP does not allow inline script {expected}")
    if "unsafe-eval'" in metas[0].replace("wasm-unsafe-eval'", ""):
        errors.append("CSP must not allow JavaScript eval")
    return errors


def pin(path: Path) -> None:
    html = path.read_text(encoding="utf-8")
    hashes = inline_script_hashes(html)
    if html.count(PLACEHOLDER) != 1 or not hashes:
        raise SystemExit(f"web_csp: {path}: expected one CSP placeholder and an inline boot script")
    path.write_text(html.replace(PLACEHOLDER, " ".join(hashes)), encoding="utf-8", newline="")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("html", nargs="+", type=Path, help="built Emscripten HTML page(s)")
    parser.add_argument("--check", action="store_true", help="verify instead of rewriting")
    args = parser.parse_args()
    failed = False
    for path in args.html:
        if not args.check:
            pin(path)
        for problem in problems(path.read_text(encoding="utf-8")):
            print(f"web_csp: {path}: {problem}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
