#!/usr/bin/env python3
"""Compare per-file line coverage against the checked-in baseline.

  python3 tools/check_coverage_baseline.py out/coverage/summary.txt
      [--baseline tools/coverage_baseline.txt] [--tolerance 2]
      [--markdown FILE] [--write]

summary.txt is the `llvm-cov report` table that `make coverage` saves. The
TOTAL floor (COVERAGE_MIN) only notices a large drop across the whole
project; one file can lose most of its tests while the total barely moves.
This script looks at each file instead.

Without --tolerance it only reports. With --tolerance N it exits 1 when any
file's line coverage is more than N points below its baseline. A file that
is new, or no longer in the report, is listed but never fails the check:
refresh the baseline (`make coverage-baseline`) to start tracking it. With
no baseline file at all, it says so and passes. --write replaces the
baseline with the current numbers. --markdown appends the comparison table
to FILE (CI passes $GITHUB_STEP_SUMMARY).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = ROOT / "tools" / "coverage_baseline.txt"

BASELINE_HEADER = """\
# Per-file line coverage (percent) of the native tests, as measured by
# `make coverage CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory` on
# Linux, the configuration of CI's Coverage job. tools/check_coverage_baseline.py
# fails that job when a file drops more than the tolerance below its number.
# Regenerate after adding tests or files:
#   make coverage-baseline CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory
"""


def parse_summary(text: str) -> dict[str, float]:
    """{file: line coverage} from an `llvm-cov report` table.

    Each file row lists region, function and line coverage (then branches,
    on newer llvm-cov) as NN.NN% fields, so the third one is the line value,
    as in the Makefile's TOTAL check. Rows without three percentages (the
    header, the dashed rule, a file with no lines, which shows "-") and the
    TOTAL row are skipped.
    """
    coverage: dict[str, float] = {}
    for line in text.splitlines():
        fields = line.split()
        if not fields or fields[0] == "TOTAL":
            continue
        percents = [field for field in fields[1:] if field.endswith("%")]
        if len(percents) < 3:
            continue
        coverage[fields[0]] = float(percents[2].rstrip("%"))
    return coverage


def read_baseline(path: Path) -> dict[str, float]:
    """{file: percent} from `file percent` lines; # starts a comment."""
    baseline: dict[str, float] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            name, percent = line.split()
            baseline[name] = float(percent)
    return baseline


def baseline_text(coverage: dict[str, float]) -> str:
    rows = "".join(f"{name} {percent:.2f}\n" for name, percent in sorted(coverage.items()))
    return BASELINE_HEADER + rows


def compare(current: dict[str, float], baseline: dict[str, float],
            tolerance: float) -> tuple[list[str], list[str], list[str]]:
    """(files that dropped more than tolerance, new files, files gone)."""
    dropped = [name for name in sorted(baseline)
               if name in current and current[name] < baseline[name] - tolerance]
    new = sorted(set(current) - set(baseline))
    gone = sorted(set(baseline) - set(current))
    return dropped, new, gone


def markdown_table(current: dict[str, float], baseline: dict[str, float],
                   dropped: list[str]) -> str:
    lines = ["| File | Lines | Baseline | Change |", "| --- | ---: | ---: | ---: |"]
    for name in sorted(set(current) | set(baseline)):
        now = f"{current[name]:.2f}%" if name in current else "gone"
        then = f"{baseline[name]:.2f}%" if name in baseline else "new"
        change = ""
        if name in current and name in baseline:
            change = f"{current[name] - baseline[name]:+.2f}"
            if name in dropped:
                change += " (dropped)"
        lines.append(f"| `{name}` | {now} | {then} | {change} |")
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("summary", type=Path, help="llvm-cov report output (summary.txt)")
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--tolerance", type=float, default=None,
                        help="fail when a file drops more than this many points")
    parser.add_argument("--markdown", type=Path, help="append the comparison table to this file")
    parser.add_argument("--write", action="store_true", help="write the current numbers as the baseline")
    args = parser.parse_args(argv)

    current = parse_summary(args.summary.read_text(encoding="utf-8"))
    if not current:
        print(f"coverage baseline: no per-file rows in {args.summary}")
        return 1

    if args.write:
        args.baseline.write_text(baseline_text(current), encoding="utf-8")
        print(f"coverage baseline: wrote {len(current)} files to {args.baseline}")
        return 0

    if not args.baseline.is_file():
        print(f"coverage baseline: {args.baseline} is missing; nothing to compare "
              "(make coverage-baseline creates it)")
        return 0

    baseline = read_baseline(args.baseline)
    tolerance = args.tolerance if args.tolerance is not None else 0.0
    dropped, new, gone = compare(current, baseline, tolerance)

    for name in dropped:
        print(f"coverage baseline: {name} line coverage {current[name]:.2f}% is more than "
              f"{tolerance:g} points below its baseline {baseline[name]:.2f}%")
    for name in new:
        print(f"coverage baseline: {name} is not in the baseline yet ({current[name]:.2f}%)")
    for name in gone:
        print(f"coverage baseline: {name} is in the baseline but not in this report")
    if args.markdown:
        with args.markdown.open("a", encoding="utf-8") as out:
            out.write("### Per-file line coverage against tools/coverage_baseline.txt\n\n")
            out.write(markdown_table(current, baseline, dropped))

    if args.tolerance is None:
        print(f"coverage baseline: report only ({len(dropped)} files below baseline)")
        return 0
    if dropped:
        print("coverage baseline: add tests back, or if the drop is intended, "
              "refresh the baseline with `make coverage-baseline` in the same pull request")
        return 1
    print(f"coverage baseline: {len(current)} files within {tolerance:g} points of the baseline")
    return 0


if __name__ == "__main__":
    sys.exit(main())
