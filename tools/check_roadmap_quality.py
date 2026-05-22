#!/usr/bin/env python3
"""Audit-roadmap quality checks that complement build/test/docs drift gates."""

from __future__ import annotations

import importlib.util
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FAILURES: list[str] = []


def fail(message: str) -> None:
    FAILURES.append(message)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def load_docs_drift_module():
    module_path = ROOT / "tools" / "check_docs_drift.py"
    spec = importlib.util.spec_from_file_location("check_docs_drift_for_roadmap", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not import {module_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def check_screen_word_parser() -> None:
    module = load_docs_drift_module()
    cases = {
        "Twenty-one screens of lava": [21],
        "Thirty screens and 12 screens": [30, 12],
        "one hundred screens": [100],
    }
    for text, expected in cases.items():
        actual = module.screen_mentions(text)
        if actual != expected:
            fail(f"screen_mentions({text!r}) returned {actual}, expected {expected}")


def check_level_line_endings() -> None:
    attrs = read(ROOT / ".gitattributes")
    if "levels/*.toml text eol=lf" not in attrs:
        fail(".gitattributes: levels/*.toml must enforce LF checkouts")
    for path in sorted((ROOT / "levels").glob("*.toml")):
        data = path.read_bytes()
        if b"\r\n" in data or b"\r" in data:
            fail(f"{path.relative_to(ROOT)}: normalize level TOML line endings to LF")


def overlay_snapshot_tokens() -> list[str]:
    source = read(ROOT / "src" / "render" / "render_overlay.c")
    tokens = re.findall(r'render_centered_text\([^;]*?"([^"]+)"', source, re.S)
    dynamic_titles = re.findall(r'\?\s*"([^"]+)"\s*:\s*"([^"]+)"', source)
    for left, right in dynamic_titles:
        tokens.extend([left, right])
    tokens.extend(re.findall(r'snprintf\([^;]*?"([^"]+)"', source, re.S))
    return sorted(set(tokens))


def check_overlay_snapshot_doc() -> None:
    path = ROOT / "docs" / "wiki" / "overlay-snapshots.md"
    if not path.exists():
        fail("docs/wiki/overlay-snapshots.md: missing overlay text snapshot doc")
        return
    text = read(path)
    for token in overlay_snapshot_tokens():
        if token not in text:
            fail(f"docs/wiki/overlay-snapshots.md: missing overlay source token {token!r}")
    if "tools/generate_overlay_snapshots.py" not in text:
        fail("docs/wiki/overlay-snapshots.md: missing generated-file source note")


def check_release_and_wasm_guardrails() -> None:
    checklist_path = ROOT / "docs" / "wiki" / "release-checklist.md"
    build_workflow = read(ROOT / ".github" / "workflows" / "build.yml")
    deploy_workflow = read(ROOT / ".github" / "workflows" / "deploy.yml")
    docs_workflow = read(ROOT / ".github" / "workflows" / "docs.yml")
    build_doc = read(ROOT / "docs" / "wiki" / "build-system.md")

    if not checklist_path.exists():
        fail("docs/wiki/release-checklist.md: missing release checklist")
    else:
        checklist = read(checklist_path)
        for needle in [
            "make docs-drift",
            "make test",
            "make validate-levels",
            "make scripted-smoke",
            "make web",
            "make dist-wasm",
            "WebAssembly.compile",
            "releases/latest",
        ]:
            if needle not in checklist:
                fail(f"docs/wiki/release-checklist.md: missing `{needle}`")

    if "tools/check_wasm_artifacts.py" not in build_workflow:
        fail(".github/workflows/build.yml: WebAssembly job must run tools/check_wasm_artifacts.py")
    if "tools/check_wasm_artifacts.py" not in docs_workflow:
        fail(".github/workflows/docs.yml: docs path filter must include tools/check_wasm_artifacts.py")
    if "docs/wiki/release-checklist.md" not in docs_workflow:
        fail(".github/workflows/docs.yml: docs path filter must include release checklist")
    for needle in ["docs/out/super-mango.js", "docs/out/super-mango.wasm", "docs/out/super-mango.data"]:
        if needle not in deploy_workflow:
            fail(f".github/workflows/deploy.yml: missing Pages WASM smoke for `{needle}`")
    if "on main push: GitHub Release creation" in build_doc:
        fail("docs/wiki/build-system.md: release trigger docs must not claim main pushes create releases")


def check_scripted_smoke_target() -> None:
    makefile = read(ROOT / "Makefile")
    build_doc = read(ROOT / "docs" / "wiki" / "build-system.md")
    workflow = read(ROOT / ".github" / "workflows" / "build.yml")
    docs_workflow = read(ROOT / ".github" / "workflows" / "docs.yml")
    for needle, label in [
        ("scripted-smoke:", "Makefile target"),
        ("tools/run_scripted_smoke.py", "script path"),
        ("SMOKE_SEEDS", "multi-seed knob"),
    ]:
        if needle not in makefile:
            fail(f"Makefile: missing scripted smoke {label} `{needle}`")
    if "make scripted-smoke" not in build_doc:
        fail("docs/wiki/build-system.md: missing make scripted-smoke docs")
    if "make scripted-smoke" not in workflow:
        fail(".github/workflows/build.yml: CI must run scripted smoke on Linux")
    player_header = read(ROOT / "src" / "player" / "player.h")
    player_input = read(ROOT / "src" / "player" / "player_input.c")
    replay_runner = read(ROOT / "tools" / "run_scripted_smoke.py")
    if "--replay-script" not in replay_runner:
        fail("tools/run_scripted_smoke.py: scripted smoke must pass replay scripts to the game binary")
    if "replay_input_mask" not in player_header or "replay_input_mask" not in player_input:
        fail("player input: replay scripts must feed sampled movement/jump state, not only SDL events")
    if "--replay-script" not in build_doc:
        fail("docs/wiki/build-system.md: missing replay-script smoke docs")
    replay_source = read(ROOT / "src" / "input" / "game_replay.c")
    if "SDL_PushEvent" not in replay_source:
        fail("src/input/game_replay.c: replay smoke must inject SDL events")
    for needle in ["tools/check_roadmap_quality.py", "tools/generate_overlay_snapshots.py", "tools/run_scripted_smoke.py", ".gitattributes"]:
        if needle not in docs_workflow:
            fail(f".github/workflows/docs.yml: docs drift path filter must include `{needle}`")


def main() -> int:
    check_screen_word_parser()
    check_level_line_endings()
    check_overlay_snapshot_doc()
    check_release_and_wasm_guardrails()
    check_scripted_smoke_target()
    if FAILURES:
        print("roadmap quality check failed:")
        for item in FAILURES:
            print(f"- {item}")
        return 1
    print("roadmap quality check: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
