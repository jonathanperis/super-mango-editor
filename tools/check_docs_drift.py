#!/usr/bin/env python3
"""Semantic documentation drift checks for the Super Mango docs.

Astro catches broken Markdown/routes. This script catches project-specific drift
that otherwise shows up only during audits: undocumented test targets, missing
source-map entries, stale TOML snippets, stale constants, and omitted runtime
flags/workflows.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs" / "wiki"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def fail(message: str) -> None:
    FAILURES.append(message)


FAILURES: list[str] = []


def makefile_test_targets() -> list[str]:
    makefile = read(ROOT / "Makefile")
    match = re.search(r"TEST_TARGETS\s*=\s*(.*?)(?=\n[A-Z_]+\s*=)", makefile, re.S)
    if not match:
        fail("Makefile: TEST_TARGETS block not found")
        return []
    return re.findall(r"\$\(OUTDIR\)/([A-Za-z0-9_-]+)", match.group(1))


def check_test_targets_documented() -> None:
    doc = read(DOCS / "build-system.md")
    targets = makefile_test_targets()
    for target in targets:
        if f"`out/{target}`" not in doc:
            fail(f"docs/wiki/build-system.md: missing test target `out/{target}`")
    count_match = re.search(r"Current test binaries \((\d+)\):", doc)
    if not count_match:
        fail("docs/wiki/build-system.md: missing current test binary count")
    elif int(count_match.group(1)) != len(targets):
        fail(
            f"docs/wiki/build-system.md: test count is {count_match.group(1)}, expected {len(targets)}"
        )


def check_source_file_map() -> None:
    doc = read(DOCS / "source-files.md")
    missing: list[str] = []
    for path in sorted((ROOT / "src").rglob("*")):
        if path.suffix not in {".c", ".h"}:
            continue
        rel = path.relative_to(ROOT).as_posix()
        stem = path.stem
        # The docs often group headers and sources as `name.h / .c`; accepting
        # the stem keeps the check semantic without forcing verbose duplicate rows.
        if rel not in doc and path.name not in doc and stem not in doc:
            missing.append(rel)
    for rel in missing:
        fail(f"docs/wiki/source-files.md: missing source map entry for {rel}")


def check_layer_snippets() -> None:
    for page in [DOCS / "assets.md", DOCS / "level-design.md"]:
        text = read(page)
        for line_no, line in enumerate(text.splitlines(), start=1):
            stripped = line.strip()
            if stripped in {"[background_layers]", "[foreground_layers]", "[fog_layers]"}:
                fail(
                    f"{page.relative_to(ROOT)}:{line_no}: use [[...]] array-of-tables for layer snippets, not {stripped}"
                )
    level_design = read(DOCS / "level-design.md")
    for required in ["[[background_layers]]", "[[foreground_layers]]", "[[fog_layers]]"]:
        if required not in level_design:
            fail(f"docs/wiki/level-design.md: missing {required} example")


def check_constants_doc() -> None:
    doc = read(DOCS / "constants-reference.md")
    stale = ["FOG_TEX_COUNT", "PARALLAX_MAX_LAYERS", "GAME_W / WINDOW_W", "GAME_H / WINDOW_H"]
    for token in stale:
        if token in doc:
            fail(f"docs/wiki/constants-reference.md: stale token still present: {token}")
    for token in ["MAX_FOG_TEXTURES", "MAX_BACKGROUND_LAYERS", "WINDOW_W / GAME_W", "WINDOW_H / GAME_H"]:
        if token not in doc:
            fail(f"docs/wiki/constants-reference.md: missing current token: {token}")


def check_gamestate_doc() -> None:
    doc = read(DOCS / "architecture.md")
    for token in ["game_over", "paused", "pause_reasons", "checkpoint_x", "completion", "level_def"]:
        if token not in doc:
            fail(f"docs/wiki/architecture.md: GameState docs missing `{token}`")


def check_level_schema_doc() -> None:
    doc = read(DOCS / "level-design.md")
    for token in ["initial_hearts", "initial_lives", "score_per_life", "coin_score", "fog_layers"]:
        if token not in doc:
            fail(f"docs/wiki/level-design.md: schema docs missing `{token}`")


def check_cli_and_workflows() -> None:
    source_doc = read(DOCS / "source-files.md")
    if "--seed" not in source_doc:
        fail("docs/wiki/source-files.md: CLI flags missing `--seed`")
    build_doc = read(DOCS / "build-system.md")
    for workflow in ["build.yml", "docs.yml", "deploy.yml", "codeql.yml"]:
        if workflow not in build_doc:
            fail(f"docs/wiki/build-system.md: workflow docs missing `{workflow}`")


def check_overlay_controls_doc() -> None:
    render = read(ROOT / "src" / "render" / "render_overlay.c")
    arch = read(DOCS / "architecture.md")
    index = read(DOCS / "index.md")
    collectibles = read(DOCS / "collectibles-and-surfaces.md")
    level_design = read(DOCS / "level-design.md")
    for label in ["Esc/Back: exit", "Enter/Space/Start"]:
        if label not in render:
            fail(f"src/render/render_overlay.c: overlay hint missing `{label}`")
    for page_name, text in [
        ("docs/wiki/architecture.md", arch),
        ("docs/wiki/index.md", index),
        ("docs/wiki/collectibles-and-surfaces.md", collectibles),
        ("docs/wiki/level-design.md", level_design),
    ]:
        if "Back" not in text:
            fail(f"{page_name}: overlay controls missing controller Back exit docs")
        if "Enter/Space/Start" not in text and "Enter, Space, or controller Start" not in text:
            fail(f"{page_name}: overlay controls missing Enter/Space/Start confirmation docs")


def main() -> int:
    check_test_targets_documented()
    check_source_file_map()
    check_layer_snippets()
    check_constants_doc()
    check_gamestate_doc()
    check_level_schema_doc()
    check_cli_and_workflows()
    check_overlay_controls_doc()

    if FAILURES:
        print("docs drift check failed:")
        for item in FAILURES:
            print(f"- {item}")
        return 1
    print("docs drift check: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
