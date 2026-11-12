#!/usr/bin/env python3
"""Prepare the standalone Web page's shell and stamp its file version.

web/shell.html is the page Emscripten wraps around the game. Two things in
it are only known when the page is built, so the Makefile's `web` target
runs this tool around the emcc link:

  template  (before emcc) writes the shell emcc will use. It pastes
            web/boot.js, the boot sequence shared with the website's home
            page, in place of the /*MANGO_BOOT_JS*/ marker, and replaces
            every @MANGO_ASSET_QUERY@ with "?v=<version>" (or with nothing
            when no version is given).

  stamp     (after emcc) adds the same "?v=<version>" to the script tag
            for super-mango.js. emcc writes that tag itself, so it cannot
            be part of the template.

The version is the commit being built (CI passes WEB_ASSET_VERSION). With
it, the page asks for super-mango.js?v=<commit>, super-mango.wasm?v=<commit>
and super-mango.data?v=<commit>. A browser that cached the files of an
older deploy then sees new addresses and cannot pair an old .js with a new
.wasm. Local builds pass no version and keep the plain names.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHELL = ROOT / "web/shell.html"
BOOT = ROOT / "web/boot.js"
BOOT_MARKER = "/*MANGO_BOOT_JS*/"
QUERY_MARKER = "@MANGO_ASSET_QUERY@"
# A commit hash or a release number. Nothing that could end an HTML
# attribute or a JavaScript string, because the text is pasted into both.
VERSION = re.compile(r"[0-9A-Za-z._-]{1,64}")
# Text that would end, or confuse the browser inside, the HTML script element
# that boot.js is pasted into. It is refused even inside a JavaScript comment.
FORBIDDEN_IN_SCRIPT = ("</script", "<script", "<!--")
# emcc minifies the shell, so the attribute may be quoted or not.
MAIN_SCRIPT_SRC = re.compile(r"(<script\b[^>]*?\bsrc\s*=\s*)([\"']?)super-mango\.js\2(?=[\s>])", re.I)


def asset_query(version: str) -> str:
    """'' for no version, else '?v=<version>' after checking its characters."""
    version = version.strip()
    if not version:
        return ""
    if not VERSION.fullmatch(version):
        raise SystemExit(f"web_shell: version {version!r} may only hold letters, digits, '.', '_' and '-'")
    return "?v=" + version


def template(shell: str, boot: str, version: str) -> str:
    """The shell emcc links with: boot.js pasted in, version markers filled."""
    lowered = boot.lower()
    for text in FORBIDDEN_IN_SCRIPT:
        if text in lowered:
            raise SystemExit(f"web_shell: web/boot.js must not contain {text!r}, not even in a comment")
    if shell.count(BOOT_MARKER) != 1:
        raise SystemExit(f"web_shell: web/shell.html must contain {BOOT_MARKER} exactly once")
    if QUERY_MARKER not in shell:
        raise SystemExit(f"web_shell: web/shell.html has no {QUERY_MARKER} marker")
    if QUERY_MARKER in boot or BOOT_MARKER in boot:
        raise SystemExit("web_shell: web/boot.js must not contain the shell's markers")
    page = shell.replace(BOOT_MARKER, boot.strip("\n"))
    return page.replace(QUERY_MARKER, asset_query(version))


def stamp(html: str, version: str) -> str:
    """Add ?v=<version> to the one script tag that loads super-mango.js."""
    query = asset_query(version)
    if len(MAIN_SCRIPT_SRC.findall(html)) != 1:
        raise SystemExit("web_shell: expected one script tag with src=super-mango.js in the built page")
    if not query:
        return html
    # Always quoted afterwards: "=" is not allowed in an unquoted attribute.
    return MAIN_SCRIPT_SRC.sub(lambda tag: f'{tag.group(1)}"super-mango.js{query}"', html, count=1)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    make = commands.add_parser("template", help="write the shell emcc links with")
    make.add_argument("--version", default="", help="commit or release being built; empty for none")
    make.add_argument("--output", type=Path, required=True)
    mark = commands.add_parser("stamp", help="version the script tag of a built page")
    mark.add_argument("--version", default="")
    mark.add_argument("page", type=Path, help="the page emcc wrote")
    args = parser.parse_args(argv)
    if args.command == "template":
        page = template(SHELL.read_text(encoding="utf-8"), BOOT.read_text(encoding="utf-8"), args.version)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(page, encoding="utf-8", newline="")
    else:
        html = args.page.read_text(encoding="utf-8")
        args.page.write_text(stamp(html, args.version), encoding="utf-8", newline="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
