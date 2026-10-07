#!/usr/bin/env python3
"""Semantic documentation drift checks for the Super Mango docs.

Astro compiles Markdown; check_docs_site.py checks emitted links. This script
catches project-specific drift: undocumented test targets, missing
source-map entries, stale TOML snippets, stale constants, and omitted runtime
flags/workflows. It also cross-checks every manual page against the code:
`make` targets, backticked src/ paths, function names on the learning pages,
documented constant values, the inspector key table and the render order.
"""

from __future__ import annotations

import ast
import re
import shlex
import sys
from pathlib import Path

from validate_levels import campaign_manifest_entries, load_level, load_max_constants, validate_schema

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - CI uses Python 3.11+
    sys.stderr.write("check_docs_drift: Python 3.11+ required for tomllib\n")
    sys.exit(2)

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs" / "wiki"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


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
        "src/player/player.h": ["int player_init(Player *player);"],
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
    if "int player_init(Player *player);" not in player_doc:
        fail("docs/wiki/player-module.md: stale or missing `int player_init(...)` signature")
    for stale in ["void game_init(GameState *gs);", "void player_init(Player *player);", "phase_resolve_path"]:
        for page, text in [("docs/wiki/source-files.md", source_doc), ("docs/wiki/player-module.md", player_doc)]:
            if stale in text:
                fail(f"{page}: stale public API token `{stale}`")


