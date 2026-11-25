#!/usr/bin/env python3
"""Check the per-file coverage baseline checker (tools/check_coverage_baseline.py)."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("check_coverage_baseline",
                                              ROOT / "tools" / "check_coverage_baseline.py")
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module  # dataclasses look their module up there
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
    C = module.Coverage
    assert module.parse_summary(SUMMARY) == {"core/game_profile.c": C(90.0, 85.0),
                                             "levels/level_ref.c": C(100.0, 100.0)}
    assert module.parse_rows(SUMMARY)["TOTAL"] == C(90.70, 86.96)

    # Older llvm-cov has no branch columns; the line value is still the third %.
    assert module.parse_summary("a.c 10 1 90.00% 2 0 100.00% 20 5 75.00%\n") == {"a.c": C(75.0, None)}
    # A file with lines but no branches shows "-" there.
    assert module.parse_summary("b.c 4 0 100.00% 1 0 100.00% 9 0 100.00% 0 0 -\n") == {"b.c": C(100.0, None)}

    with tempfile.TemporaryDirectory() as tmp:
        summary = Path(tmp) / "summary.txt"
        summary.write_text(SUMMARY, encoding="utf-8")
        baseline = Path(tmp) / "baseline.txt"

        # No baseline yet: say so and pass.
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2")
        assert code == 0 and "missing" in text, text

        # --write records every file; a fresh baseline passes.
        assert run(str(summary), "--baseline", str(baseline), "--write")[0] == 0
        assert module.read_baseline(baseline) == module.parse_summary(SUMMARY)
        assert run(str(summary), "--baseline", str(baseline), "--tolerance", "2")[0] == 0

        # 91.5 -> 90.0 is within 2 points; 93.0 -> 90.0 is not. The old
        # one-number format still reads (no branch baseline then).
        baseline.write_text("# comment\ncore/game_profile.c 91.50\nlevels/level_ref.c 100.00\n",
                            encoding="utf-8")
        assert run(str(summary), "--baseline", str(baseline), "--tolerance", "2")[0] == 0
        baseline.write_text("core/game_profile.c 93.00\nlevels/level_ref.c 100.00\n", encoding="utf-8")
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2")
        assert code == 1 and "core/game_profile.c line coverage" in text, text

        # Branches are gated the same way: 85.0 against 88.0 fails.
        baseline.write_text("core/game_profile.c 90.00 88.00\nlevels/level_ref.c 100.00 -\n",
                            encoding="utf-8")
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2")
        assert code == 1 and "core/game_profile.c branch coverage 85.00%" in text, text

        # Without --tolerance the same drop is reported, not failed.
        code, text = run(str(summary), "--baseline", str(baseline))
        assert code == 0 and "report only" in text, text

        # Total floors: lines 90.70 and branches 86.96 against 90 and 87.
        code, text = run(str(summary), "--baseline", str(baseline), "--min-lines", "90")
        assert code == 0 and "meets the 90% floor" in text, text
        code, text = run(str(summary), "--baseline", str(baseline), "--min-branches", "87")
        assert code == 1 and "total branch coverage 86.96% is below the 87% floor" in text, text

        # New files are listed and pass without a floor, but must reach one
        # when it is set. Removed files never fail.
        baseline.write_text("core/game_profile.c 90.00 85.00\nold/removed.c 50.00 40.00\n",
                            encoding="utf-8")
        markdown = Path(tmp) / "summary.md"
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2",
                         "--markdown", str(markdown))
        assert code == 0, text
        assert "levels/level_ref.c is not in the baseline" in text, text
        assert "old/removed.c is in the baseline but not in this report" in text, text
        table = markdown.read_text(encoding="utf-8")
        assert "| `old/removed.c` | gone | 50.00% | gone | 40.00% |" in table, table
        assert "| `levels/level_ref.c` | 100.00% | new | 100.00% | new |" in table, table
        baseline.write_text("levels/level_ref.c 100.00 100.00\n", encoding="utf-8")
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2",
                         "--new-file-min-lines", "80", "--new-file-min-branches", "88")
        assert code == 1 and "core/game_profile.c is new and covers 85.00% of its branches" in text, text
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2",
                         "--new-file-min-lines", "80", "--new-file-min-branches", "80")
        assert code == 0, text

        # --ratchet raises what improved, keeps what fell, adds new files and
        # drops gone ones; it never lowers a number.
        baseline.write_text("core/game_profile.c 95.00 80.00\nold/removed.c 50.00 40.00\n",
                            encoding="utf-8")
        code, text = run(str(summary), "--baseline", str(baseline), "--ratchet")
        assert code == 0 and "ratcheted 3 files" in text, text
        assert module.read_baseline(baseline) == {"core/game_profile.c": C(95.0, 85.0),
                                                  "levels/level_ref.c": C(100.0, 100.0)}
        code, text = run(str(summary), "--baseline", str(baseline), "--ratchet")
        assert "ratcheted 0 files" in text, text

        # An improved file is pointed out, so the gain gets recorded.
        baseline.write_text("core/game_profile.c 89.00 85.00\nlevels/level_ref.c 100.00 100.00\n",
                            encoding="utf-8")
        code, text = run(str(summary), "--baseline", str(baseline), "--tolerance", "2")
        assert code == 0 and "1 files are above their baseline" in text, text

    print("coverage_baseline_test: ok")


if __name__ == "__main__":
    main()
