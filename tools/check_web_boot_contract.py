#!/usr/bin/env python3
"""Static contract checks for the browser game's boot code.

Two pages host the WebAssembly build: the standalone page (web/shell.html)
and the website's home page (docs/src/components/home/Dashboard.astro).
Both paste web/boot.js into an inline script and call
SuperMangoBoot.start() from a second, small script of their own.

Each page once carried its own copy of the boot sequence and the copies
drifted apart: the home page handled a lost WebGL context and a failed
script download, the standalone page did not. These checks keep it one
copy:

  * web/boot.js holds every Emscripten hook and failure handler;
  * neither page mentions any of them (a page that grows boot logic of its
    own fails here, which is how the old drift would have been caught);
  * each page pastes the boot script in before its own, starts it exactly
    once, and tells it how to report status and failures.

super-mango.js is shrunk by the Closure Compiler (`--closure 1` in the
Makefile), which renames every property it does not know is shared with
code it never compiles, such as web/boot.js. web/closure-externs.js lists
the shared names, and the last check here keeps that list complete: each
Module.<name> the game's JavaScript in src/ reads, and each SuperMango*
object the web/ pre-js files put on the window.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOOT = "web/boot.js"
SHELL = "web/shell.html"
DASHBOARD = "docs/src/components/home/Dashboard.astro"
DEBUG_MARKER = "web/debug-boot.js"
EXTERNS = "web/closure-externs.js"
# Module properties Emscripten declares to Closure itself.
EMSCRIPTEN_MODULE_NAMES = {"canvas"}

# What web/boot.js must contain, and why.
BOOT_REQUIREMENTS = (
    ("view.Module = moduleConfig", "hands Emscripten its settings object"),
    ("__superMangoMainStarted", "starts main() only once"),
    ("var DEFAULT_ARGUMENTS = ['--level', 'levels/00_sandbox_01.toml'];", "boots the sandbox level"),
    ("onRuntimeInitialized:", "waits for the runtime before calling main()"),
    ("onAbort:", "reports an aborted start"),
    ("onGameEnded:", "reports the end of a run"),
    ("setStatus:", "passes download progress to the page"),
    ("monitorRunDependencies:", "reports the last download finishing"),
    ("locateFile:", "versions the .wasm and .data addresses"),
    ("canvas.addEventListener('webglcontextlost', handleContextLoss, false)",
     "handles a lost WebGL context"),
    ("script.onerror", "handles a failed download of the script it injects"),
    ("doc.addEventListener('error', handleResourceError, true)",
     "handles a failed download of a script tag the page wrote itself"),
    ("doc.addEventListener('keydown', handleKeyDown, true)", "scopes the keyboard to the canvas"),
    ("SuperMangoTouch.mount(", "mounts the touch buttons"),
    ("__superMangoDebug", "honours the debug page's marker"),
)

# Boot logic. Any of these in a page means the page has started to carry its
# own copy again. The names are Emscripten's Module hooks and the calls only
# the boot script makes.
HOST_FORBIDDEN = (
    "callMain", "onRuntimeInitialized", "onAbort", "onGameEnded", "monitorRunDependencies",
    "locateFile", "setStatus", "window.Module", "__superMangoMainStarted", "__superMangoDebug",
    "webglcontextlost", "SuperMangoTouch", "_game_web_input_touch", "createElement('script')",
    'createElement("script")', "'--level'", '"--level"', "'--debug'", '"--debug"',
)

# What every page must hand to SuperMangoBoot.start().
HOST_OPTIONS = ("canvas", "touchRoot", "assetQuery", "injectScript", "onStatus", "onFailure")

# Text that would end, or confuse, the HTML script element boot.js is pasted in.
FORBIDDEN_IN_BOOT = ("</script", "<script", "<!--")


def boot_problems(boot: str) -> list[str]:
    problems = []
    for fragment, reason in BOOT_REQUIREMENTS:
        if fragment not in boot:
            problems.append(f"{BOOT}: missing `{fragment}` ({reason})")
    if boot.count("moduleConfig.callMain(") != 1:
        problems.append(f"{BOOT}: must call main exactly once (moduleConfig.callMain)")
    lowered = boot.lower()
    for text in FORBIDDEN_IN_BOOT:
        if text in lowered:
            problems.append(f"{BOOT}: must not contain `{text}`; it is pasted inside a script element")
    return problems


def common_problems(text: str, label: str) -> list[str]:
    problems = []
    # super-mango.js declares Module itself; a second declaration in a page
    # would shadow the settings object the boot script prepared.
    if re.search(r"\b(?:var|let|const)\s+Module\b", text):
        problems.append(f"{label}: declares the generated Module")
    # Replay restarts the level inside the running game (app_session.c), so
    # nothing may read a boot intent from origin-shared session storage.
    if "sessionStorage" in text or "super-mango-replay-level" in text:
        problems.append(f"{label}: still reads a replay boot intent")
    return problems


def host_problems(text: str, label: str) -> list[str]:
    """Checks for one page: the text of the page's own (non-boot) code."""
    problems = common_problems(text, label)
    for token in HOST_FORBIDDEN:
        if token in text:
            problems.append(f"{label}: contains `{token}`; boot logic belongs in {BOOT} only")
    starts = len(re.findall(r"SuperMangoBoot\.start\(", text))
    if starts != 1:
        problems.append(f"{label}: must call SuperMangoBoot.start() exactly once, found {starts}")
        return problems
    call = text.split("SuperMangoBoot.start(", 1)[1]
    for option in HOST_OPTIONS:
        if not re.search(rf"^\s*{option}\b", call, re.M):
            problems.append(f"{label}: does not pass `{option}` to SuperMangoBoot.start()")
    return problems