def check_runtime_error_docs() -> None:
    pages = [
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
    parser = read(ROOT / "src" / "shared" / "serializer_load_collectibles.c")
    required_pairs = {
        "star_greens": "MAX_STAR_GREENS",
        "star_reds": "MAX_STAR_REDS",
    }
    for toml_key, max_token in required_pairs.items():
        pattern = rf'LOAD_XY_ARRAY\("{toml_key}",\s*[^,]+,\s*{max_token},'
        if not re.search(pattern, parser, re.S):
            fail(f"src/shared/serializer_load_collectibles.c: `{toml_key}` must use `{max_token}`")


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


def check_toml_examples() -> None:
    constants = load_max_constants()
    for page in sorted(DOCS.glob("*.md")):
        text = read(page)
        for match in re.finditer(r"^```toml\n(.*?)^```", text, re.M | re.S):
            line = text.count("\n", 0, match.start()) + 1
            label = f"{page.relative_to(ROOT)}:{line}"
            try:
                data = tomllib.loads(match.group(1))
            except tomllib.TOMLDecodeError as exc:
                fail(f"{label}: invalid TOML example: {exc}")
                continue
            # Campaign snippets describe the actual ordered manifest.
            if set(data) == {"format_version", "levels"}:
                if data != load_level(ROOT / "levels/campaigns/main.toml"):
                    fail(f"{label}: campaign example differs from the manifest")
                continue
            # Partial level snippets still need strict v1 types and table shape.
            for error in validate_schema({"format_version": 1, **data}, constants=constants):
                fail(f"{label}: {error}")


def check_input_reference() -> None:
    controls = read(DOCS / "controls.md")
    for flag in set(re.findall(r'"(--[a-z-]+)"', read(ROOT / "src/main.c"))):
        if flag not in controls:
            fail(f"docs/wiki/controls.md: missing runtime flag `{flag}`")
    header = read(ROOT / "src/player/player.h")
    doc = read(DOCS / "player-module.md")
    declarations = r"\b(?:int|void|IntRect)\s+player_\w+\([^;{}]*\);"
    documented = {" ".join(item.split()) for item in re.findall(declarations, doc)}
    for declaration in re.findall(declarations, header):
        if " ".join(declaration.split()) not in documented:
            fail(f"docs/wiki/player-module.md: missing/stale declaration `{declaration.split('(')[0]}`")


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
    for token in ["game_over", "paused", "pause_reasons", "respawn_x", "checkpoint_index", "completion", "level_def"]:
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
    for workflow in ["build.yml", "docs.yml", "codeql.yml"]:
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
    for label in ["Esc/B/Back: Exit", "Enter/Space/A/Start: Confirm"]:
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
        if "Enter/Space/Start" not in text and "Enter/Space/A/Start" not in text and "Enter, Space, or controller Start" not in text:
            fail(f"{page_name}: overlay controls missing Enter/Space/Start confirmation docs")


def check_level_catalog_doc() -> None:
    catalog = read(DOCS / "level-catalog.md")
    index = read(DOCS / "index.md")
    sidebar = read(ROOT / "docs" / "src" / "lib" / "docsSidebar.ts")
    labels = read(ROOT / "docs" / "src" / "pages" / "docs" / "[...slug].astro")
    manifest_entries, manifest_errors = campaign_manifest_entries()
    for error in manifest_errors:
        fail(f"campaign manifest: {error}")
    for rel, level, data in manifest_entries:
        if rel not in catalog:
            fail(f"docs/wiki/level-catalog.md: missing level entry for `{rel}`")
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


def check_developer_context_docs() -> None:
    expected_count = len(makefile_test_targets())
    rel = "docs/wiki/developer-guide.md"
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


# ── Cross-checks between the manual and the code ──────────────────────
#
# The checks below read every manual page (plus the README) and compare what
# the prose names with what exists: make targets, src/ paths, function names,
# constant values, inspector keys and the render order. Each failure names the
# page and line so the fix is a single edit.

# Pages a learner reads first. Their backticked `name()` calls must exist.
LEARNING_PAGES = [
    "learning-path.md",
    "mechanics-museum.md",
    "entity-walkthrough.md",
    "debugging-c.md",
    "c-concepts.md",
]

# Names that only exist inside the Token exercise in entity-walkthrough.md.
EXERCISE_NAMES = {"MAX_TOKENS", "TOKEN_SCORE", "tokens_render", "tokens_update", "load_tokens",
                  "draw_token_properties"}

# Backticked src/ paths that deliberately name something absent from the
# repository, with the reason. Ignored local folders are read from .gitignore.
ABSENT_SRC_PATHS = {
    "src/fetch.ts": "build-system.md explains why the docs site has no Astro src/fetch.ts",
}

FENCE_OPEN_RE = re.compile(r"^(`{3,}|~{3,})")
INLINE_CODE_RE = re.compile(r"`([^`\n]+)`")


def doc_pages() -> list[Path]:
    return sorted(DOCS.glob("*.md")) + [ROOT / "README.md"]


def page_label(path: Path, line_no: int) -> str:
    return f"{path.relative_to(ROOT).as_posix()}:{line_no}"


def split_code(text: str) -> tuple[list[tuple[int, str]], list[tuple[int, str]]]:
    """Return (inline code spans outside fences, lines inside fences), each
    with its 1-based line number."""
    spans: list[tuple[int, str]] = []
    fenced: list[tuple[int, str]] = []
    # A fence closes only on a line made of the opener's character, at least
    # as many of them as it opened with, as in CommonMark: a ``` line inside
    # a ```` block, or ~~~ inside ```, is content.
    fence: str | None = None
    for line_no, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if fence is None:
            opener = FENCE_OPEN_RE.match(stripped)
            if opener:
                fence = opener.group(1)
                continue
            spans.extend((line_no, span) for span in INLINE_CODE_RE.findall(line))
        elif re.fullmatch(re.escape(fence[0]) + "{%d,}" % len(fence), stripped):
            fence = None
        else:
            fenced.append((line_no, line))
    return spans, fenced


def makefile_targets() -> set[str]:
    """Every target a user can name: .PHONY entries and plain rule names.
    Rules added later (for example `help`) are accepted automatically."""
    targets: set[str] = set()
    for line in read(ROOT / "Makefile").splitlines():
        phony = re.match(r"^\.PHONY\s*:(.*)$", line)
        if phony:
            targets.update(phony.group(1).split())
            continue
        rule = re.match(r"^([A-Za-z0-9_.\-]+(?:[ \t]+[A-Za-z0-9_.\-]+)*)[ \t]*:(?!=)", line)
        if rule:
            targets.update(rule.group(1).split())
    return targets


def make_targets_in(command: str) -> list[str]:
    """Targets named by one `make ...` command (variables and flags skipped)."""
    command = re.split(r"\s(?:&&|\|\||[|;#])", command, maxsplit=1)[0]
    try:
        words = shlex.split(command)
    except ValueError:
        words = command.split()
    if not words or words[0] != "make":
        return []
    return [word for word in words[1:]
            if "=" not in word and not word.startswith("-") and "/" not in word
            and word not in {"...", "<target>"}]


def check_make_targets_documented() -> None:
    known = makefile_targets()
    for page in doc_pages():
        spans, fenced = split_code(read(page))
        commands = [(n, s.strip()) for n, s in spans if s.strip().startswith("make")]
        commands += [(n, re.sub(r"^\$\s+", "", line.strip())) for n, line in fenced
                     if re.match(r"^(\$\s+)?make(\s|$)", line.strip())]
        for line_no, command in commands:
            for target in make_targets_in(command):
                if target not in known:
                    fail(f"{page_label(page, line_no)}: `make {target}` is not a Makefile target "
                         "(check the spelling or the Makefile's .PHONY list)")


def ignored_src_prefixes() -> list[str]:
    prefixes = []
    for line in read(ROOT / ".gitignore").splitlines():
        line = line.strip()
        if line.startswith("src/"):
            prefixes.append(line.split("*", 1)[0])
    return prefixes


def check_src_paths_exist() -> None:
    ignored = ignored_src_prefixes()
    for page in doc_pages():
        spans, _ = split_code(read(page))
        for line_no, span in spans:
            for raw in re.findall(r"(?<![\w./-])src/[\w./*<>{}-]+", span):
                path = re.sub(r":\d+$", "", raw).rstrip(".")
                if any(ch in path for ch in "<>*{}") or path in ABSENT_SRC_PATHS:
                    continue
                if any(path.startswith(prefix) for prefix in ignored):
                    continue
                if not (ROOT / path).exists() and not (ROOT / "docs" / path).exists():
                    fail(f"{page_label(page, line_no)}: `{path}` does not exist; "
                         "update the path or remove the reference")


def defined_c_names() -> set[str]:
    """Function and macro names declared or defined at file scope in src/,
    tests/ and labs/ (calls are indented, so column-0 lines are enough)."""
    names: set[str] = set()
    for folder in ("src", "tests", "labs"):
        for path in (ROOT / folder).rglob("*.[ch]"):
            for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
                macro = re.match(r"^\s*#\s*define\s+(\w+)", line)
                if macro:
                    names.add(macro.group(1))
                    continue
                if line[:1].isalpha() and not line.startswith(("return", "if", "else")):
                    names.update(re.findall(r"\b([A-Za-z_]\w*)\s*\(", line))
    return names


def check_learning_page_functions() -> None:
    names = defined_c_names() | EXERCISE_NAMES
    for page_name in LEARNING_PAGES:
        page = DOCS / page_name
        spans, _ = split_code(read(page))
        for line_no, span in spans:
            for name in re.findall(r"^([a-z]\w*_\w*)\(\)$", span.strip()):
                if name not in names:
                    fail(f"{page_label(page, line_no)}: `{name}()` is not defined in src/, tests/ "
                         "or labs/; rename it to match the code")


def source_defines() -> dict[str, list[str]]:
    defines: dict[str, list[str]] = {}
    for path in sorted((ROOT / "src").rglob("*.[ch]")):
        rel = path.relative_to(ROOT).as_posix()
        if any(rel.startswith(prefix) for prefix in ignored_src_prefixes()):
            continue
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            match = re.match(r"^\s*#\s*define\s+(\w+)(?!\()\s+(.+?)\s*(?:/\*.*|//.*)?$", line)
            if match:
                defines.setdefault(match.group(1), []).append(match.group(2))
    return defines


def normalize_c_value(value: str) -> str:
    value = re.sub(r"/\*.*?\*/|//.*$", "", value).strip()
    value = re.sub(r"(?<=\d)[fFuUlL]+\b", "", value)
    value = re.sub(r"\s+", "", value)
    while value.startswith("(") and value.endswith(")") and value.count("(") == 1:
        value = value[1:-1]
    return value


def eval_c_value(value: str, defines: dict[str, list[str]], depth: int = 0) -> float | None:
    """Evaluate a numeric #define expression, following other #defines."""
    if depth > 8:
        return None
    expr = normalize_c_value(value)
    expr = re.sub(r"\((?:int|float|double|unsigned)\)", "", expr)

    def substitute(match: re.Match[str]) -> str:
        name = match.group(0)
        values = defines.get(name)
        if not values:
            raise KeyError(name)
        result = eval_c_value(values[0], defines, depth + 1)
        if result is None:
            raise KeyError(name)
        return repr(result)

    try:
        expr = re.sub(r"[A-Za-z_]\w*", substitute, expr)
    except KeyError:
        return None
    try:
        return arithmetic(ast.parse(expr, mode="eval").body)
    except (SyntaxError, ValueError, ZeroDivisionError):
        return None


def arithmetic(node: ast.AST) -> float:
    """Evaluate +, -, *, / and parentheses over numbers; reject anything else."""
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return float(node.value)
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
        value = arithmetic(node.operand)
        return -value if isinstance(node.op, ast.USub) else value
    if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Sub, ast.Mult, ast.Div)):
        left, right = arithmetic(node.left), arithmetic(node.right)
        if isinstance(node.op, ast.Add):
            return left + right
        if isinstance(node.op, ast.Sub):
            return left - right
        if isinstance(node.op, ast.Mult):
            return left * right
        return left / right
    raise ValueError("not plain arithmetic")


