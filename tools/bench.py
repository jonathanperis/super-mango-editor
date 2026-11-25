#!/usr/bin/env python3
"""Time the game's update and drawing per campaign level (`make bench`).

Each level of the campaign manifest runs headless (the raylib Memory
backend, which draws on the CPU) for a fixed number of frames with a fixed
seed and a scripted input that holds Right, so the camera scrolls and the
same frames are drawn on every run. The game prints a SMOKE_TIMING line at
the end of a smoke run (src/core/game_timing.c) with its average CPU time
per frame:

  update_ms         the fixed simulation steps
  draw_ms           drawing, up to handing the frame to the renderer
  frame_ms          the whole frame, EndDrawing included
  texture_switches  draws that used another texture than the one before,
                    about one draw call each (src/shared/graphics.h)

Each level runs --repeat times and the run with the median frame time is
reported, which keeps one disturbed run from moving the result.

--check compares the result with tools/bench_budget.json. Times differ a
lot between machines, so a level fails only when it takes more than the
file's "tolerance" times its budget; texture switches are the same on every
machine and get a tight limit of their own.
"""

from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "levels" / "campaigns" / "main.toml"
BUDGET = ROOT / "tools" / "bench_budget.json"
METRICS = ("update_ms", "draw_ms", "frame_ms", "texture_switches")
# Holding Right from the first frame: the player walks, the camera follows
# and the parallax layers scroll. The game accepts only a few script names
# (src/input/game_replay.c), and this is the "move-right" one.
REPLAY_NAME = "move-right"
REPLAY_SCRIPT = "0 down right\n"


def campaign_levels() -> list[str]:
    with MANIFEST.open("rb") as handle:
        return list(tomllib.load(handle)["levels"])


def parse_timing(output: str) -> dict[str, float]:
    """The SMOKE_TIMING record a smoke run prints when it ends."""
    for line in output.splitlines():
        if line.startswith("SMOKE_TIMING "):
            return json.loads(line.removeprefix("SMOKE_TIMING "))
    raise ValueError("the game printed no SMOKE_TIMING line")


def run_level(binary: Path, level: str, frames: int, replay_dir: Path) -> dict[str, float]:
    cmd = [str(binary), "--level", level, "--smoke-test-frames", str(frames), "--seed", "1",
           "--replay-dir", str(replay_dir), "--replay-script", REPLAY_NAME]
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=600)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(f"bench: {level} exited with {result.returncode}")
    return parse_timing(result.stdout)


def median_run(runs: list[dict[str, float]]) -> dict[str, float]:
    """The run whose frame time is the median one (a real run, not a mix)."""
    ordered = sorted(runs, key=lambda run: run["frame_ms"])
    return ordered[(len(ordered) - 1) // 2]


def check(results: dict[str, dict[str, float]], budget: dict) -> list[str]:
    """Problems with results against the budget file; empty when within it."""
    tolerance = float(budget["tolerance"])
    switch_tolerance = float(budget["texture_switch_tolerance"])
    problems = []
    for level, measured in results.items():
        limits = budget["levels"].get(level)
        if limits is None:
            problems.append(f"{level}: no budget in tools/bench_budget.json")
            continue
        for metric in ("update_ms", "draw_ms", "frame_ms"):
            allowed = limits[metric] * tolerance
            if measured[metric] > allowed:
                problems.append(f"{level}: {metric} {measured[metric]:.3f} ms is over "
                                f"{allowed:.3f} ms ({tolerance:g} x budget {limits[metric]:g})")
        allowed = limits["texture_switches"] * switch_tolerance
        if measured["texture_switches"] > allowed:
            problems.append(f"{level}: {measured['texture_switches']:.1f} texture switches per frame "
                            f"is over {allowed:.1f} ({switch_tolerance:g} x budget {limits['texture_switches']:g})")
    return problems


def table(results: dict[str, dict[str, float]]) -> str:
    lines = [f"{'level':28} {'update ms':>10} {'draw ms':>9} {'frame ms':>9} {'tex switches':>13}"]
    for level, row in results.items():
        lines.append(f"{level:28} {row['update_ms']:10.3f} {row['draw_ms']:9.3f} "
                     f"{row['frame_ms']:9.3f} {row['texture_switches']:13.1f}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--binary", type=Path, default=ROOT / "out/bench/super-mango",
                        help="headless (RAYLIB_PLATFORM=memory) game binary")
    parser.add_argument("--frames", type=int, default=600, help="frames per run (600 = 10 s of play)")
    parser.add_argument("--repeat", type=int, default=3, help="runs per level; the median one is reported")
    parser.add_argument("--levels", nargs="*", help="levels to time (default: the campaign manifest)")
    parser.add_argument("--check", action="store_true", help="fail when a level is over tools/bench_budget.json")
    parser.add_argument("--json", type=Path, help="also write the results to this file")
    args = parser.parse_args(argv)
    if args.frames < 1 or args.repeat < 1:
        parser.error("--frames and --repeat must be at least 1")

    replay_dir = args.binary.resolve().parent / "bench-replays"
    replay_dir.mkdir(parents=True, exist_ok=True)
    (replay_dir / f"{REPLAY_NAME}.replay").write_text(REPLAY_SCRIPT, encoding="utf-8")

    results = {}
    for level in args.levels or campaign_levels():
        runs = [run_level(args.binary, level, args.frames, replay_dir) for _ in range(args.repeat)]
        results[level] = {metric: median_run(runs)[metric] for metric in METRICS}
    print(f"bench: {args.frames} frames per run, median of {args.repeat} runs, per frame:")
    print(table(results))
    if args.json:
        args.json.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")

    if args.check:
        budget = json.loads(BUDGET.read_text(encoding="utf-8"))
        problems = check(results, budget)
        for problem in problems:
            print(f"bench: over budget: {problem}", file=sys.stderr)
        if problems:
            return 1
        print("bench: every level is within tools/bench_budget.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
