#!/usr/bin/env python3
"""Validate TOML level files before runtime.

Checks stay deliberately boring: parse every repo level, verify referenced files
exist, validate phase links, and enforce the same MAX_* array bounds the C loader
uses. If this fails in CI, a level is wrong. Fix the level, not the validator.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover - CI uses Python 3.11+
    sys.stderr.write("validate_levels: Python 3.11+ required for tomllib\n")
    sys.exit(2)


ROOT = Path(__file__).resolve().parents[1]
LEVEL_DIR = ROOT / "levels"

COUNT_LIMITS = {
    "floor_gaps": "MAX_FLOOR_GAPS",
    "rails": "MAX_RAILS",
    "platforms": "MAX_PLATFORMS",
    "coins": "MAX_COINS",
    "star_yellows": "MAX_STAR_YELLOWS",
    "star_greens": "MAX_STAR_GREENS",
    "star_reds": "MAX_STAR_REDS",
    "spiders": "MAX_SPIDERS",
    "jumping_spiders": "MAX_JUMPING_SPIDERS",
    "birds": "MAX_BIRDS",
    "faster_birds": "MAX_FASTER_BIRDS",
    "fish": "MAX_FISH",
    "faster_fish": "MAX_FASTER_FISH",
    "axe_traps": "MAX_AXE_TRAPS",
    "circular_saws": "MAX_CIRCULAR_SAWS",
    "spike_rows": "MAX_SPIKE_ROWS",
    "spike_platforms": "MAX_SPIKE_PLATFORMS",
    "spike_blocks": "MAX_SPIKE_BLOCKS",
    "blue_flames": "MAX_BLUE_FLAMES",
    "fire_flames": "MAX_BLUE_FLAMES",
    "float_platforms": "MAX_FLOAT_PLATFORMS",
    "bridges": "MAX_BRIDGES",
    "bouncepads_small": "MAX_BOUNCEPADS_SMALL",
    "bouncepads_medium": "MAX_BOUNCEPADS_MEDIUM",
    "bouncepads_high": "MAX_BOUNCEPADS_HIGH",
    "vines": "MAX_VINES",
    "ladders": "MAX_LADDERS",
    "ropes": "MAX_ROPES",
    "background_layers": "MAX_BACKGROUND_LAYERS",
    "foreground_layers": "MAX_BACKGROUND_LAYERS",
    "fog_layers": "MAX_FOG_TEXTURES",
}

PATH_KEYS = {"music_path", "floor_tile_path", "tile_path", "path", "next_phase"}


def load_max_constants() -> dict[str, int]:
    constants: dict[str, int] = {}
    define_re = re.compile(r"^\s*#define\s+(MAX_[A-Z0-9_]+)\s+([0-9]+)\b")

    for header in (ROOT / "src").rglob("*.h"):
        for line in header.read_text(encoding="utf-8").splitlines():
            match = define_re.match(line)
            if match:
                constants[match.group(1)] = int(match.group(2))

    return constants


def load_level(path: Path) -> dict:
    try:
        with path.open("rb") as fp:
            return tomllib.load(fp)
    except tomllib.TOMLDecodeError as exc:
        raise ValueError(f"{path.relative_to(ROOT)}: TOML parse failed: {exc}") from exc


def validate_relative_file(level_path: Path, field: str, value: str) -> list[str]:
    errors: list[str] = []
    if value == "":
        return errors

    candidate = Path(value)
    if candidate.is_absolute() or ".." in candidate.parts:
        errors.append(f"{level_path.relative_to(ROOT)}: {field} must stay inside repo: {value}")
        return errors

    full_path = ROOT / candidate
    if not full_path.is_file():
        errors.append(f"{level_path.relative_to(ROOT)}: {field} missing file: {value}")

    return errors


def validate_paths(level_path: Path, value, field_path: str = "") -> list[str]:
    errors: list[str] = []

    if isinstance(value, dict):
        for key, child in value.items():
            child_path = f"{field_path}.{key}" if field_path else key
            if key in PATH_KEYS and isinstance(child, str):
                errors.extend(validate_relative_file(level_path, child_path, child))
            else:
                errors.extend(validate_paths(level_path, child, child_path))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(validate_paths(level_path, child, f"{field_path}[{index}]"))

    return errors


def validate_counts(level_path: Path, data: dict, constants: dict[str, int]) -> list[str]:
    errors: list[str] = []

    for field, constant in COUNT_LIMITS.items():
        if field not in data:
            continue
        if constant not in constants:
            errors.append(f"{constant} not found while validating {field}")
            continue

        if not isinstance(data[field], list):
            errors.append(
                f"{level_path.relative_to(ROOT)}: {field} must be an array "
                f"(found {type(data[field]).__name__})"
            )
            continue

        count = len(data[field])
        max_count = constants[constant]
        if count > max_count:
            errors.append(
                f"{level_path.relative_to(ROOT)}: {field} has {count} items (max {max_count})"
            )

    return errors


def validate_rail_links(level_path: Path, data: dict) -> list[str]:
    errors: list[str] = []
    rails = data.get("rails", [])
    rail_count = len(rails) if isinstance(rails, list) else 0

    def check_rail_index(field: str, index: int, value) -> None:
        if not isinstance(value, dict):
            return
        rail_index = value.get("rail_index")
        if isinstance(rail_index, bool) or not isinstance(rail_index, int):
            errors.append(
                f"{level_path.relative_to(ROOT)}: {field}[{index}].rail_index "
                "must be an integer"
            )
            return
        if rail_index < 0 or rail_index >= rail_count:
            errors.append(
                f"{level_path.relative_to(ROOT)}: {field}[{index}].rail_index "
                f"{rail_index} out of range (rails: {rail_count})"
            )

    spike_blocks = data.get("spike_blocks", [])
    if isinstance(spike_blocks, list):
        for index, spike_block in enumerate(spike_blocks):
            check_rail_index("spike_blocks", index, spike_block)

    float_platforms = data.get("float_platforms", [])
    if isinstance(float_platforms, list):
        for index, platform in enumerate(float_platforms):
            if isinstance(platform, dict) and platform.get("mode") == "RAIL":
                check_rail_index("float_platforms", index, platform)

    return errors


def validate_level(level_path: Path, constants: dict[str, int]) -> list[str]:
    errors: list[str] = []
    data = load_level(level_path)

    screen_count = data.get("screen_count")
    if isinstance(screen_count, bool) or not isinstance(screen_count, int) or screen_count <= 0:
        errors.append(f"{level_path.relative_to(ROOT)}: screen_count must be a positive integer")

    errors.extend(validate_paths(level_path, data))
    errors.extend(validate_counts(level_path, data, constants))
    errors.extend(validate_rail_links(level_path, data))

    return errors


def main() -> int:
    constants = load_max_constants()
    level_paths = sorted(LEVEL_DIR.glob("*.toml"))

    if not level_paths:
        sys.stderr.write("validate_levels: no levels/*.toml files found\n")
        return 1

    errors: list[str] = []
    for level_path in level_paths:
        try:
            errors.extend(validate_level(level_path, constants))
        except ValueError as exc:
            errors.append(str(exc))

    if errors:
        for error in errors:
            sys.stderr.write(f"validate_levels: {error}\n")
        return 1

    print(f"validate_levels: ok ({len(level_paths)} levels)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
