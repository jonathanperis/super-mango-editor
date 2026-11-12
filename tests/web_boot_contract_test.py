#!/usr/bin/env python3
"""tools/check_web_boot_contract.py rejects boot logic outside web/boot.js.

The repository's own files must pass. Then each case edits one file's text
the way a real regression would, and the checker must name the problem.
The last cases rebuild the two divergences that once shipped: a standalone
page with its own Module object, and a home page with its own handlers.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import check_web_boot_contract as contract  # noqa: E402


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def expect(problems: list[str], needle: str) -> None:
    assert any(needle in problem for problem in problems), f"no problem mentions {needle!r}: {problems}"


def replaced(text: str, old: str, new: str) -> str:
    assert old in text, f"test is stale: {old!r} is not in the file"
    return text.replace(old, new, 1)


def main() -> None:
    boot, shell = read(contract.BOOT), read(contract.SHELL)
    dashboard, marker = read(contract.DASHBOARD), read(contract.DEBUG_MARKER)
    assert contract.all_problems() == [], contract.all_problems()

    # ── web/boot.js loses a handler ────────────────────────────────────────
    expect(contract.boot_problems(replaced(boot, "canvas.addEventListener('webglcontextlost'", "canvas.addEventListener('contextlost'")),
           "handles a lost WebGL context")
    expect(contract.boot_problems(boot.replace("script.onerror", "script.onload")),
           "failed download of the script it injects")
    expect(contract.boot_problems(replaced(boot, "locateFile:", "findFile:")), "versions the .wasm and .data")
    expect(contract.boot_problems(boot + "\nmoduleConfig.callMain([]);\n"), "main exactly once")
    expect(contract.boot_problems(boot + "\n// see <script src=x>\n"), "pasted inside a script element")
    expect(contract.common_problems(boot + "\nvar Module = {};\n", "x"), "declares the generated Module")
    expect(contract.common_problems("sessionStorage.getItem('super-mango-replay-level')", "x"),
           "replay boot intent")

    # ── a page starts to carry boot logic again ────────────────────────────
    for token in ("onRuntimeInitialized", "moduleConfig.callMain(args)", "window.Module = {}",
                  "canvas.addEventListener('webglcontextlost', lost)", "setStatus(text) {}",
                  "const args = ['--level', 'levels/00_sandbox_01.toml'];",
                  "document.createElement('script')"):
        for text, check in ((shell, contract.shell_problems), (dashboard, contract.dashboard_problems)):
            expect(check(text + "\n" + token + "\n"), "boot logic belongs in web/boot.js only")

    # ── a page wires the shared script wrongly ─────────────────────────────
    expect(contract.shell_problems(replaced(shell, "/*MANGO_BOOT_JS*/", "")), "marker exactly once")
    expect(contract.shell_problems(replaced(shell, "onFailure: showRestart", "")), "does not pass `onFailure`")
    expect(contract.shell_problems(replaced(shell, "onStatus:", "onProgress:")), "does not pass `onStatus`")
    expect(contract.shell_problems(replaced(shell, "assetQuery: '@MANGO_ASSET_QUERY@'", "assetQuery: ''")),
           "assetQuery must be the @MANGO_ASSET_QUERY@ build marker")
    expect(contract.shell_problems(replaced(shell, 'href="super-mango.wasm@MANGO_ASSET_QUERY@"',
                                            'href="super-mango.wasm"')), "preload of super-mango.wasm")
    expect(contract.shell_problems(replaced(shell, "injectScript: false", "injectScript: true")),
           "injectScript must be false")
    expect(contract.shell_problems(shell + "\nwindow.SuperMangoBoot.start({});\n"), "exactly once, found 2")
    paste = "<script is:inline set:html={bootSource}></script>"
    expect(contract.dashboard_problems(replaced(dashboard, paste, "")), "must paste the boot script once")
    moved = replaced(dashboard, paste, "") + "\n" + paste + "\n"
    expect(contract.dashboard_problems(moved), "must come before the page's own script")
    expect(contract.dashboard_problems(replaced(dashboard, "import bootSource from", "import source from")),
           "must import web/boot.js as text")
    expect(contract.dashboard_problems(replaced(dashboard, "window.SuperMangoBoot.preload(", "preloadFiles(")),
           "SuperMangoBoot.preload()")
    expect(contract.dashboard_problems(replaced(dashboard, "onFailure: showFailure,", "")),
           "does not pass `onFailure`")
    expect(contract.marker_problems(marker + "\nwindow.Module.callMain([]);\n"),
           "must only set window.Module.__superMangoDebug")

    # ── the two copies that drifted apart, rebuilt ─────────────────────────
    # The old standalone page: its own Module object, no failure handling.
    old_shell = replaced(shell, "window.SuperMangoBoot.start({", '''var moduleConfig = {
            canvas: gameCanvas,
            onAbort: function() { showRestart('Game startup failed.'); },
            onRuntimeInitialized: function() { moduleConfig.callMain(moduleConfig.arguments); }
        };
        window.Module = moduleConfig;
        window.SuperMangoBoot.start({''')
    problems = contract.shell_problems(old_shell)
    for token in ("onAbort", "onRuntimeInitialized", "callMain", "window.Module"):
        expect(problems, f"contains `{token}`")
    # The old home page: its own script tag and context-loss handler.
    old_dashboard = replaced(dashboard, "window.SuperMangoBoot.start({", '''const script = document.createElement('script');
        script.onerror = () => showFailure('Failed to load game files.', false);
        canvas.addEventListener('webglcontextlost', () => showFailure('WebGL context lost.', true));
        window.SuperMangoBoot.start({''')
    problems = contract.dashboard_problems(old_dashboard)
    for token in ("createElement('script')", "webglcontextlost"):
        expect(problems, f"contains `{token}`")
    print("web_boot_contract_test: ok")


if __name__ == "__main__":
    main()
