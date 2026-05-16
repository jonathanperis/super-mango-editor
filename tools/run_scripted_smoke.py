#!/usr/bin/env python3
"""Run deterministic multi-seed, multi-level SDL smoke scenarios."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="out/super-mango", help="game binary to execute")
    parser.add_argument("--editor", default="out/super-mango-editor", help="editor binary to smoke")
    parser.add_argument("--frames", type=int, default=5, help="frames per game scenario")
    parser.add_argument("--seeds", nargs="+", type=int, default=[1, 7, 23], help="deterministic RNG seeds")
    parser.add_argument("--levels", nargs="*", default=None, help="levels to run; defaults to levels/*.toml")
    parser.add_argument("--skip-editor", action="store_true", help="skip editor smoke scenario")
    return parser.parse_args()


def run(cmd: list[str], env: dict[str, str]) -> None:
    printable = " ".join(shlex.quote(part) for part in cmd)
    print(f"scripted-smoke: {printable}")
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)


def main() -> int:
    args = parse_args()
    if args.frames <= 0:
        raise SystemExit("--frames must be positive")

    binary = ROOT / args.binary
    editor = ROOT / args.editor
    if not binary.exists():
        raise SystemExit(f"game binary missing: {binary.relative_to(ROOT)}")
    if not args.skip_editor and not editor.exists():
        raise SystemExit(f"editor binary missing: {editor.relative_to(ROOT)}")

    levels = [Path(item) for item in args.levels] if args.levels else sorted((ROOT / "levels").glob("*.toml"))
    if not levels:
        raise SystemExit("no levels selected for scripted smoke")

    env = os.environ.copy()
    env.setdefault("SDL_VIDEODRIVER", "dummy")
    env.setdefault("SDL_AUDIODRIVER", "dummy")

    for level in levels:
        level_path = level if level.is_absolute() else ROOT / level
        if not level_path.exists():
            raise SystemExit(f"level missing: {level}")
        rel_level = level_path.relative_to(ROOT).as_posix()
        for seed in args.seeds:
            run([
                str(binary),
                "--level",
                rel_level,
                "--smoke-test-frames",
                str(args.frames),
                "--seed",
                str(seed),
            ], env)

    if not args.skip_editor:
        run([str(editor), "--smoke-test"], env)

    print(
        f"scripted-smoke: ok ({len(levels)} levels, {len(args.seeds)} seeds, "
        f"{len(levels) * len(args.seeds)} game scenarios)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