def shell_problems(shell: str) -> list[str]:
    problems = host_problems(shell, SHELL)
    marker = "/*MANGO_BOOT_JS*/"
    if shell.count(marker) != 1:
        problems.append(f"{SHELL}: must contain the {marker} marker exactly once")
    elif "SuperMangoBoot.start(" in shell and shell.index(marker) > shell.index("SuperMangoBoot.start("):
        problems.append(f"{SHELL}: the boot script must be pasted in before it is started")
    if "@MANGO_ASSET_QUERY@" not in shell.split("SuperMangoBoot.start(", 1)[-1]:
        problems.append(f"{SHELL}: assetQuery must be the @MANGO_ASSET_QUERY@ build marker")
    for name in ("super-mango.wasm", "super-mango.data"):
        if f'href="{name}@MANGO_ASSET_QUERY@"' not in shell:
            problems.append(f"{SHELL}: the preload of {name} must carry @MANGO_ASSET_QUERY@")
    if "injectScript: false" not in shell:
        problems.append(f"{SHELL}: emcc writes the script tag, so injectScript must be false")
    return problems


def dashboard_problems(page: str) -> list[str]:
    problems = host_problems(page, DASHBOARD)
    if "import bootSource from '../../../../web/boot.js?raw';" not in page:
        problems.append(f"{DASHBOARD}: must import web/boot.js as text (bootSource)")
    paste = "<script is:inline set:html={bootSource}></script>"
    if page.count(paste) != 1:
        problems.append(f"{DASHBOARD}: must paste the boot script once with `{paste}`")
    elif "<script is:inline>" not in page or page.index(paste) > page.index("<script is:inline>"):
        problems.append(f"{DASHBOARD}: the boot script must come before the page's own script")
    if "injectScript: true" not in page:
        problems.append(f"{DASHBOARD}: the home page adds the script tag on Play, so injectScript must be true")
    if "SuperMangoBoot.preload(" not in page:
        problems.append(f"{DASHBOARD}: must start the .wasm/.data downloads with SuperMangoBoot.preload()")
    return problems


def marker_problems(marker: str) -> list[str]:
    problems = common_problems(marker, DEBUG_MARKER)
    code = [line.strip() for line in marker.splitlines()
            if line.strip() and not line.strip().startswith("//")]
    if code != ["window.Module.__superMangoDebug = true;"]:
        problems.append(f"{DEBUG_MARKER}: must only set window.Module.__superMangoDebug")
    return problems


def shared_names(root: Path = ROOT) -> set[str]:
    """Names Closure must not rename: Module.<name> read by the game's own
    JavaScript (EM_JS bodies in src/), and the SuperMango* objects the
    pre-js files in web/ (everything but the uncompiled boot scripts) put on
    the window."""
    names = set()
    for source in sorted((root / "src").rglob("*.c")):
        names.update(re.findall(r"\bModule\.([A-Za-z_$][\w$]*)", source.read_text(encoding="utf-8")))
    for script in sorted((root / "web").glob("*.js")):
        if script.name in {Path(BOOT).name, Path(DEBUG_MARKER).name, Path(EXTERNS).name}:
            continue
        names.update(re.findall(r"\broot\.(SuperMango\w+)\s*=", script.read_text(encoding="utf-8")))
    return names - EMSCRIPTEN_MODULE_NAMES


def externs_problems(externs: str, names: set[str], makefile: str) -> list[str]:
    problems = []
    for name in sorted(names):
        if not re.search(rf"(?<![\w$]){re.escape(name)}(?![\w$])", externs):
            problems.append(f"{EXTERNS}: does not name `{name}`, which super-mango.js shares with "
                            "code Closure does not compile; Closure would rename it")
    if "--closure 1" in makefile and f"--externs={EXTERNS}" not in makefile:
        problems.append(f"Makefile: links with --closure 1 but does not pass --externs={EXTERNS}")
    return problems


def all_problems(root: Path = ROOT) -> list[str]:
    def text(rel: str) -> str:
        return (root / rel).read_text(encoding="utf-8")

    boot = text(BOOT)
    return (boot_problems(boot) + common_problems(boot, BOOT) + shell_problems(text(SHELL))
            + dashboard_problems(text(DASHBOARD)) + marker_problems(text(DEBUG_MARKER))
            + externs_problems(text(EXTERNS), shared_names(root), text("Makefile")))


def main() -> int:
    problems = all_problems()
    for problem in problems:
        print(f"web boot contract: {problem}", file=sys.stderr)
    if problems:
        return 1
    print("web boot contract: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
