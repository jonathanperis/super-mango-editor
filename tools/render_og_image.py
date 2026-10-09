#!/usr/bin/env python3
"""Render docs/public/og-image.png, the site's link preview picture.

Screenshots tools/og-image.html at 1200x630 with headless Google Chrome. A
manual tool, not part of any build: run `python3 tools/render_og_image.py`
from the repository root after changing the template, and commit the image
it writes. Needs Chrome (set CHROME to its path if it is not found) and the
website's font packages: the template loads Pixelify Sans from
docs/node_modules, so run `bun install` in docs/ first. No network is used.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "tools" / "og-image.html"
OUTPUT = ROOT / "docs" / "public" / "og-image.png"
FONT_CSS = ROOT / "docs" / "node_modules" / "@fontsource" / "pixelify-sans" / "500.css"
WIDTH, HEIGHT = 1200, 630

CANDIDATES = [
    "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
    "google-chrome",
    "google-chrome-stable",
    "chromium",
    "chromium-browser",
]


def find_chrome() -> str:
    for name in [os.environ.get("CHROME", "")] + CANDIDATES:
        if name and (Path(name).is_file() or shutil.which(name)):
            return name
    sys.exit("Google Chrome not found; set CHROME to its executable")


def main() -> None:
    # Without the font Chrome quietly falls back to monospace, and the
    # picture would look right enough to be committed by mistake.
    if not FONT_CSS.is_file():
        sys.exit(f"{FONT_CSS.relative_to(ROOT)} not found; run `bun install` in docs/ first")
    subprocess.run(
        [
            find_chrome(),
            "--headless=new",
            "--disable-gpu",
            "--hide-scrollbars",
            # The template is a local file that loads local fonts and sprites.
            "--allow-file-access-from-files",
            "--force-device-scale-factor=1",
            f"--window-size={WIDTH},{HEIGHT}",
            # Lets the web font finish loading before the shot is taken.
            "--virtual-time-budget=10000",
            f"--screenshot={OUTPUT}",
            TEMPLATE.as_uri(),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    print("generated", OUTPUT)


if __name__ == "__main__":
    main()
