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

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - CI uses Python 3.11+
    sys.stderr.write("check_docs_drift: Python 3.11+ required for tomllib\n")
    sys.exit(2)

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs" / "wiki"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def load_level(path: Path) -> dict:
    with path.open("rb") as fp:
        return tomllib.load(fp)


def fail(message: str) -> None:
    FAILURES.append(message)


FAILURES: list[str] = []

SCREEN_WORDS = {
    "zero": 0,
    "one": 1,
    "two": 2,
    "three": 3,
    "four": 4,
    "five": 5,
    "six": 6,
    "seven": 7,
    "eight": 8,
    "nine": 9,
    "ten": 10,
    "eleven": 11,
    "twelve": 12,
    "thirteen": 13,
    "fourteen": 14,
    "fifteen": 15,
    "sixteen": 16,
    "seventeen": 17,
    "eighteen": 18,
    "nineteen": 19,
    "twenty": 20,
    "thirty": 30,
    "forty": 40,
    "fifty": 50,
    "sixty": 60,
    "seventy": 70,
    "eighty": 80,
    "ninety": 90,
    "hundred": 100,
}

SCREEN_WORD_PATTERN = "|".join(sorted(SCREEN_WORDS, key=len, reverse=True))
SCREEN_COUNT_RE = re.compile(
    rf"\b((?:\d+|(?:{SCREEN_WORD_PATTERN}))(?:[-\s]+(?:{SCREEN_WORD_PATTERN}))*)\s+screens?\b",
    re.I,
)


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


def check_public_api_docs() -> None:
    source_doc = read(DOCS / "source-files.md")
    player_doc = read(DOCS / "player-module.md")
    source_expectations = {
        "src/game.h": ["int  game_init(GameState *gs);"],
        "src/levels/level_loader.h": ["int level_load(GameState *gs, const LevelDef *def);"],
        "src/player/player.h": ["int player_init(Player *player, SDL_Renderer *renderer);"],
    }
    source_doc_expectations = {
        "int  game_init(GameState *gs);",
        "int level_load(GameState *gs, const LevelDef *def);",
    }
    for rel, signatures in source_expectations.items():
        source = read(ROOT / rel)
        for signature in signatures:
            normalized = " ".join(signature.split())
            if normalized not in " ".join(source.split()):
                fail(f"{rel}: expected public signature `{signature}` in source")
            if signature in source_doc_expectations and normalized not in " ".join(source_doc.split()):
                fail(f"docs/wiki/source-files.md: missing public signature `{signature}`")
    if "int player_init(Player *player, SDL_Renderer *renderer);" not in player_doc:
        fail("docs/wiki/player-module.md: stale or missing `int player_init(...)` signature")
    for stale in ["void game_init(GameState *gs);", "void player_init(Player *player, SDL_Renderer *renderer);", "phase_resolve_path"]:
        for page, text in [("docs/wiki/source-files.md", source_doc), ("docs/wiki/player-module.md", player_doc)]:
            if stale in text:
                fail(f"{page}: stale public API token `{stale}`")


def check_runtime_error_docs() -> None:
    pages = [
        "AGENTS.md",
        "docs/wiki/architecture.md",
        "docs/wiki/developer-guide.md",
        "docs/wiki/assets.md",
    ]
    for rel in pages:
        text = read(ROOT / rel)
        if "exit(EXIT_FAILURE)" in text:
            fail(f"{rel}: stale runtime error handling docs still mention `exit(EXIT_FAILURE)`")
    architecture = read(DOCS / "architecture.md")
    if "return `-1`" not in architecture or "top-level runner returns `EXIT_FAILURE`" not in architecture:
        fail("docs/wiki/architecture.md: missing return-based game_init failure semantics")


