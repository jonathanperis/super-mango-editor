#!/usr/bin/env python3
"""Run deterministic multi-seed, multi-level raylib smoke scenarios."""

from __future__ import annotations

import argparse
import json
import math
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

from validate_levels import load_level

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPLAY_DIR = "out/replays-smoke"  # the game reads here without --replay-dir
ALLOWED_REPLAY_IDS = {"move-right", "jump-right", "pause-resume"}


def repo_relative(path: Path) -> str:
    """The path as the game expects it: relative to the repository root.

    The game runs from ROOT, so a path inside the repository is passed
    relative to it (levels/x.toml). A path elsewhere cannot be made
    relative; Path.relative_to() would raise ValueError, so it is passed
    (and reported) as an absolute path instead.
    """
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def default_levels() -> list[Path]:
    """Every campaign level and lab, the same set `make smoke` boots."""
    return sorted((ROOT / "levels").glob("*.toml")) + sorted((ROOT / "levels" / "labs").glob("*.toml"))


def replay_id(path: Path) -> str:
    name = path.stem
    if name not in ALLOWED_REPLAY_IDS:
        raise SystemExit(f"unsupported replay script name: {path} (allowed: {', '.join(sorted(ALLOWED_REPLAY_IDS))})")
    return name


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="out/super-mango", help="game binary to execute")
    parser.add_argument("--editor", default="out/super-mango-editor", help="editor binary to smoke")
    parser.add_argument("--frames", type=int, default=5, help="frames per game scenario")
    parser.add_argument("--seeds", nargs="+", type=int, default=[1, 7, 23], help="deterministic RNG seeds")
    parser.add_argument("--levels", nargs="*", default=None, help="levels to run; defaults to levels/*.toml and levels/labs/*.toml")
    parser.add_argument("--replays", nargs="*", default=None, help="replay scripts to run; defaults to built-in movement scripts")
    parser.add_argument("--skip-editor", action="store_true", help="skip editor smoke scenario")
    parser.add_argument("--replay-dir", default=DEFAULT_REPLAY_DIR,
                        help="folder for generated replay scripts, passed to the game as --replay-dir")
    return parser.parse_args()


def run(cmd: list[str], env: dict[str, str]) -> str:
    printable = " ".join(shlex.quote(part) for part in cmd)
    print(f"scripted-smoke: {printable}")
    result = subprocess.run(cmd, cwd=ROOT, env=env, text=True,
                            capture_output=True, timeout=30)
    if result.returncode:
        print(result.stdout, result.stderr, file=sys.stderr)
        result.check_returncode()
    return result.stdout


def smoke_state(output: str, frames: int) -> dict:
    records = [line.removeprefix("SMOKE_STATE ") for line in output.splitlines()
               if line.startswith("SMOKE_STATE ")]
    if len(records) != 1:
        raise AssertionError("smoke must report exactly one final state")
    state = json.loads(records[0])
    if state["frames"] != frames or not all(math.isfinite(value) for value in state.values()):
        raise AssertionError(f"invalid simulation state: {state}")
    return state


def check_builtin_behavior(state: dict, replay: str, level: Path, frames: int) -> None:
    if frames != 5:
        return  # Longer/custom runs still verify determinism and finite state.
    expected_active_frames = 3 if replay == "pause-resume" else 5
    if not math.isclose(state["elapsed"], expected_active_frames / 60, abs_tol=1e-5):
        raise AssertionError(f"incorrect active simulation time: {state}")
    if state["paused"] or state["complete"]:
        raise AssertionError(f"unexpected overlay after built-in replay: {state}")
    data = load_level(level)
    start_x = data.get("player_start_x", 0)
    if start_x == 0 and data.get("player_start_y", 0) == 0:
        start_x = 80
    if replay == "move-right" and state["x"] <= start_x:
        raise AssertionError("right input did not move the player")
    if replay == "jump-right" and state["vy"] >= 0:
        raise AssertionError("jump input did not launch the player")


def check_replay_failures(binary: Path, level: str, replay: Path, replay_dir: str,
                          env: dict[str, str]) -> None:
    original = replay.read_text(encoding="utf-8")
    cmd = [str(binary), "--level", level, "--smoke-test-frames", "5", "--seed", "1",
           "--replay-dir", replay_dir, "--replay-script"]
    try:
        for contents in (None, "", "-1 down right\n", "999999999999999999999999 down right\n",
                         "0 down mystery\n", "2 down right\n1 up right\n"):
            if contents is None:
                replay.unlink()
            else:
                replay.write_text(contents, encoding="utf-8")
            result = subprocess.run(cmd + [replay_id(replay)], cwd=ROOT, env=env,
                                    capture_output=True, text=True, timeout=30)
            if result.returncode == 0 or "Error:" not in result.stderr:
                raise AssertionError("invalid replay did not fail explicitly")
    finally:
        replay.write_text(original, encoding="utf-8")


