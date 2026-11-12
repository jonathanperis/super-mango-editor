#!/usr/bin/env python3
"""tools/check_wasm_artifacts.py accepts a sound Web build and names a broken one.

No Emscripten is needed: the test assembles a tiny stand-in build (the real
shell page made by the real tools, a few bytes of JavaScript, and the
smallest valid WebAssembly module), then breaks one thing at a time.
"""

from __future__ import annotations

import os
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import check_wasm_artifacts as check  # noqa: E402
import web_csp  # noqa: E402
import web_debug_page  # noqa: E402
import web_shell  # noqa: E402

# "\0asm" and version 1: a module with nothing in it, which still compiles.
EMPTY_WASM = b"\x00asm\x01\x00\x00\x00"
# The names the checker looks for in super-mango.js. A comment is enough,
# and keeps the file harmless for a tool that runs it instead of parsing it.
GOOD_JS = ("// super-mango.wasm super-mango.data SuperMangoTouch SuperMangoKeyboard\n"
           "// _game_web_input_touch super-mango-profile-v2 navigator.locks\n")


def build(folder: Path, version: str = "abc123") -> None:
    """Write the five files of a Web build into folder."""
    shell = web_shell.template((ROOT / "web/shell.html").read_text(encoding="utf-8"),
                               (ROOT / "web/boot.js").read_text(encoding="utf-8"), version)
    page = web_shell.stamp(shell.replace("{{{ SCRIPT }}}", "<script async src=super-mango.js></script>"),
                           version)
    normal = folder / "super-mango.html"
    normal.write_text(page, encoding="utf-8")
    web_csp.pin(normal)
    debug = folder / "super-mango-debug.html"
    marker = (ROOT / "web/debug-boot.js").read_text(encoding="utf-8")
    debug.write_text(web_debug_page.debug_page(normal.read_text(encoding="utf-8"), marker), encoding="utf-8")
    web_csp.pin(debug)
    (folder / "super-mango.js").write_text(GOOD_JS, encoding="utf-8")
    (folder / "super-mango.wasm").write_bytes(EMPTY_WASM)
    (folder / "super-mango.data").write_bytes(b"data")


def run(folder: Path, *extra: str) -> tuple[int, str]:
    """(exit code, everything printed) of the checker on one folder.

    The checker starts Node, which writes straight to the terminal, so the
    output is captured at the file-descriptor level (1 = stdout, 2 = stderr)
    rather than by swapping sys.stdout. Otherwise the error Node prints for
    the deliberately broken module below would look like a failing test.
    """
    argv, sys.argv = sys.argv, ["check_wasm_artifacts.py", "--out-dir", str(folder), *extra]
    sys.stdout.flush()
    sys.stderr.flush()
    saved = os.dup(1), os.dup(2)
    with tempfile.TemporaryFile() as captured:
        os.dup2(captured.fileno(), 1)
        os.dup2(captured.fileno(), 2)
        try:
            code = check.main()
        finally:
            sys.stdout.flush()
            sys.stderr.flush()
            os.dup2(saved[0], 1)
            os.dup2(saved[1], 2)
            os.close(saved[0])
            os.close(saved[1])
            sys.argv = argv
        captured.seek(0)
        return code, captured.read().decode("utf-8", errors="replace")


