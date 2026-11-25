#!/usr/bin/env python3
"""tools/bench.py reads the game's timing line and judges it against a budget.

No game binary is run: these are the pure parts (parsing, picking the
median run, the budget check), plus a look at the committed budget file.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import bench  # noqa: E402

FAILURES: list[str] = []


def expect(condition: bool, message: str) -> None:
    if not condition:
        FAILURES.append(message)


def row(frame_ms: float, switches: float = 20.0) -> dict[str, float]:
    return {"update_ms": 0.01, "draw_ms": frame_ms - 0.1, "frame_ms": frame_ms,
            "texture_switches": switches}


def test_parse_timing() -> None:
    output = ('SMOKE_STATE {"frames":3}\n'
              'SMOKE_TIMING {"frames":3,"update_ms":0.5,"draw_ms":4.25,'
              '"frame_ms":5.0,"texture_switches":21.00}\n')
    timing = bench.parse_timing(output)
    expect(timing["draw_ms"] == 4.25 and timing["texture_switches"] == 21.0,
           f"SMOKE_TIMING was not read: {timing}")
    try:
        bench.parse_timing("SMOKE_STATE {}\n")
    except ValueError:
        pass
    else:
        FAILURES.append("output without SMOKE_TIMING was accepted")


def test_median_run() -> None:
    runs = [row(9.0), row(30.0), row(10.0)]
    expect(bench.median_run(runs)["frame_ms"] == 10.0, "median run is not the middle frame time")
    expect(bench.median_run([row(7.0), row(8.0)])["frame_ms"] == 7.0,
           "with two runs the faster one is reported")


def test_check() -> None:
    budget = {"tolerance": 2, "texture_switch_tolerance": 1.25,
              "levels": {"a.toml": row(10.0, 20.0)}}
    expect(bench.check({"a.toml": row(19.0, 24.0)}, budget) == [],
           "a level within tolerance was reported")
    problems = bench.check({"a.toml": row(25.0)}, budget)
    expect(any("frame_ms" in problem for problem in problems), f"slow frame not reported: {problems}")
    problems = bench.check({"a.toml": row(10.0, 30.0)}, budget)
    expect(any("texture switches" in problem for problem in problems),
           f"extra texture switches not reported: {problems}")
    problems = bench.check({"b.toml": row(1.0)}, budget)
    expect(any("no budget" in problem for problem in problems), "a level without a budget passed")


def test_budget_file() -> None:
    budget = json.loads(bench.BUDGET.read_text(encoding="utf-8"))
    expect(sorted(budget["levels"]) == sorted(bench.campaign_levels()),
           "tools/bench_budget.json must list exactly the campaign levels")
    for level, limits in budget["levels"].items():
        expect(sorted(limits) == sorted(bench.METRICS), f"{level}: budget needs {bench.METRICS}")
    expect(budget["tolerance"] >= 1 and budget["texture_switch_tolerance"] >= 1,
           "tolerances below 1 would fail a run that meets its budget")


def main() -> int:
    test_parse_timing()
    test_median_run()
    test_check()
    test_budget_file()
    for failure in FAILURES:
        print(f"bench_test: {failure}", file=sys.stderr)
    if FAILURES:
        return 1
    print("bench_test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
