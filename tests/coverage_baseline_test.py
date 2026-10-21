#!/usr/bin/env python3
"""Check the per-file coverage baseline checker (tools/check_coverage_baseline.py)."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_coverage_baseline",
                                              ROOT / "tools" / "check_coverage_baseline.py")
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

# The shape `llvm-cov report` prints: regions, functions, lines, branches.
SUMMARY = """\
Filename                      Regions    Missed Regions     Cover   Functions  Missed Functions  Executed       Lines      Missed Lines     Cover    Branches   Missed Branches     Cover
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
core/game_profile.c               400                40    90.00%          20                 1    95.00%         800                80    90.00%         200                30    85.00%
levels/level_ref.c                 50                 0   100.00%           3                 0   100.00%          60                 0   100.00%          30                 0   100.00%
shared/empty.c                      0                 0         -           0                 0         -           0                 0         -           0                 0         -
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TOTAL                             450                40    91.11%          23                 1    95.65%         860                80    90.70%         230                30    86.96%
"""


def run(*argv: str) -> tuple[int, str]:
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = module.main(list(argv))
    return code, out.getvalue()


def main() -> None:
    assert module.parse_summary(SUMMARY) == {"core/game_profile.c": 90.0, "levels/level_ref.c": 100.0}

    # Older llvm-cov has no branch columns; the line value is still the third %.
    assert module.parse_summary("a.c 10 1 90.00% 2 0 100.00% 20 5 75.00%\n") == {"a.c": 75.0}

    with tempfile.TemporaryDirectory() as tmp:
        summary = Path(tmp) / "summary.txt"
        summary.write_text(SUMMARY, encoding="utf-8")
        baseline = Path(tmp) / "baseline.txt"

        # No baseline yet: say so and pass.
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2")
        assert code == 0 and "missing" in text, text

        # --write records every file; a fresh baseline passes.
        assert run(str(summary), "--baseline", str(baseline), "--write")[0] == 0
        assert module.read_baseline(baseline) == {"core/game_profile.c": 90.0, "levels/level_ref.c": 100.0}
        assert run(str(summary), "--baseline", str(baseline), "--tolerance", "2")[0] == 0

        # 91.5 -> 90.0 is within 2 points; 93.0 -> 90.0 is not.
        baseline.write_text("# comment\ncore/game_profile.c 91.50\nlevels/level_ref.c 100.00\n",
                            encoding="utf-8")
        assert run(str(summary), "--baseline", str(baseline), "--tolerance", "2")[0] == 0
        baseline.write_text("core/game_profile.c 93.00\nlevels/level_ref.c 100.00\n", encoding="utf-8")
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2")
        assert code == 1 and "core/game_profile.c" in text, text

        # Without --tolerance the same drop is reported, not failed.
        code, text = run(str(summary), "--baseline", str(baseline))
        assert code == 0 and "report only" in text, text

        # New and removed files are listed, never failed.
        baseline.write_text("core/game_profile.c 90.00\nold/removed.c 50.00\n", encoding="utf-8")
        markdown = Path(tmp) / "summary.md"
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2",
                         "--markdown", str(markdown))
        assert code == 0, text
        assert "levels/level_ref.c is not in the baseline" in text, text
        assert "old/removed.c is in the baseline but not in this report" in text, text
        table = markdown.read_text(encoding="utf-8")
        assert "| `old/removed.c` | gone | 50.00% |" in table, table
        assert "| `levels/level_ref.c` | 100.00% | new |" in table, table

    print("coverage_baseline_test: ok")


if __name__ == "__main__":
    main()
