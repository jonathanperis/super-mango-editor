#!/usr/bin/env python3
"""Run deterministic multi-seed, multi-level SDL smoke scenarios."""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="out/super-mango", help="game binary to execute")
    parser.add_argument("--editor", default="out/super-mango-editor", help="editor binary to smoke")
    parser.add_argument("--frames", type=int, default=5, help="frames per game scenario")
    parser.add_argument("--seeds", nargs="+", type=int, default=[1, 7, 23], help="deterministic RNG seeds")
    parser.add_argument("--levels", nargs="*", default=None, help="levels to run; defaults to levels/*.toml")
    parser.add_argument("--replays", nargs="*", default=None, help="replay scripts to run; defaults to built-in movement scripts")
    parser.add_argument("--skip-editor", action="store_true", help="skip editor smoke scenario")
    return parser.parse_args()


def run(cmd: list[str], env: dict[str, str]) -> None:
    printable = " ".join(shlex.quote(part) for part in cmd)
    print(f"scripted-smoke: {printable}")
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)


def default_replay_scripts(workdir: Path) -> list[Path]:
    scripts = {
        "move-right.replay": "0 down right\n0 down shift\n4 up right\n4 up shift\n",
        "jump-right.replay": "0 down right\n1 tap space\n4 up right\n",
        "pause-resume.replay": "0 tap escape\n2 tap enter\n4 tap space\n",
    }
    paths: list[Path] = []
    for name, content in scripts.items():
        path = workdir / name
        path.write_text(content, encoding="utf-8")
        paths.append(path)
    return paths


def selected_replays(args: argparse.Namespace, workdir: Path) -> list[Path]:
    if args.replays:
        paths = [Path(item) for item in args.replays]
        for path in paths:
            full = path if path.is_absolute() else ROOT / path
            if not full.exists():
                raise SystemExit(f"replay script missing: {path}")
        return [path if path.is_absolute() else ROOT / path for path in paths]
    return default_replay_scripts(workdir)


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

    with tempfile.TemporaryDirectory(prefix="super-mango-replay-") as tmp:
        replays = selected_replays(args, Path(tmp))
        for level in levels:
            level_path = level if level.is_absolute() else ROOT / level
            if not level_path.exists():
                raise SystemExit(f"level missing: {level}")
            rel_level = level_path.relative_to(ROOT).as_posix()
            for seed in args.seeds:
                for replay in replays:
                    rel_replay = replay.relative_to(ROOT).as_posix() if replay.is_relative_to(ROOT) else str(replay)
                    run([
                        str(binary),
                        "--level",
                        rel_level,
                        "--smoke-test-frames",
                        str(args.frames),
                        "--seed",
                        str(seed),
                        "--replay-script",
                        rel_replay,
                    ], env)

        scenario_count = len(levels) * len(args.seeds) * len(replays)

    if not args.skip_editor:
        run([str(editor), "--smoke-test"], env)

    print(
        f"scripted-smoke: ok ({len(levels)} levels, {len(args.seeds)} seeds, "
        f"{scenario_count} replay scenarios)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
