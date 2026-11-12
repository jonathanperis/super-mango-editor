#!/usr/bin/env python3
"""tools/web_shell.py builds the shell emcc links with and versions the page."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import web_shell  # noqa: E402


def refused(action, label: str) -> None:
    try:
        action()
    except SystemExit:
        return
    raise AssertionError(label)


def main() -> None:
    shell = (ROOT / "web/shell.html").read_text(encoding="utf-8")
    boot = (ROOT / "web/boot.js").read_text(encoding="utf-8")

    # ── template: the real files, with and without a version ───────────────
    plain = web_shell.template(shell, boot, "")
    assert "MANGO_BOOT_JS" not in plain and "@MANGO_ASSET_QUERY@" not in plain
    assert "root.SuperMangoBoot = api;" in plain, "web/boot.js was not pasted in"
    assert plain.index("root.SuperMangoBoot = api;") < plain.index("window.SuperMangoBoot.start(")
    assert 'href="super-mango.wasm" as="fetch"' in plain and "assetQuery: ''" in plain
    # emcc needs its own placeholder to survive, and exactly one inline script.
    assert plain.count("{{{ SCRIPT }}}") == 1 and plain.count("<script>") == 1

    versioned = web_shell.template(shell, boot, " 3f2a9c1 \n")
    assert 'href="super-mango.wasm?v=3f2a9c1" as="fetch"' in versioned
    assert 'href="super-mango.data?v=3f2a9c1" as="fetch"' in versioned
    assert "assetQuery: '?v=3f2a9c1'" in versioned
    assert versioned.replace("?v=3f2a9c1", "") == plain, "the version changed more than the addresses"

    # A version is pasted into HTML and JavaScript, so its characters are limited.
    assert web_shell.asset_query("") == "" and web_shell.asset_query("v2.0.9_rc-1") == "?v=v2.0.9_rc-1"
    for bad in ("a b", "x'y", 'x"y', "a<b", "a&b", "é", "x" * 65):
        refused(lambda bad=bad: web_shell.asset_query(bad), f"accepted the version {bad!r}")

    # Broken inputs stop the build instead of producing a half-made page.
    refused(lambda: web_shell.template(shell.replace(web_shell.BOOT_MARKER, ""), boot, ""),
            "accepted a shell without the boot marker")
    refused(lambda: web_shell.template(shell + web_shell.BOOT_MARKER, boot, ""),
            "accepted a shell with two boot markers")
    refused(lambda: web_shell.template(shell.replace(web_shell.QUERY_MARKER, ""), boot, ""),
            "accepted a shell without a version marker")
    for text in ("// </SCRIPT>", "/* <script src=x> */", "// <!-- note"):
        refused(lambda text=text: web_shell.template(shell, boot + text, ""),
                f"pasted a boot script containing {text!r}")
    refused(lambda: web_shell.template(shell, boot + "// @MANGO_ASSET_QUERY@", ""),
            "pasted a boot script containing a shell marker")

    # ── stamp: the script tag emcc writes, in the forms its minifier uses ──
    for tag in ("<script async src=super-mango.js></script>",
                '<script async type="text/javascript" src="super-mango.js"></script>',
                "<script src='super-mango.js' async></script>"):
        page = "<body><script>boot()</script>" + tag + "</body>"
        assert web_shell.stamp(page, "") == page, "no version must leave the page alone"
        stamped = web_shell.stamp(page, "3f2a9c1")
        assert 'src="super-mango.js?v=3f2a9c1"' in stamped, stamped
        assert stamped.count("super-mango.js") == 1 and "<script>boot()</script>" in stamped
        assert stamped.replace('"super-mango.js?v=3f2a9c1"', "X") == \
            page.replace("'super-mango.js'", "X").replace('"super-mango.js"', "X").replace("=super-mango.js", "=X")
    refused(lambda: web_shell.stamp("<body></body>", "1"), "stamped a page without the script tag")
    refused(lambda: web_shell.stamp("<script src=super-mango.js></script>" * 2, "1"),
            "stamped a page with two script tags")
    # Another file whose name merely starts the same way is not the program.
    refused(lambda: web_shell.stamp("<script src=super-mango.js.map></script>", "1"),
            "took super-mango.js.map for the program")

    # ── the command line, as the Makefile calls it ─────────────────────────
    with tempfile.TemporaryDirectory() as tmp:
        output = Path(tmp) / "obj" / "web" / "shell.html"
        assert web_shell.main(["template", "--version", "abc", "--output", str(output)]) == 0
        assert output.read_text(encoding="utf-8") == web_shell.template(shell, boot, "abc")
        built = Path(tmp) / "super-mango.html"
        built.write_text(output.read_text(encoding="utf-8").replace(
            "{{{ SCRIPT }}}", "<script async src=super-mango.js></script>"), encoding="utf-8")
        assert web_shell.main(["stamp", "--version", "abc", str(built)]) == 0
        assert '<script async src="super-mango.js?v=abc"></script>' in built.read_text(encoding="utf-8")
    print("web_shell_test: ok")


if __name__ == "__main__":
    main()
