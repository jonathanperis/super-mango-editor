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
