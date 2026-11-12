#!/usr/bin/env python3
"""Unit tests of the docs gates (tools/check_docs_drift.py, check_docs_site.py).

They need no built site, so `make docs-drift` runs them after the checks
themselves. The probes that read the built site (docs/out/) stay in
tests/docs_checks_test.py, which `bun run check-site` runs after a build.
"""

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import re

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import check_docs_drift as drift
import check_docs_site as site


class DocsDriftUnitTest(unittest.TestCase):
    def test_source_contract_regressions(self):
        cases = [
            ("collectibles-and-surfaces.md", "[[coins]]", "[coins]",
             drift.check_toml_examples, "root.coins has type table"),
            ("level-design.md", "levels/01_lugio_01.toml", "levels/02_lugio_02.toml",
             drift.check_toml_examples, "campaign example differs from the manifest"),
            ("controls.md", "--profile", "--profil",
             drift.check_input_reference, "missing runtime flag `--profile`"),
            ("player-module.md", "unsigned int physical_input_mask,", "",
             drift.check_input_reference, "player_handle_input"),
            ("learning-path.md", "`make timing-lab`", "`make timing-labs`",
             drift.check_make_targets_documented, "`make timing-labs` is not a Makefile target"),
            ("learning-path.md", "`src/core/game_timing.c`", "`src/core/game_timer.c`",
             drift.check_src_paths_exist, "`src/core/game_timer.c` does not exist"),
            ("learning-path.md", "`coins_render()`", "`coin_render()`",
             drift.check_learning_page_functions, "`coin_render()` is not defined"),
            ("constants-reference.md", "| `GRAVITY` | `800.0f` |", "| `GRAVITY` | `900.0f` |",
             drift.check_constant_values, "`GRAVITY` is documented as `900.0f`"),
            ("constants-reference.md", "| `FLOOR_Y` | `252` |", "| `FLOOR_Y` | `250` |",
             drift.check_constant_values, "`FLOOR_Y` is documented as `250`"),
            ("source-files.md", "#define TILE_SIZE     48", "#define TILE_SIZE     32",
             drift.check_constant_values, "`TILE_SIZE` is documented as `32`"),
            ("controls.md", "| F10 |", "| F11 |",
             drift.check_inspector_keys_doc, "inspector key F10 is handled"),
            ("architecture.md", "| `ladders_render` |", "| `vines_render` |",
             drift.check_render_order_doc, "missing from the render order table"),
            ("architecture.md", "| 14 | collision |", "| 14 | collisions |",
             drift.check_update_order_doc, "update row 14 is `collisions`, but row 14 of `s_update_steps`"),
            ("architecture.md", "| 5 | fish | `fish_update` |", "| 5 | fish | `fishes_update` |",
             drift.check_update_order_doc, "names `fishes_update`, which `update_fish()`"),
            ("architecture.md", "| 20 | bouncepads (high) |", "| 21 | bouncepads (high) |",
             drift.check_update_order_doc, "must number its rows 1..N"),
            ("architecture.md", "| 25 | Spiders |", "| 25 | Crabs |",
             drift.check_render_order_doc, "render layer 25 is `Crabs`, but layer 25 of `s_render_layers`"),
            ("architecture.md", "| `spike_blocks_render` |", "| `spike_block_render` |",
             drift.check_render_order_doc, "render layer 22 names `spike_block_render`, which `render_spike_blocks()`"),
            ("c-concepts.md", "`campaign_catalog_load()` in `src/levels/campaign_catalog.c`",
             "`campaign_catalog_load()` in `src/levels/level_session.c`", drift.check_function_locations,
             "`campaign_catalog_load()` is not defined in `src/levels/level_session.c`"),
            ("c-concepts.md", "`undo_group_begin()` and `undo_push()` in `src/editor/undo.c`",
             "`undo_group_begin()` and `editor_push()` in `src/editor/undo.c`", drift.check_function_locations,
             "`editor_push()` is not defined in `src/editor/undo.c`"),
            ("entity-walkthrough.md", "`level_validate_runtime()` (`src/levels/level_validate.c`",
             "`level_validate_runtime()` (`src/levels/level_check.c`", drift.check_function_locations,
             "`src/levels/level_check.c` does not exist"),
            ("index.md", "33 render layers drawn", "32 render layers drawn",
             drift.check_content_counts, "says 32 render layers; the render order table"),
            ("developer-guide.md", "The full 33-layer order", "The full 34-layer order",
             drift.check_content_counts, "says 34 render layers"),
            ("architecture.md", "| 33 | Debug overlay", "| 34 | Debug overlay",
             drift.check_content_counts, "must number its layers 1..N"),
            ("testing.md", "Builds and runs %d native regression binaries" % len(drift.makefile_test_targets()),
             "Builds and runs %d native regression binaries" % (len(drift.makefile_test_targets()) - 1),
             drift.check_test_count_mentions, "docs/wiki/testing.md: stale make test count"),
        ]
        original_read = drift.read
        for name, old, new, check, expected in cases:
            with self.subTest(name=name):
                target = drift.DOCS / name
                self.assertIn(old, original_read(target))

                def mutated(path):
                    text = original_read(path)
                    return text.replace(old, new) if path == target else text

                drift.FAILURES.clear()
                with patch.object(drift, "read", side_effect=mutated):
                    check()
                self.assertTrue(any(expected in error for error in drift.FAILURES), drift.FAILURES)
        drift.FAILURES.clear()

    def test_code_tables_must_match_the_manual(self):
        # The same gates from the other side: the manual stays as it is and a
        # table in the code loses a row, has two rows swapped, or is renamed.
        fish = '    { "fish",                update_fish               },\n'
        faster_fish = '    { "faster fish",         update_faster_fish        },\n'
        player_layer = '    { 30, "player",               render_player            },\n'
        cases = [
            ("src/core/game_update.c", '    { "collision",           update_collision          },\n', "",
             drift.check_update_order_doc, "has 20 rows; `s_update_steps` in src/core/game_update.c has 19"),
            ("src/core/game_update.c", fish + faster_fish, faster_fish + fish,
             drift.check_update_order_doc, "update row 5 is `fish`, but row 5 of `s_update_steps`"),
            ("src/core/game_update.c", "UpdateStep s_update_steps[]", "UpdateStep s_steps[]",
             drift.check_update_order_doc, "no `s_update_steps` table found"),
            ("src/render/game_render.c", player_layer, "",
             drift.check_render_order_doc, "has 33 layers; `s_render_layers` in src/render/game_render.c has 32"),
            ("src/render/game_render.c", player_layer, "",
             drift.check_render_order_doc, "`s_render_layers` must number its layers 1..N in order"),
            ("src/render/game_render.c", '"fish",                 render_fish ', '"sharks",               render_fish ',
             drift.check_render_order_doc, "but layer 19 of `s_render_layers` in src/render/game_render.c is `sharks`"),
            ("src/render/game_render.c", "    fish_render(draw, count,", "    faster_fish_render(draw, count,",
             drift.check_render_order_doc, "render layer 19 names `fish_render`, which `render_fish()`"),
            ("src/render/game_render.c", "    fish_render(draw, count,", "    faster_fish_render(draw, count,",
             drift.check_render_order_doc, "`render_fish()` (layer 19) calls `faster_fish_render`, which is missing"),
            ("src/render/game_render.c", "    draw_overlay(gs);\n",
             "    player_render(&gs->world.player, cam_x);\n    draw_overlay(gs);\n",
             drift.check_render_order_doc, "game_render_frame() calls `player_render` itself"),
            ("src/render/game_render.c", "RenderLayer s_render_layers[]", "RenderLayer s_layers[]",
             drift.check_render_order_doc, "no `s_render_layers` table found"),
        ]
        original_read = drift.read
        for rel, old, new, check, expected in cases:
            with self.subTest(source=rel, expected=expected):
                target = ROOT / rel
                self.assertIn(old, original_read(target))

                def mutated(path):
                    text = original_read(path)
                    return text.replace(old, new) if path == target else text

                drift.FAILURES.clear()
                with patch.object(drift, "read", side_effect=mutated):
                    check()
                self.assertTrue(any(expected in error for error in drift.FAILURES), drift.FAILURES)
        drift.FAILURES.clear()

    def test_stale_stack_claims_fail(self):
        # The maintainer pages and STATE.md quote pins. A rewritten version,
        # a WAV description, or an old regression-binary count must fail drift.
        original_read = drift.read
        raylib = json.loads(original_read(ROOT / "vendor" / "raylib" / "manifest.json"))["version"]
        count = len(drift.makefile_test_targets())
        cases = [
            (ROOT / ".specs" / "project" / "STATE.md", f"raylib {raylib}", "raylib 1.0",
             drift.check_state_stack_claims, "says raylib 1.0"),
            (ROOT / ".specs" / "project" / "STATE.md", "twelve QOA files", "twelve WAVs",
             drift.check_state_stack_claims, "must name the QOA files"),
            # Every page that quotes the raylib pin, in each spelling it uses.
            (ROOT / "README.md", f"| raylib | {raylib},", "| raylib | 1.0,",
             drift.check_state_stack_claims, "says raylib 1.0"),
            (ROOT / "README.md", f"pinned raylib {raylib} source", "pinned raylib 1.0 source",
             drift.check_state_stack_claims, "says raylib 1.0"),
            (ROOT / "THIRD_PARTY_NOTICES.md", f"raylib {raylib}", "raylib 1.0",
             drift.check_state_stack_claims, "says raylib 1.0"),
            (ROOT / ".specs" / "project" / "PROJECT.md", f"raylib {raylib}", "raylib 1.0",
             drift.check_state_stack_claims, "says raylib 1.0"),
            (ROOT / "docs" / "wiki" / "build-system.md", f"raylib **{raylib}**", "raylib **1.0**",
             drift.check_state_stack_claims, "says raylib 1.0"),
            (ROOT / ".specs" / "features" / "level-editor" / "design.md", f"raylib {raylib}", "raylib 1.0",
             drift.check_state_stack_claims, "says raylib 1.0"),
            (ROOT / "docs" / "README.md", "does not run on pushes to `main`",
             "checks every `main` push",
             drift.check_docs_readme_workflow, "does not run on pushes to `main`"),
            # The toolchain pages name the docs site's packages but no versions.
            (ROOT / "docs" / "README.md", "The frontend versions (Astro,", "Astro **7.4** (Astro,",
             drift.check_frontend_versions, "docs/README.md: quotes a version for astro"),
            (ROOT / "docs" / "wiki" / "build-system.md", "and TypeScript) are the",
             "and TypeScript **6.0**) are the",
             drift.check_frontend_versions, "build-system.md: quotes a version for typescript"),
            (ROOT / "docs" / "README.md", "TypeScript", "TS",
             drift.check_frontend_versions, "docs/README.md: the toolchain paragraph must still name typescript"),
            (ROOT / "Makefile", f"{count}-regression-binary", f"{count - 1}-regression-binary",
             drift.check_native_test_counts, f"says {count - 1}-regression-binary"),
            # The feature specs are checked too, in the "N-native-binary" spelling as well.
            (ROOT / ".specs" / "features" / "level-editor" / "tasks.md", f"runs {count} native binaries",
             "runs 15 native binaries", drift.check_native_test_counts, "says 15 native"),
            (ROOT / ".specs" / "project" / "PROJECT.md", f"{count}-native-binary", "15-native-binary",
             drift.check_native_test_counts, "says 15 native"),
            (ROOT / ".specs" / "features" / "level-editor" / "tasks.md", "`src/shared/serializer*.c`",
             "`src/editor/serializer.*`", drift.check_src_paths_exist,
             "`src/editor/serializer.*` matches no file"),
        ]
        for target, old, new, check, expected in cases:
            with self.subTest(expected=expected):
                self.assertIn(old, original_read(target))

                def mutated(path, target=target, old=old, new=new):
                    text = original_read(path)
                    return text.replace(old, new) if path == target else text

                drift.FAILURES.clear()
                with patch.object(drift, "read", side_effect=mutated):
                    check()
                self.assertTrue(any(expected in error for error in drift.FAILURES), drift.FAILURES)
        drift.FAILURES.clear()

    def test_dependency_bumps_need_no_docs_edit(self):
        """What Renovate changes for a docs-site bump or a Node bump passes drift on its own."""
        real_read = drift.read
        lock_path = ROOT / "docs" / "bun.lock"
        lock = real_read(lock_path)
        self.assertIn('"typescript": ["typescript@', lock)

        def bumped_lock(path):
            text = real_read(path)
            if path == lock_path:
                # TypeScript 7 and a minor Astro bump, as bun would record them.
                text = re.sub(r'"typescript": \["typescript@[^"]+"', '"typescript": ["typescript@7.0.1"', text)
                text = re.sub(r'"astro": \["astro@[^"]+"', '"astro": ["astro@7.9.0"', text)
            return text

        with patch.object(drift, "read", bumped_lock), patch.object(drift, "FAILURES", []):
            self.assertEqual(drift.locked_frontend_versions()["typescript"], "7.0.1")
            drift.check_frontend_versions()
            self.assertEqual(drift.FAILURES, [])

        # Tailwind and its Vite plugin on different lines still fail.
        def split_tailwind(path):
            text = real_read(path)
            if path == lock_path:
                text = re.sub(r'"tailwindcss": \["tailwindcss@[^"]+"', '"tailwindcss": ["tailwindcss@9.9.0"', text)
            return text

        with patch.object(drift, "read", split_tailwind), patch.object(drift, "FAILURES", []):
            drift.check_frontend_versions()
            self.assertTrue(any("different major.minor" in item for item in drift.FAILURES), drift.FAILURES)

        # Node: .node-version plus the mentions renovate.json's Node manager
        # rewrites, all moved to one new version, pass together.
        pin = real_read(ROOT / ".node-version").strip()
        manager = next(item for item in json.loads(real_read(ROOT / "renovate.json"))["customManagers"]
                       if item.get("depNameTemplate") == "node")
        pattern = re.compile(manager["matchStrings"][0].replace("(?<currentValue>", "(?P<currentValue>"))
        mentions = {rel: real_read(ROOT / rel) for rel in drift.NODE_MENTION_PAGES}
        for rel, text in mentions.items():
            self.assertTrue(pattern.search(text), f"renovate.json's Node manager matches nothing in {rel}")

        def node_bump(path):
            text = real_read(path)
            if path == ROOT / ".node-version":
                return "99.1.2\n"
            rel = path.relative_to(ROOT).as_posix() if path.is_relative_to(ROOT) else ""
            if rel in mentions:
                text = pattern.sub(lambda m: m.group(0).replace(m.group("currentValue"), "99.1.2"), text)
            return text

        with patch.object(drift, "read", node_bump), patch.object(drift, "FAILURES", []):
            drift.check_readme_prerequisites()
            self.assertEqual(drift.FAILURES, [])
        self.assertTrue(pin)

    def test_script_test_lists_must_match_the_makefile(self):
        """Node/Python tests: files in tests/, Makefile lists and docs agree."""
        real_read = drift.read

        def failures_with(replacements):
            def read(path):
                text = real_read(path)
                for rel, old, new in replacements:
                    if path == ROOT / rel:
                        self.assertIn(old, text, f"test is stale: {old!r} is not in {rel}")
                        text = text.replace(old, new)
                return text
            with patch.object(drift, "read", read), patch.object(drift, "FAILURES", []):
                drift.check_script_test_counts()
                return list(drift.FAILURES)

        own = [item for item in failures_with([])]
        self.assertEqual(own, [], "the repository's own test lists are out of step")
        node, python = drift.makefile_script_tests()
        self.assertIn("tests/boot_test.cjs", node)
        self.assertIn("tests/parser_validator_test.py", python)
        self.assertNotIn("tests/docs_checks_test.py", python, "it runs from bun run check-site")

        cases = [
            # The drift this check was written for: the README's count.
            ([("README.md", "its six JavaScript host-contract tests", "its four JavaScript host-contract tests")],
             f"says four Node tests; the Makefile runs {len(node)}"),
            ([(".specs/project/STATE.md", "six Node tests", "5 Node tests")],
             f"says 5 Node tests; the Makefile runs {len(node)}"),
            # A test file that exists but that no target runs.
            ([("Makefile", "tests/audio_unlock_test.cjs", "")],
             "tests/audio_unlock_test.cjs: no make target runs this test"),
            ([("Makefile", "tests/doctor_test.py", "")],
             "tests/doctor_test.py: no make target runs this test"),
            # A path in the Makefile with no file behind it.
            ([("Makefile", "tests/doctor_test.py", "tests/doctors_test.py")],
             "Makefile: runs tests/doctors_test.py, which does not exist"),
            # A test the docs forgot (STATE.md once listed half of them).
            ([(".specs/project/STATE.md", "`tests/coverage_baseline_test.py`, ", "")],
             "STATE.md: the `- Tests:` line does not list `tests/coverage_baseline_test.py`"),
            ([("docs/wiki/testing.md", "`tests/boot_test.cjs`", "`tests/boot.cjs`")],
             "docs/wiki/testing.md: does not list `tests/boot_test.cjs`"),
            ([("docs/wiki/build-system.md", "`tests/wasm_artifacts_test.py`", "the artifact test")],
             "docs/wiki/build-system.md: does not list `tests/wasm_artifacts_test.py`"),
            # The one test that runs from the docs site's check-site script.
            ([("docs/package.json", "../tests/docs_checks_test.py", "../tests/other.py")],
             "tests/docs_checks_test.py: no make target runs this test"),
        ]
        for replacements, expected in cases:
            with self.subTest(expected=expected):
                found = failures_with(replacements)
                self.assertTrue(any(expected in item for item in found), found)

    def test_ci_node_version_has_one_source(self):
        """README and workflows follow .node-version."""
        real_read = drift.read

        def failures_with(rel, old, new):
            def read(path):
                text = real_read(path)
                if path == ROOT / rel:
                    self.assertIn(old, text, f"test is stale: {old!r} is not in {rel}")
                    text = text.replace(old, new)
                return text
            with patch.object(drift, "read", read), patch.object(drift, "FAILURES", []):
                drift.check_readme_prerequisites()
                return list(drift.FAILURES)

        pin = (ROOT / ".node-version").read_text(encoding="utf-8").strip()
        self.assertEqual(failures_with("README.md", "README", "README"), [])
        cases = [
            ("README.md", f"CI uses Node {pin}", "CI uses Node 1.2.3", f"CI uses Node.js 1.2.3; .node-version says {pin}"),
            ("docs/README.md", f"CI pins Node **{pin}**", "CI pins Node **1.2.3**",
             f"docs/README.md: says CI uses Node.js 1.2.3; .node-version says {pin}"),
            ("docs/wiki/build-system.md", f"CI uses Node **{pin}**", "CI uses Node **1.2.3**",
             f"build-system.md: says CI uses Node.js 1.2.3; .node-version says {pin}"),
            ("docs/wiki/build-system.md", f"CI uses Node **{pin}**", "CI uses a recent Node",
             "build-system.md: must quote the Node.js version CI uses"),
            (".node-version", pin, "latest", ".node-version: must hold the Node.js version CI uses"),
            (".github/workflows/docs.yml", "node-version-file: .node-version", "node-version: '20.0.0'",
             "docs.yml: use `node-version-file: .node-version`"),
            (".github/workflows/build.yml", "node-version-file: .node-version", "check-latest: true",
             "build.yml: every actions/setup-node step must read .node-version"),
        ]
        for rel, old, new, expected in cases:
            with self.subTest(expected=expected):
                found = failures_with(rel, old, new)
                self.assertTrue(any(expected in item for item in found), found)

    def test_function_locations_on_the_current_manual(self):
        # Every "`foo()` in `src/...`" claim in the manual holds today, and the
        # pattern sees the claims (so a regex that matched nothing would fail).
        drift.FAILURES.clear()
        drift.check_function_locations()
        self.assertEqual(drift.FAILURES, [])
        text = drift.read(drift.DOCS / "c-concepts.md")
        claims = [match.group(1) for match in drift.FUNCTION_LOCATION_RE.finditer(text)]
        self.assertIn("`campaign_manifest_load_entries()`", claims)
        self.assertIn("`texture_load()` and `texture_unload()`", claims)

    def test_function_definition_lookup(self):
        source = "static int\nhelper(void)\n{\n    return other(1);\n}\n#define MACRO(x) (x)\n"
        self.assertTrue(drift.file_defines_function(source, "helper"))
        self.assertTrue(drift.file_defines_function(source, "MACRO"))
        # A call inside a body is not a definition.
        self.assertFalse(drift.file_defines_function(source, "other"))
        # Nor is a call on the same file-scope line as another definition.
        one_line = "int wrapper(void) { return target(); }\nint target(void);\n"
        self.assertTrue(drift.file_defines_function(one_line, "wrapper"))
        self.assertTrue(drift.file_defines_function(one_line, "target"))
        self.assertFalse(drift.file_defines_function(one_line.splitlines()[0], "target"))

    def test_code_fences_close_like_commonmark(self):
        # A shorter or different fence line inside a fence is content, so the
        # prose after the real closing fence is still read as prose.
        text = "````md\n```\n`inside`\n````\n`after`\n~~~\n```\n~~~\n`last`\n"
        spans, fenced = drift.split_code(text)
        self.assertEqual([span for _, span in spans], ["after", "last"])
        self.assertEqual([line for _, line in fenced], ["```", "`inside`", "```"])

    def test_google_fonts_urls_fail_in_sources(self):
        # The real sources are clean, without building the site.
        self.assertEqual(site.check_font_sources(ROOT), [])
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            (root / "tools").mkdir()
            (root / "docs" / "node_modules" / "pkg").mkdir(parents=True)
            (root / "docs" / "node_modules" / "pkg" / "a.css").write_text(
                "@import url(https://fonts.googleapis.com/css2);", encoding="utf-8")
            (root / "tools" / "card.html").write_text(
                '<link href="https://fonts.googleapis.com/css2?family=X">\n'
                "<style>@font-face { src: url(//fonts.gstatic.com/x.woff2); }</style>\n",
                encoding="utf-8")
            failures = site.check_font_sources(root)
        self.assertEqual([item.split(": ")[0] for item in failures],
                         ["tools/card.html:1", "tools/card.html:2"], failures)


if __name__ == "__main__":
    unittest.main()