def check_collectible_source_guards() -> None:
    parser = read(ROOT / "src" / "editor" / "serializer_load_collectibles.c")
    required_pairs = {
        "star_greens": "MAX_STAR_GREENS",
        "star_reds": "MAX_STAR_REDS",
    }
    for toml_key, max_token in required_pairs.items():
        pattern = rf'LOAD_XY_ARRAY\("{toml_key}",\s*[^,]+,\s*{max_token},'
        if not re.search(pattern, parser, re.S):
            fail(f"src/editor/serializer_load_collectibles.c: `{toml_key}` must use `{max_token}`")


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
    top_scalar_block = doc.split("## Rails", 1)[0]
    if "next_phase" in top_scalar_block:
        fail("docs/wiki/level-design.md: `next_phase` is stale as a top-level scalar; document it under `[last_star]`")
    last_star_section = doc.split("## Last Star", 1)[1].split("---", 1)[0]
    if "next_phase" not in last_star_section or "serialized inside `[last_star]`" not in last_star_section:
        fail("docs/wiki/level-design.md: `[last_star]` section must document nested `next_phase`")


def check_cli_and_workflows() -> None:
    source_doc = read(DOCS / "source-files.md")
    if "--seed" not in source_doc:
        fail("docs/wiki/source-files.md: CLI flags missing `--seed`")
    build_doc = read(DOCS / "build-system.md")
    for workflow in ["build.yml", "docs.yml", "deploy.yml", "codeql.yml"]:
        if workflow not in build_doc:
            fail(f"docs/wiki/build-system.md: workflow docs missing `{workflow}`")
    for token in ["Astro 7", "Vite 8", "Rolldown", "Sätteri Markdown processor", "output: \"static\""]:
        if token not in build_doc:
            fail(f"docs/wiki/build-system.md: Astro 7 docs toolchain missing `{token}`")
    if "src/fetch.ts" not in build_doc or "not" not in build_doc.split("src/fetch.ts", 1)[1][:120].lower():
        fail("docs/wiki/build-system.md: must explain why Astro 7 advanced routing is not configured")


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


def check_level_catalog_doc() -> None:
    catalog = read(DOCS / "level-catalog.md")
    index = read(DOCS / "index.md")
    sidebar = read(ROOT / "docs" / "src" / "lib" / "docsSidebar.ts")
    labels = read(ROOT / "docs" / "src" / "pages" / "docs" / "[...slug].astro")
    level_files = sorted((ROOT / "levels").glob("*.toml"))
    for level in level_files:
        rel = level.relative_to(ROOT).as_posix()
        if rel not in catalog:
            fail(f"docs/wiki/level-catalog.md: missing level entry for `{rel}`")
        data = load_level(level)
        last_star = data.get("last_star")
        if isinstance(last_star, dict):
            next_phase = str(last_star.get("next_phase") or "")
            if next_phase and f"`{next_phase}`" not in catalog:
                fail(f"docs/wiki/level-catalog.md: missing next_phase `{next_phase}` from {rel}")
    if "`last_star`" not in catalog:
        fail("docs/wiki/level-catalog.md: collectibles counts must include `last_star`")
    if "tools/generate_level_catalog.py" not in catalog:
        fail("docs/wiki/level-catalog.md: missing generated-file banner")
    for page_name, text in [
        ("docs/wiki/index.md", index),
        ("docs/src/lib/docsSidebar.ts", sidebar),
        ("docs/src/pages/docs/[...slug].astro", labels),
    ]:
        if "level-catalog" not in text:
            fail(f"{page_name}: missing level-catalog navigation/reference")


def parse_screen_count_token(token: str) -> int | None:
    token = token.strip().lower()
    if token.isdigit():
        return int(token)
    total = 0
    current = 0
    saw_word = False
    for part in re.split(r"[-\s]+", token):
        if not part:
            continue
        if part not in SCREEN_WORDS:
            return None
        saw_word = True
        value = SCREEN_WORDS[part]
        if value == 100:
            current = max(current, 1) * 100
        else:
            current += value
    total += current
    return total if saw_word else None


def screen_mentions(description: str) -> list[int]:
    mentions: list[int] = []
    for match in SCREEN_COUNT_RE.finditer(description):
        parsed = parse_screen_count_token(match.group(1))
        if parsed is not None:
            mentions.append(parsed)
    return mentions


