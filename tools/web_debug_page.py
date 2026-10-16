#!/usr/bin/env python3
"""Derive the debug Web page from the built normal page.

The normal and debug pages run the same program; the debug page only sets
Module.__superMangoDebug, which makes web/shell.html pass --debug at boot.
Linking the program twice produced byte-identical .wasm and .data files, so
the debug page now loads the normal page's super-mango.js (and through it
super-mango.wasm and super-mango.data). This tool copies the built, CSP-pinned
super-mango.html, inserts web/debug-boot.js as an inline script just before
the Emscripten <script> tag, and pins the CSP again so it also allows the new
inline script.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from web_csp import CSP_META, PLACEHOLDER, inline_script_hashes, pin

ROOT = Path(__file__).resolve().parents[1]
DEBUG_BOOT = ROOT / "web/debug-boot.js"
# emcc minifies the shell, so the attribute order and quoting can vary.
MAIN_SCRIPT = re.compile(r"<script\b[^>]*\bsrc\s*=\s*[\"']?super-mango\.js[\"']?[^>]*>", re.I)


def debug_page(html: str, marker: str) -> str:
    """Return the debug page for a pinned normal page."""
    if "</script" in marker.lower():
        raise SystemExit("web_debug_page: web/debug-boot.js must not contain </script")
    metas = CSP_META.findall(html)
    hashes = " ".join(inline_script_hashes(html))
    if len(metas) != 1 or not hashes or hashes not in metas[0]:
        raise SystemExit("web_debug_page: the normal page's CSP is not pinned")
    if len(MAIN_SCRIPT.findall(html)) != 1:
        raise SystemExit("web_debug_page: expected one <script src=super-mango.js> tag")
    # Put the placeholder back so web_csp.pin hashes every inline script,
    # the inserted one included.
    html = html.replace(metas[0], metas[0].replace(hashes, PLACEHOLDER), 1)
    return MAIN_SCRIPT.sub(lambda tag: f"<script>{marker}</script>{tag.group(0)}", html, count=1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("page", type=Path, help="built and pinned super-mango.html")
    parser.add_argument("output", type=Path, help="debug page to write")
    args = parser.parse_args()
    html = args.page.read_text(encoding="utf-8")
    marker = DEBUG_BOOT.read_text(encoding="utf-8")
    args.output.write_text(debug_page(html, marker), encoding="utf-8", newline="")
    pin(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
