#!/usr/bin/env python3
"""Regression probes for docs gates; run after building docs/out/."""

from pathlib import Path
import sys
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
            ("architecture.md", "| `ladder_render` |", "| `vine_render` |",
             drift.check_render_order_doc, "missing from the render order table"),
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

    def test_code_fences_close_like_commonmark(self):
        # A shorter or different fence line inside a fence is content, so the
        # prose after the real closing fence is still read as prose.
        text = "````md\n```\n`inside`\n````\n`after`\n~~~\n```\n~~~\n`last`\n"
        spans, fenced = drift.split_code(text)
        self.assertEqual([span for _, span in spans], ["after", "last"])
        self.assertEqual([line for _, line in fenced], ["```", "`inside`", "```"])

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
