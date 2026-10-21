#!/usr/bin/env python3
"""Regression probes for docs gates; run after building docs/out/."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import check_docs_drift as drift
import check_docs_site as site


class DocsChecksTest(unittest.TestCase):
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

    def test_published_site_regressions(self):
        self.assertTrue((site.OUT / "index.html").exists(), "build docs before this test")
        cases = [
            ("docs/index.html", "Super Mango Editor", "Unpublished overview",
             "Markdown page content is not published"),
            ("docs/index.html", 'name="description"', 'name="missing-description"',
             "missing or duplicate page description"),
            ("index.html", "</body>", '<a href="docs/missing/">Missing</a></body>',
             "missing target docs/missing/"),
            ("index.html", "</body>", '<a href="docs/controls/#missing">Missing</a></body>',
             "missing anchor docs/controls/#missing"),
            ("robots.txt", "sitemap-index.xml", "sitemap.xml", "missing sitemap"),
            ("index.html", 'http-equiv="Content-Security-Policy"', 'http-equiv="X-Removed"',
             "expected one Content-Security-Policy meta tag"),
            ("index.html", "const canvas = document.getElementById('canvas');",
             "const canvas = document.getElementById('canvas'); ",
             "CSP does not allow inline script"),
            ("index.html", "script-src 'self'", "script-src 'self' 'unsafe-inline'",
             "script-src must be 'self' plus inline script hashes only"),
        ]
        original_read = Path.read_text
        for name, old, new, expected in cases:
            with self.subTest(name=name):
                target = site.OUT / name
                self.assertIn(old, original_read(target, encoding="utf-8"))

                def mutated(path, *args, **kwargs):
                    text = original_read(path, *args, **kwargs)
                    return text.replace(old, new) if path == target else text

                with patch.object(Path, "read_text", mutated):
                    failures = site.check_site(site.OUT)
                self.assertTrue(any(expected in error for error in failures), failures)


if __name__ == "__main__":
    unittest.main()
