#!/usr/bin/env python3
"""Compare per-file line and branch coverage against the checked-in baseline.

  python3 tools/check_coverage_baseline.py out/coverage/summary.txt
      [--baseline tools/coverage_baseline.txt] [--tolerance 2]
      [--min-lines N] [--min-branches N]
      [--new-file-min-lines N] [--new-file-min-branches N]
      [--markdown FILE] [--write | --ratchet]

summary.txt is the `llvm-cov report` table that `make coverage` saves. It
has, per file, region, function, line and (on newer llvm-cov) branch
coverage. This script gates two of them, lines and branches, at three
levels:

  * the TOTAL row: --min-lines and --min-branches are floors for the whole
    project (the Makefile's COVERAGE_MIN and COVERAGE_BRANCH_MIN);
  * each file in the baseline: with --tolerance N, a file whose line or
    branch coverage is more than N points below its baseline fails. One
    file can lose most of its tests while the total barely moves, so this
    is the check that notices;
  * each file not in the baseline yet (new, or split out of another): it
    must reach --new-file-min-lines / --new-file-min-branches, so a new
    file cannot arrive untested just because nothing recorded it.

A file in the baseline but not in the report (deleted or renamed) is listed
and never fails. A file with no branches shows "-" and has no branch check.

Two ways to update the baseline:

  --write    replace it with the current numbers (after an intended drop,
             such as tests moved elsewhere);
  --ratchet  keep each file's better number: raise the baseline where the
             tests improved, add new files, drop files that are gone, and
             never lower anything. Run it (`make coverage-ratchet`) after
             adding tests, so the next pull request cannot give the gain
             back unnoticed.

Without --tolerance (and no floors) it only reports. With no baseline file
at all, it says so and passes. --markdown appends the comparison table to
FILE (CI passes $GITHUB_STEP_SUMMARY).
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = ROOT / "tools" / "coverage_baseline.txt"

BASELINE_HEADER = """\
# Per-file line and branch coverage (percent) of the native tests, as
# measured by `make coverage CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory`
# on Linux, the configuration of CI's Coverage job. Columns: file, lines,
# branches ("-" when the file has no branches).
# tools/check_coverage_baseline.py fails that job when a file drops more
# than the tolerance below either number. After adding tests, raise the
# numbers that improved (never lowers one):
#   make coverage-ratchet CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory
# After an intended drop, rewrite the whole file:
#   make coverage-baseline CC=clang OUTDIR=out/headless RAYLIB_PLATFORM=memory
"""


@dataclass(frozen=True)
class Coverage:
    """One file's (or the total's) line and branch percentages."""
    lines: float
    branches: float | None  # None: the file has no branches


def _percent(field: str) -> float | None:
    return None if field == "-" else float(field.rstrip("%"))


def parse_rows(text: str) -> dict[str, Coverage]:
    """{name: Coverage} for every row of an `llvm-cov report` table, TOTAL included.

    A row is the name and then count/count/percent triples: regions,
    functions, lines and, on newer llvm-cov, branches. A percent is
    "NN.NN%", or "-" when there is nothing to count. Rows that do not have
    that shape (the header, the dashed rules) are skipped, and so is a file
    with no lines at all.
    """
    rows: dict[str, Coverage] = {}
    for line in text.splitlines():
        fields = line.split()
        values = fields[1:]
        if len(values) not in (9, 12):
            continue
        counts = [v for i, v in enumerate(values) if i % 3 != 2]
        percents = values[2::3]
        if not all(v.isdigit() for v in counts):
            continue
        if not all(p == "-" or p.endswith("%") for p in percents):
            continue
        lines = _percent(percents[2])
        if lines is None:
            continue
        branches = _percent(percents[3]) if len(percents) > 3 else None
        rows[fields[0]] = Coverage(lines, branches)
    return rows


def parse_summary(text: str) -> dict[str, Coverage]:
    """The per-file rows of the report (TOTAL left out)."""
    rows = parse_rows(text)
    rows.pop("TOTAL", None)
    return rows


def read_baseline(path: Path) -> dict[str, Coverage]:
    """{file: Coverage} from `file lines [branches]` lines; # starts a comment.

    The older format had only the line number; such a file has no branch
    baseline until it is rewritten.
    """
    baseline: dict[str, Coverage] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        name, lines = fields[0], float(fields[1])
        branches = _percent(fields[2]) if len(fields) > 2 else None
        baseline[name] = Coverage(lines, branches)
    return baseline


def _fmt(value: float | None) -> str:
    return "-" if value is None else f"{value:.2f}"


def baseline_text(coverage: dict[str, Coverage]) -> str:
    rows = "".join(f"{name} {_fmt(c.lines)} {_fmt(c.branches)}\n"
                   for name, c in sorted(coverage.items()))
    return BASELINE_HEADER + rows


def ratchet(current: dict[str, Coverage], baseline: dict[str, Coverage]) -> dict[str, Coverage]:
    """The baseline with every number that improved raised to today's.

    Files no longer in the report are dropped; new files come in at their
    current numbers. A number never goes down.
    """
    merged: dict[str, Coverage] = {}
    for name, now in current.items():
        then = baseline.get(name)
        if then is None:
            merged[name] = now
            continue
        lines = max(now.lines, then.lines)
        if now.branches is None:
            branches = None  # the file has no branches (any more)
        elif then.branches is None:
            branches = now.branches  # first branch number for this file
        else:
            branches = max(now.branches, then.branches)
        merged[name] = Coverage(lines, branches)
    return merged


@dataclass
class Findings:
    dropped: list[str]          # messages: a tracked file fell below its baseline
    below_floor: list[str]      # messages: a new file is under the new-file floor
    improved: list[str]         # file names: above the baseline (ratchet would raise)
    new: list[str]              # file names not in the baseline
    gone: list[str]             # baseline names not in the report


def compare(current: dict[str, Coverage], baseline: dict[str, Coverage], tolerance: float,
            new_min_lines: float | None = None,
            new_min_branches: float | None = None) -> Findings:
    found = Findings([], [], [], sorted(set(current) - set(baseline)),
                     sorted(set(baseline) - set(current)))
    for name in sorted(set(current) & set(baseline)):
        now, then = current[name], baseline[name]
        if now.lines < then.lines - tolerance:
            found.dropped.append(f"{name} line coverage {now.lines:.2f}% is more than "
                                 f"{tolerance:g} points below its baseline {then.lines:.2f}%")
        if now.branches is not None and then.branches is not None and \
                now.branches < then.branches - tolerance:
            found.dropped.append(f"{name} branch coverage {now.branches:.2f}% is more than "
                                 f"{tolerance:g} points below its baseline {then.branches:.2f}%")
        if now.lines > then.lines or (now.branches is not None and
                                      (then.branches is None or now.branches > then.branches)):
            found.improved.append(name)
    for name in found.new:
        now = current[name]
        if new_min_lines is not None and now.lines < new_min_lines:
            found.below_floor.append(f"{name} is new and covers {now.lines:.2f}% of its lines; "
                                     f"new files need {new_min_lines:g}%")
        if new_min_branches is not None and now.branches is not None and \
                now.branches < new_min_branches:
            found.below_floor.append(f"{name} is new and covers {now.branches:.2f}% of its "
                                     f"branches; new files need {new_min_branches:g}%")
    return found


def check_totals(total: Coverage | None, min_lines: float | None,
                 min_branches: float | None) -> list[str]:
    """Messages for each TOTAL floor that is not met (and the ones that are)."""
    messages: list[str] = []
    if min_lines is None and min_branches is None:
        return messages
    if total is None:
        return ["FAIL no TOTAL row in the report"]
    for label, value, floor in (("line", total.lines, min_lines),
                                ("branch", total.branches, min_branches)):
        if floor is None:
            continue
        if value is None:
            messages.append(f"FAIL no TOTAL {label} coverage in the report")
        elif value < floor:
            messages.append(f"FAIL total {label} coverage {value:.2f}% is below the {floor:g}% floor")
        else:
            messages.append(f"ok total {label} coverage {value:.2f}% meets the {floor:g}% floor")
    return messages


def markdown_table(current: dict[str, Coverage], baseline: dict[str, Coverage]) -> str:
    lines = ["| File | Lines | Baseline | Branches | Baseline |",
             "| --- | ---: | ---: | ---: | ---: |"]

    def cell(c: Coverage | None, attr: str, missing: str) -> str:
        if c is None:
            return missing
        value = getattr(c, attr)
        return "-" if value is None else f"{value:.2f}%"

    for name in sorted(set(current) | set(baseline)):
        now, then = current.get(name), baseline.get(name)
        lines.append(f"| `{name}` | {cell(now, 'lines', 'gone')} | {cell(then, 'lines', 'new')} "
                     f"| {cell(now, 'branches', 'gone')} | {cell(then, 'branches', 'new')} |")
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("summary", type=Path, help="llvm-cov report output (summary.txt)")
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--tolerance", type=float, default=None,
                        help="fail when a file drops more than this many points")
    parser.add_argument("--min-lines", type=float, help="floor for the TOTAL line coverage")
    parser.add_argument("--min-branches", type=float, help="floor for the TOTAL branch coverage")
    parser.add_argument("--new-file-min-lines", type=float,
                        help="line coverage a file not in the baseline must reach")
    parser.add_argument("--new-file-min-branches", type=float,
                        help="branch coverage a file not in the baseline must reach")
    parser.add_argument("--markdown", type=Path, help="append the comparison table to this file")
    update = parser.add_mutually_exclusive_group()
    update.add_argument("--write", action="store_true", help="write the current numbers as the baseline")
    update.add_argument("--ratchet", action="store_true",
                        help="raise the baseline where coverage improved (never lowers it)")
    args = parser.parse_args(argv)

    rows = parse_rows(args.summary.read_text(encoding="utf-8"))
    total = rows.pop("TOTAL", None)
    current = rows
    if not current:
        print(f"coverage baseline: no per-file rows in {args.summary}")
        return 1

    if args.write:
        args.baseline.write_text(baseline_text(current), encoding="utf-8")
        print(f"coverage baseline: wrote {len(current)} files to {args.baseline}")
        return 0

    failed = False
    for message in check_totals(total, args.min_lines, args.min_branches):
        status, text = message.split(" ", 1)
        print(f"coverage: {text}")
        failed |= status == "FAIL"

    if not args.baseline.is_file():
        if args.ratchet:
            args.baseline.write_text(baseline_text(current), encoding="utf-8")
            print(f"coverage baseline: wrote {len(current)} files to {args.baseline}")
            return 0
        print(f"coverage baseline: {args.baseline} is missing; nothing to compare "
              "(make coverage-baseline creates it)")
        return 1 if failed else 0

    baseline = read_baseline(args.baseline)

    if args.ratchet:
        raised = ratchet(current, baseline)
        changed = sorted(name for name in set(raised) | set(baseline)
                         if raised.get(name) != baseline.get(name))
        args.baseline.write_text(baseline_text(raised), encoding="utf-8")
        for name in changed:
            then, now = baseline.get(name), raised.get(name)
            before = "new" if then is None else f"{_fmt(then.lines)} {_fmt(then.branches)}"
            after = "gone" if now is None else f"{_fmt(now.lines)} {_fmt(now.branches)}"
            print(f"coverage baseline: {name}: {before} -> {after}")
        print(f"coverage baseline: ratcheted {len(changed)} files in {args.baseline}")
        return 0

    tolerance = args.tolerance if args.tolerance is not None else 0.0
    found = compare(current, baseline, tolerance,
                    args.new_file_min_lines, args.new_file_min_branches)

    for message in found.dropped + found.below_floor:
        print(f"coverage baseline: {message}")
    for name in found.new:
        if not any(m.startswith(name + " ") for m in found.below_floor):
            print(f"coverage baseline: {name} is not in the baseline yet "
                  f"({current[name].lines:.2f}% lines, {_fmt(current[name].branches)}% branches)")
    for name in found.gone:
        print(f"coverage baseline: {name} is in the baseline but not in this report")
    if found.improved:
        print(f"coverage baseline: {len(found.improved)} files are above their baseline; "
              "`make coverage-ratchet` records the gain")
    if args.markdown:
        with args.markdown.open("a", encoding="utf-8") as out:
            out.write("### Per-file coverage against tools/coverage_baseline.txt\n\n")
            out.write(markdown_table(current, baseline))

    if args.tolerance is None:
        print(f"coverage baseline: report only ({len(found.dropped)} drops below baseline, "
              f"{len(found.below_floor)} new files under the floor)")
        return 1 if failed else 0
    if found.dropped or found.below_floor:
        print("coverage baseline: add tests back (or for the new files), or if a drop is "
              "intended, refresh the baseline with `make coverage-baseline` in the same pull request")
        failed = True
    else:
        print(f"coverage baseline: {len(current)} files within {tolerance:g} points of the baseline")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