def edit(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    assert old in text, f"test is stale: {old!r} is not in {path.name}"
    path.write_text(text.replace(old, new), encoding="utf-8")


def broken(change, needle: str, version: str = "abc123") -> None:
    """Build a good folder, apply one change, expect a failure naming needle."""
    with tempfile.TemporaryDirectory() as tmp:
        folder = Path(tmp)
        build(folder, version)
        change(folder)
        code, output = run(folder, "--zip", str(folder / "missing.zip"))
        assert code != 0, f"accepted a build that should fail with: {needle}"
        assert needle in output, f"expected {needle!r} in:\n{output}"


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        folder = Path(tmp)
        for version in ("abc123", ""):
            build(folder, version)
            code, output = run(folder)
            assert code == 0 and "wasm artifact check: ok" in output, output
        # The page-level checks, called directly.
        assert check.check_asset_version(folder / "super-mango.html") == 0
        assert check.check_debug_page(folder / "super-mango-debug.html") == 0

        # A release zip must hold the five files and the notices.
        archive = folder / "release.zip"
        with zipfile.ZipFile(archive, "w") as package:
            for name in check.REQUIRED_ZIP:
                package.writestr(name, "x")
        code, output = run(folder, "--zip", str(archive))
        assert code == 0, output
        with zipfile.ZipFile(archive, "w") as package:
            for name in check.REQUIRED_ZIP[:-1]:
                package.writestr(name, "x")
        code, output = run(folder, "--zip", str(archive))
        assert code != 0 and "missing entries: " + check.REQUIRED_ZIP[-1] in output, output
        # An explicitly named zip that is absent is an error, not a skip;
        # the message shows the path as given because it is outside the repository.
        code, output = run(folder, "--zip", str(folder / "absent.zip"))
        assert code != 0 and f"missing {folder / 'absent.zip'}" in output, output
    assert check.display(ROOT / "out" / "x.html") == "out/x.html"

    broken(lambda f: (f / "super-mango.data").unlink(), "missing ")
    broken(lambda f: (f / "super-mango.wasm").write_bytes(b""), "empty ")
    broken(lambda f: (f / "super-mango.wasm").write_bytes(b"not wasm"), "")
    broken(lambda f: edit(f / "super-mango.js", "SuperMangoKeyboard", "x"),
           "missing host interface SuperMangoKeyboard")
    broken(lambda f: edit(f / "super-mango.js", "super-mango.data", "x"), "does not reference super-mango.data")
    broken(lambda f: edit(f / "super-mango.js", "//", "// _emscripten_sleep"), "unsupported emscripten_sleep")
    # The CSP no longer covers the inline script once that script changes.
    broken(lambda f: edit(f / "super-mango.html", "injectScript: false", "injectScript: !1"),
           "CSP does not allow inline script")
    # Old and new files must not be mixable: one version everywhere.
    broken(lambda f: edit(f / "super-mango.html", 'href="super-mango.wasm?v=abc123"', 'href="super-mango.wasm"'),
           "names game files with different versions")
    broken(lambda f: edit(f / "super-mango.html", 'src="super-mango.js?v=abc123"', 'src="super-mango.js?v=old"'),
           "names game files with different versions")
    broken(lambda f: edit(f / "super-mango-debug.html", 'src="super-mango.js?v=abc123"', 'src="super-mango.js"'),
           "names game files with different versions")
    broken(lambda f: edit(f / "super-mango.html", '<link rel="preload" href="super-mango.data" as="fetch" crossorigin>', ""),
           "does not preload super-mango.data", version="")
    # The debug page must set its marker before the program loads, and must
    # not bring a second program. (Called directly: in a whole run the CSP
    # check would object to the edited inline script first.)
    with tempfile.TemporaryDirectory() as tmp:
        folder = Path(tmp)
        build(folder)
        debug = folder / "super-mango-debug.html"
        good = debug.read_text(encoding="utf-8")
        assert good.index("__superMangoDebug = true") < good.index('src="super-mango.js')
        # The checker reports on stderr; keep these expected messages quiet.
        stderr, sys.stderr = sys.stderr, open(os.devnull, "w", encoding="utf-8")
        try:
            late = good.replace("window.Module.__superMangoDebug = true;", "") + \
                "<script>window.Module.__superMangoDebug = true;</script>"
            debug.write_text(late, encoding="utf-8")
            assert check.check_debug_page(debug) == 1, "accepted a marker that runs after the program"
            debug.write_text(good.replace("super-mango.js?v=abc123", "super-mango-debug.js"), encoding="utf-8")
            assert check.check_debug_page(debug) == 1, "accepted a separate debug program"
        finally:
            sys.stderr.close()
            sys.stderr = stderr
    print("wasm_artifacts_test: ok")


if __name__ == "__main__":
    main()
