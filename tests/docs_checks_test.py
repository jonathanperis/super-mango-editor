#!/usr/bin/env python3
"""Regression probes for the built docs site; run after building docs/out/.

`bun run check-site` runs this file. The unit tests of the docs checkers,
which need no built site, are in tests/docs_drift_unit_test.py and run from
`make docs-drift`.
"""

from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import check_docs_site as site


class DocsSiteChecksTest(unittest.TestCase):
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
