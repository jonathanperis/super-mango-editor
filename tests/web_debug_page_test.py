#!/usr/bin/env python3
"""The debug Web page is derived from the normal page and shares its program."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import web_csp  # noqa: E402
import web_debug_page  # noqa: E402
import web_shell  # noqa: E402


def main() -> None:
    # The page as the Makefile builds it: tools/web_shell.py pastes web/boot.js
    # into the shell and fills in the file version, emcc replaces its
    # placeholder with the program's (minified) script tag, and the same tool
    # then versions that tag.
    shell = web_shell.template((ROOT / "web/shell.html").read_text(encoding="utf-8"),
                               (ROOT / "web/boot.js").read_text(encoding="utf-8"), "abc123")
    built = web_shell.stamp(shell.replace("{{{ SCRIPT }}}", "<script async src=super-mango.js></script>"),
                            "abc123")
    with tempfile.TemporaryDirectory() as tmp:
        normal, debug = Path(tmp) / "super-mango.html", Path(tmp) / "super-mango-debug.html"
        normal.write_text(built, encoding="utf-8")
        web_csp.pin(normal)
        before = normal.read_text(encoding="utf-8")
        sys.argv = ["web_debug_page.py", str(normal), str(debug)]
        assert web_debug_page.main() == 0
        page = debug.read_text(encoding="utf-8")
        assert normal.read_text(encoding="utf-8") == before, "the normal page changed"
    assert web_csp.problems(page) == [], web_csp.problems(page)
    assert len(web_csp.inline_script_hashes(page)) == 2, "debug marker is not an allowed inline script"
    marker, program = page.find("__superMangoDebug = true"), page.find('src="super-mango.js?v=abc123"')
    assert 0 <= marker < program, "debug marker must run before super-mango.js"
    assert page.count("src=") == 1 and "super-mango-debug.js" not in page
    # The debug page loads the very same versioned files as the normal one.
    assert page.count("?v=abc123") == before.count("?v=abc123") == 4
    # The shell's own boot script is untouched, so its hash still applies.
    assert web_csp.inline_script_hashes(before)[0] in page
    try:
        web_debug_page.debug_page(built, "x")
    except SystemExit:
        pass
    else:
        raise AssertionError("derived a debug page from an unpinned normal page")
    print("web_debug_page_test: ok")


if __name__ == "__main__":
    main()