def check_level_prose_counts() -> None:
    for level in sorted((ROOT / "levels").glob("*.toml")):
        data = load_level(level)
        expected = int(data.get("screen_count", 0))
        for mentioned in screen_mentions(str(data.get("description", ""))):
            if mentioned != expected:
                fail(
                    f"{level.relative_to(ROOT)}: description says {mentioned} screens, "
                    f"but screen_count is {expected}"
                )
        generated_by = str(data.get("generated_by", ""))
        if generated_by.lower().startswith("generated by "):
            fail(
                f"{level.relative_to(ROOT)}: `generated_by` should be a credit string, "
                "not prose that repeats the catalog label"
            )


def check_agent_context_docs() -> None:
    expected_count = len(makefile_test_targets())
    for rel in ["AGENTS.md"]:
        text = read(ROOT / rel)
        if f"{expected_count}-test `make test` suite" not in text:
            fail(f"{rel}: stale make test count; expected {expected_count}")
        for token in ["Enter/Space/Start", "Esc/Back", "semantic docs drift"]:
            if token not in text:
                fail(f"{rel}: missing current project context token `{token}`")


def check_public_readme_docs() -> None:
    expected_count = len(makefile_test_targets())
    readme = read(ROOT / "README.md")
    if f"{expected_count} native regression tests" not in readme:
        fail(f"README.md: stale make test count; expected {expected_count}")
    for token in ["Enter/Space/Start", "Esc/Back", "scripted replay smoke on Linux"]:
        if token not in readme:
            fail(f"README.md: missing current project context token `{token}`")
    if "3 coins restore a heart" in readme:
        fail("README.md: stale coin pickup docs; coins award score/bonus-life threshold, stars restore hearts")
    if "GitHub CI is the authoritative WASM release verification" not in readme:
        fail("README.md: missing current authoritative CI WebAssembly verification note")

    docs_readme = read(ROOT / "docs" / "README.md")
    docs_package = read(ROOT / "docs" / "package.json")
    for token in ["bun run lint", "bun run drift", "PUBLIC_GA_ID", "NEXT_PUBLIC_GA_ID"]:
        if token not in docs_readme:
            fail(f"docs/README.md: missing docs command/environment token `{token}`")
    if '"drift": "cd .. && make docs-drift"' not in docs_package:
        fail("docs/package.json: `bun run drift` must delegate to the full `make docs-drift` gate")


def check_wasm_authority_docs() -> None:
    expectations = {
        "docs/wiki/build-system.md": "GitHub Actions WebAssembly build is authoritative",
        "docs/wiki/release-checklist.md": "The authoritative WebAssembly gate is GitHub CI",
        "docs/wiki/index.md": "CI is authoritative for WASM releases",
    }
    for rel, token in expectations.items():
        if token not in read(ROOT / rel):
            fail(f"{rel}: missing current CI-authoritative WebAssembly verification note")


def check_pages_metadata() -> None:
    layout = read(ROOT / "docs" / "src" / "layouts" / "BaseLayout.astro")
    if "const canonicalUrl = new URL(Astro.url.pathname, Astro.site).toString();" not in layout:
        fail("docs/src/layouts/BaseLayout.astro: canonical URL must be derived from the current Astro page path")
    for stale in [
        '<meta property="og:url" content="https://jonathanperis.github.io/super-mango-editor/" />',
        '<link rel="canonical" href="https://jonathanperis.github.io/super-mango-editor/" />',
    ]:
        if stale in layout:
            fail("docs/src/layouts/BaseLayout.astro: stale root-only canonical/og URL remains")


def main() -> int:
    check_test_targets_documented()
    check_source_file_map()
    check_public_api_docs()
    check_runtime_error_docs()
    check_collectible_source_guards()
    check_layer_snippets()
    check_constants_doc()
    check_gamestate_doc()
    check_level_schema_doc()
    check_cli_and_workflows()
    check_overlay_controls_doc()
    check_level_catalog_doc()
    check_level_prose_counts()
    check_agent_context_docs()
    check_public_readme_docs()
    check_wasm_authority_docs()
    check_pages_metadata()

    if FAILURES:
        print("docs drift check failed:")
        for item in FAILURES:
            print(f"- {item}")
        return 1
    print("docs drift check: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