def values_match(doc_value: str, source_value: str, defines: dict[str, list[str]]) -> bool:
    if normalize_c_value(doc_value) == normalize_c_value(source_value):
        return True
    doc_number = eval_c_value(doc_value, defines)
    source_number = eval_c_value(source_value, defines)
    return (doc_number is not None and source_number is not None
            and abs(doc_number - source_number) <= 1e-6 * max(1.0, abs(source_number)))


def check_constant_values() -> None:
    defines = source_defines()

    def compare(page: Path, line_no: int, name: str, doc_value: str) -> None:
        if name in EXERCISE_NAMES:
            return
        values = defines.get(name)
        if not values:
            fail(f"{page_label(page, line_no)}: constant `{name}` is not #defined in src/")
        elif not any(values_match(doc_value, value, defines) for value in values):
            fail(f"{page_label(page, line_no)}: `{name}` is documented as `{doc_value}` "
                 f"but src/ defines `{values[0]}`")

    constants = DOCS / "constants-reference.md"
    for line_no, line in enumerate(read(constants).splitlines(), start=1):
        row = re.match(r"^\|\s*`([A-Z][A-Z0-9_]+)`\s*\|\s*`([^`]+)`\s*\|", line)
        if row:
            compare(constants, line_no, row.group(1), row.group(2))
    for page in doc_pages():
        _, fenced = split_code(read(page))
        for line_no, line in fenced:
            define = re.match(r"^\s*#define\s+([A-Z][A-Z0-9_]+)\s+(.+?)\s*(?://.*)?$", line)
            if define and "(" not in define.group(1):
                compare(page, line_no, define.group(1), define.group(2))


