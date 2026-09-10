#!/usr/bin/env python3
"""Static contract checks for the two hosted WebAssembly shells."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
DASHBOARD = (ROOT / "docs/src/components/home/Dashboard.astro").read_text()
SHELL = (ROOT / "web/shell.html").read_text()


def require(text: str, fragment: str, label: str) -> None:
    if fragment not in text:
        raise SystemExit(f"web boot contract: missing {label}: {fragment}")


def assert_no_generated_module_declaration(text: str, label: str) -> None:
    if re.search(r"\b(?:var|let|const)\s+Module\b", text):
        raise SystemExit(f"web boot contract: {label} declares generated Module")


assert_no_generated_module_declaration(DASHBOARD, "Dashboard")
assert_no_generated_module_declaration(SHELL, "standalone shell")

for text, label in ((DASHBOARD, "Dashboard"), (SHELL, "standalone shell")):
    require(text, "window.Module = moduleConfig", f"{label} scoped Module assignment")
    require(text, "super-mango-replay-level", f"{label} replay storage key")
    require(text, "sessionStorage.removeItem('super-mango-replay-level')",
            f"{label} replay intent consumption")
    require(text, "['--level', replayLevel",
            f"{label} replay startup arguments")
    require(text, "__superMangoMainStarted", f"{label} single-start guard")
    if text.count("moduleConfig.callMain") != 1:
        raise SystemExit(f"web boot contract: {label} must call main once")

require(DASHBOARD, "const replayBootLevel = consumeReplayLevel()",
        "Dashboard automatic replay boot")
require(DASHBOARD, "startGame(false, replayBootLevel)",
        "Dashboard replay auto-start")
require(SHELL, "moduleConfig.arguments = ['--level', 'levels/00_sandbox_01.toml']",
        "standalone default startup arguments")

print("web boot contract: ok")