def default_replay_scripts(workdir: Path) -> list[Path]:
    scripts = {
        "move-right.replay": "0 down right\n0 down shift\n4 up right\n4 up shift\n",
        "jump-right.replay": "0 down right\n1 tap space\n4 up right\n",
        "pause-resume.replay": "0 tap escape\n2 tap enter\n4 tap space\n",
    }
    paths: list[Path] = []
    workdir.mkdir(parents=True, exist_ok=True)
    for name, content in scripts.items():
        path = workdir / name
        path.write_text(content, encoding="utf-8")
        paths.append(path)
    return paths


def selected_replays(args: argparse.Namespace, workdir: Path) -> list[Path]:
    if args.replays:
        paths = []
        workdir.mkdir(parents=True, exist_ok=True)
        for item in args.replays:
            src = Path(item)
            full = src if src.is_absolute() else ROOT / src
            if not full.exists():
                raise SystemExit(f"replay script missing: {src}")
            dst = workdir / f"{replay_id(full)}.replay"
            shutil.copyfile(full, dst)
            paths.append(dst)
        return paths
    return default_replay_scripts(workdir)


def executable(path: Path) -> Path:
    """Return the built program, allowing for the .exe MinGW linkers append."""
    windows_exe = path.with_name(path.name + ".exe")
    if os.name == "nt" and not path.exists() and windows_exe.exists():
        return windows_exe
    return path


def main() -> int:
    args = parse_args()
    if args.frames <= 0:
        raise SystemExit("--frames must be positive")

    binary = executable(ROOT / args.binary)
    editor = executable(ROOT / args.editor)
    if not binary.exists():
        raise SystemExit(f"game binary missing: {repo_relative(binary)}")
    if not args.skip_editor and not editor.exists():
        raise SystemExit(f"editor binary missing: {repo_relative(editor)}")

    levels = [Path(item) for item in args.levels] if args.levels else default_levels()
    if not levels:
        raise SystemExit("no levels selected for scripted smoke")

    env = os.environ.copy()
    env.setdefault("MANGO_TEST_WINDOW", "1")

    # The game runs from ROOT, so a relative --replay-dir is passed through
    # as given (short, like the default) and only Python resolves it.
    replay_dir = ROOT / args.replay_dir
    replays = selected_replays(args, replay_dir)
    for level in levels:
        level_path = level if level.is_absolute() else ROOT / level
        if not level_path.exists():
            raise SystemExit(f"level missing: {level}")
        rel_level = repo_relative(level_path)
        for seed in args.seeds:
            for replay in replays:
                cmd = [
                    str(binary),
                    "--level",
                    rel_level,
                    "--smoke-test-frames",
                    str(args.frames),
                    "--seed",
                    str(seed),
                    "--replay-dir",
                    args.replay_dir,
                    "--replay-script",
                    replay_id(replay),
                ]
                first = smoke_state(run(cmd, env), args.frames)
                second = smoke_state(run(cmd, env), args.frames)
                if first != second:
                    raise AssertionError(f"non-deterministic replay: {first} != {second}")
                if not args.replays:
                    check_builtin_behavior(first, replay_id(replay), level_path, args.frames)

    scenario_count = len(levels) * len(args.seeds) * len(replays)
    if not args.replays:
        first_level = levels[0] if levels[0].is_absolute() else ROOT / levels[0]
        check_replay_failures(binary, repo_relative(first_level), replays[0],
                              args.replay_dir, env)
        # An explicit profile must still be ignored by smoke/replay. Use an
        # intentionally invalid task-owned file to prove it is neither loaded nor overwritten.
        profile = replay_dir / 'smoke-profile.toml'
        contents = 'not a player profile\n'
        profile.write_text(contents, encoding='utf-8')
        try:
            smoke_state(run([str(binary), '--level', repo_relative(first_level),
                                      '--smoke-test-frames', '5', '--profile', str(profile)], env), 5)
            if profile.read_text(encoding='utf-8') != contents:
                raise AssertionError('smoke modified a profile')
        finally:
            profile.unlink()

    if not args.skip_editor:
        run([str(editor), "--smoke-test"], env)

    print(
        f"scripted-smoke: ok ({len(levels)} levels, {len(args.seeds)} seeds, "
        f"{scenario_count} replay scenarios, each repeated with state assertions)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