def check_inspector_keys_doc() -> None:
    source = read(ROOT / "src" / "core" / "game_inspector.c")
    handled = set(re.findall(r"KEY_(F\d+)", source))
    controls = DOCS / "controls.md"
    text = read(controls)
    if "### Debug inspector keys" not in text:
        fail("docs/wiki/controls.md: missing the `### Debug inspector keys` table "
             "(other pages link to #debug-inspector-keys)")
        return
    section = text.split("### Debug inspector keys", 1)[1].split("\n### ", 1)[0]
    documented = set(re.findall(r"^\|\s*(F\d+)\s*\|", section, re.M))
    for key in sorted(handled - documented, key=lambda k: int(k[1:])):
        fail(f"docs/wiki/controls.md: inspector key {key} is handled in "
             "src/core/game_inspector.c but missing from the Debug inspector keys table")
    for key in sorted(documented - handled, key=lambda k: int(k[1:])):
        fail(f"docs/wiki/controls.md: Debug inspector keys table lists {key}, "
             "which src/core/game_inspector.c does not handle")
    help_keys = re.search(r"static const char \*keys\[\]\s*=\s*\{(.*?)\};", source, re.S)
    if help_keys:
        in_game = set(re.findall(r'"(F\d+)"', help_keys.group(1)))
        for key in sorted(handled - in_game, key=lambda k: int(k[1:])):
            fail(f"src/core/game_inspector.c: key {key} is handled but missing from the F5 help list")


def check_render_order_doc() -> None:
    source = read(ROOT / "src" / "render" / "game_render.c")
    start = source.find("game_render_frame(")
    body = source[start:] if start >= 0 else source
    arch = DOCS / "architecture.md"
    text = read(arch)
    section = text.split("### Render Order (back to front)", 1)
    if len(section) != 2:
        fail("docs/wiki/architecture.md: missing `### Render Order (back to front)` section")
        return
    rows = re.findall(r"^\|\s*(\d+)\s*\|[^|]*\|\s*(.+?)\s*\|\s*$", section[1].split("\n### ", 1)[0], re.M)
    position = 0
    listed: set[str] = set()
    for layer, drawn_by in rows:
        if drawn_by.startswith("inline"):
            continue
        for name in re.findall(r"`(\w+)`", drawn_by):
            listed.add(name)
            found = re.search(rf"\b{name}\s*\(", body[position:])
            if not found:
                fail(f"docs/wiki/architecture.md: render layer {layer} names `{name}`, which "
                     "game_render_frame() does not call at that point; fix the order or the name")
                continue
            position += found.end()
    calls = set(re.findall(r"\b(\w+_render)\s*\(", body))
    for name in sorted(calls - listed - {"game_render_frame", "settings_menu_render"}):
        fail(f"docs/wiki/architecture.md: game_render_frame() calls `{name}`, "
             "which is missing from the render order table")


def main() -> int:
    check_test_targets_documented()
    check_source_file_map()
    check_public_api_docs()
    check_runtime_error_docs()
    check_collectible_source_guards()
    check_layer_snippets()
    check_toml_examples()
    check_input_reference()
    check_constants_doc()
    check_gamestate_doc()
    check_level_schema_doc()
    check_cli_and_workflows()
    check_overlay_controls_doc()
    check_level_catalog_doc()
    check_level_prose_counts()
    check_developer_context_docs()
    check_public_readme_docs()
    check_wasm_authority_docs()
    check_pages_metadata()
    check_make_targets_documented()
    check_src_paths_exist()
    check_learning_page_functions()
    check_constant_values()
    check_inspector_keys_doc()
    check_render_order_doc()

    if FAILURES:
        print("docs drift check failed:")
        for item in FAILURES:
            print(f"- {item}")
        return 1
    print("docs drift check: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
