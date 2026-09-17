#!/usr/bin/env python3
"""Validate TOML level files before runtime.

Checks stay deliberately boring: parse every repo level, verify referenced assets
against the repo asset manifest, validate phase links, and enforce the same MAX_*
array bounds the C loader uses. If this fails in CI, a level is wrong. Fix the
level, not the validator.
"""

from __future__ import annotations

import ctypes
import math
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
ASSET_DIR = ROOT / "assets"
CAMPAIGN_MANIFEST = LEVEL_DIR / "campaigns" / "main.toml"
CURRENT_FORMAT_VERSION = 1
CAMPAIGN_MANIFEST_VERSION = 1
CAMPAIGN_MANIFEST_FIELDS = {"format_version", "levels"}

COUNT_LIMITS = {
    "floor_gaps": "MAX_FLOOR_GAPS",
    "checkpoints": "MAX_CHECKPOINTS",
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
    "fire_flames": "MAX_FIRE_FLAMES",
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

ASSET_PATH_RULES = {
    "music_path": (("assets/sounds/",), {".wav"}),
    "floor_tile_path": (("assets/sprites/levels/",), {".png"}),
    "tile_path": (("assets/sprites/levels/",), {".png"}),
    "path": (("assets/sprites/",), {".png"}),
}

ASSET_LITERAL_RE = re.compile(r'"(assets/[^"\n]+\.(?:png|wav|ttf))"')


# These schemas mirror serializer_parse.c.  Fields remain optional because the
# C loader supplies LevelDef defaults for absent values; present v1 values must
# still use the exact compatible TOML type.
XY_FIELDS = {"x": "number", "y": "number"}
SCHEMA_TABLE_FIELDS = {
    "last_star": {**XY_FIELDS, "next_phase": "string"},
    "physics": {
        "walk_max_speed": "number",
        "run_max_speed": "number",
        "walk_ground_accel": "number",
        "run_ground_accel": "number",
        "ground_friction": "number",
        "ground_counter_accel": "number",
        "air_accel_walk": "number",
        "air_accel_run": "number",
        "air_friction": "number",
        "cam_lookahead_vx_factor": "number",
        "cam_lookahead_max": "number",
    },
}
SCHEMA_ARRAY_FIELDS = {
    "rails": {
        "layout": ("enum", ("RECT", "HORIZ")),
        "x": "integer", "y": "integer", "w": "integer", "h": "integer",
        "end_cap": "integer",
    },
    "platforms": {
        "x": "number", "tile_height": "integer", "tile_width": "integer",
        "tile_path": "string",
    },
    "coins": XY_FIELDS,
    "star_yellows": XY_FIELDS,
    "star_greens": XY_FIELDS,
    "star_reds": XY_FIELDS,
    "spiders": {
        "x": "number", "vx": "number", "patrol_x0": "number",
        "patrol_x1": "number", "frame_index": "integer",
    },
    "jumping_spiders": {
        "x": "number", "vx": "number", "patrol_x0": "number",
        "patrol_x1": "number",
    },
    "birds": {
        "x": "number", "base_y": "number", "vx": "number",
        "patrol_x0": "number", "patrol_x1": "number", "frame_index": "integer",
    },
    "faster_birds": {
        "x": "number", "base_y": "number", "vx": "number",
        "patrol_x0": "number", "patrol_x1": "number", "frame_index": "integer",
    },
    "fish": {
        "x": "number", "vx": "number", "patrol_x0": "number",
        "patrol_x1": "number",
    },
    "faster_fish": {
        "x": "number", "vx": "number", "patrol_x0": "number",
        "patrol_x1": "number",
    },
    "axe_traps": {
        "pillar_x": "number", "y": "number",
        "mode": ("enum", ("PENDULUM", "SPIN")),
    },
    "circular_saws": {
        "x": "number", "y": "number", "patrol_x0": "number",
        "patrol_x1": "number", "direction": "integer",
    },
    "spike_rows": {"x": "number", "count": "integer"},
    "spike_platforms": {
        "x": "number", "y": "number", "tile_count": "integer",
    },
    "spike_blocks": {
        "rail_index": "integer", "t_offset": "number", "speed": "number",
    },
    "blue_flames": {"x": "number"},
    "fire_flames": {"x": "number"},
    "float_platforms": {
        "mode": ("enum", ("STATIC", "CRUMBLE", "RAIL")),
        "x": "number", "y": "number", "tile_count": "integer",
        "rail_index": "integer", "t_offset": "number", "speed": "number",
    },
    "bridges": {
        "x": "number", "y": "number", "brick_count": "integer",
    },
    "bouncepads_small": {
        "x": "number", "launch_vy": "number",
        "pad_type": ("enum", ("GREEN", "WOOD", "RED")),
    },
    "bouncepads_medium": {
        "x": "number", "launch_vy": "number",
        "pad_type": ("enum", ("GREEN", "WOOD", "RED")),
    },
    "bouncepads_high": {
        "x": "number", "launch_vy": "number",
        "pad_type": ("enum", ("GREEN", "WOOD", "RED")),
    },
    "vines": {
        "x": "number", "y": "number", "tile_count": "integer",
        "vine_type": "integer",
    },
    "ladders": {"x": "number", "y": "number", "tile_count": "integer"},
    "ropes": {"x": "number", "y": "number", "tile_count": "integer"},
    "checkpoints": XY_FIELDS,
    "background_layers": {"path": "string", "speed": "number"},
    "foreground_layers": {"path": "string", "speed": "number"},
    "fog_layers": {"path": "string", "speed": "number"},
}
REQUIRED_ARRAY_FIELDS = {
    "checkpoints": {"x", "y"},
}
SCHEMA_SCALAR_FIELDS = {
    "format_version": "integer",
    "name": "string",
    "description": "string",
    "generated_by": "string",
    "screen_count": "integer",
    "player_start_x": "number",
    "player_start_y": "number",
    "music_path": "string",
    "music_volume": "integer",
    "floor_tile_path": "string",
    "initial_hearts": "integer",
    "initial_lives": "integer",
    "score_per_life": "integer",
    "coin_score": "integer",
}


def _schema_type(value) -> str:
    if isinstance(value, bool):
        return "boolean"
    if isinstance(value, int):
        return "integer"
    if isinstance(value, float):
        return "float"
    if isinstance(value, str):
        return "string"
    if isinstance(value, list):
        return "array"
    if isinstance(value, dict):
        return "table"
    return type(value).__name__


def _embedded_nul_errors(value, path: str = "root") -> list[str]:
    errors: list[str] = []

    if isinstance(value, dict):
        for key, child in value.items():
            if isinstance(key, str) and "\x00" in key:
                errors.append(f"{path}: key {key!r} contains an embedded NUL")
                child_path = f"{path}[key]"
            else:
                child_path = f"{path}.{key}"
            errors.extend(_embedded_nul_errors(child, child_path))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(_embedded_nul_errors(child, f"{path}[{index}]"))
    elif isinstance(value, str) and "\x00" in value:
        errors.append(f"{path}: string value contains an embedded NUL")

    return errors


def _is_finite_number(value) -> bool:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        numeric = float(value)
        return math.isfinite(numeric) and math.isfinite(ctypes.c_float(numeric).value)
    except (OverflowError, ValueError):
        return False


def _validate_schema_value(value, expected, path: str) -> list[str]:
    if expected == "integer":
        if isinstance(value, bool) or not isinstance(value, int):
            return [f"{path} has type {_schema_type(value)}, expected integer"]
        if value < -(2**31) or value > (2**31 - 1):
            return [f"{path} integer is outside C int range"]
        return []
    if expected == "number":
        if not _is_finite_number(value):
            return [f"{path} must be a finite number"]
        return []
    if expected == "string":
        if not isinstance(value, str):
            return [f"{path} has type {_schema_type(value)}, expected string"]
        key = path.rsplit(".", 1)[-1]
        capacity = {"description": 4096, "generated_by": 128, "next_phase": 256}.get(key, 64)
        if len(value.encode("utf-8")) >= capacity:
            return [f"{path} exceeds its {capacity - 1}-byte storage"]
        return []

    enum_values = expected[1]
    if not isinstance(value, str):
        return [f"{path} has type {_schema_type(value)}, expected enum string"]
    if value not in enum_values:
        return [f"{path} has unknown enum value {value!r}"]
    return []


def validate_schema(
    data: dict,
    *,
    require_explicit_version: bool = False,
    constants: dict[str, int] | None = None,
) -> list[str]:
    """Validate explicit v1 TOML shape before LevelDef normalization.

    Missing format_version is accepted only when callers request legacy
    compatibility. Checked-in level validation passes require_explicit_version.
    """
    errors: list[str] = []
    if not isinstance(data, dict):
        return [f"root has type {_schema_type(data)}, expected table"]

    nul_errors = _embedded_nul_errors(data)
    if nul_errors:
        return nul_errors

    if "format_version" not in data:
        if require_explicit_version:
            errors.append("format_version must be explicit integer 1")
        # Legacy readers accept fractional integers, but never unsafe casts.
        def check_legacy(value, expected, path):
            if expected == "integer" and isinstance(value, (int, float)) and not isinstance(value, bool):
                if not math.isfinite(value) or not -(2**31) <= math.trunc(value) <= 2**31 - 1:
                    errors.append(f"{path} is outside C int range")
            elif expected == "number" and isinstance(value, (int, float)):
                if not _is_finite_number(value):
                    errors.append(f"{path} must be finite")
            elif expected == "string" and isinstance(value, str):
                errors.extend(_validate_schema_value(value, expected, path))
        for key, value in data.items():
            if key in SCHEMA_SCALAR_FIELDS:
                check_legacy(value, SCHEMA_SCALAR_FIELDS[key], key)
            elif key in SCHEMA_TABLE_FIELDS or key in SCHEMA_ARRAY_FIELDS:
                fields = SCHEMA_TABLE_FIELDS[key] if key in SCHEMA_TABLE_FIELDS else SCHEMA_ARRAY_FIELDS[key]
                items = [value] if key in SCHEMA_TABLE_FIELDS else value if isinstance(value, list) else []
                for item in items:
                    if isinstance(item, dict):
                        for child, supplied in item.items():
                            if child in fields:
                                check_legacy(supplied, fields[child], f"{key}.{child}")
            elif key == "floor_gaps" and isinstance(value, list):
                for item in value:
                    check_legacy(item, "integer", key)
        return errors

    version_errors = _validate_schema_value(data["format_version"], "integer", "root.format_version")
    if version_errors:
        return version_errors
    if data["format_version"] != CURRENT_FORMAT_VERSION:
        return [
            f"root.format_version is {data['format_version']} (expected "
            f"{CURRENT_FORMAT_VERSION})"
        ]

    known_root = set(SCHEMA_SCALAR_FIELDS) | set(SCHEMA_TABLE_FIELDS) | set(SCHEMA_ARRAY_FIELDS)
    known_root.add("floor_gaps")
    for key in data:
        if key not in known_root:
            errors.append(f"root has unknown key {key!r}")
            continue
        path = f"root.{key}"
        value = data[key]
        if key in SCHEMA_SCALAR_FIELDS:
            errors.extend(_validate_schema_value(value, SCHEMA_SCALAR_FIELDS[key], path))
        elif key in SCHEMA_TABLE_FIELDS:
            if not isinstance(value, dict):
                errors.append(f"{path} has type {_schema_type(value)}, expected table")
                continue
            fields = SCHEMA_TABLE_FIELDS[key]
            for child_key, child_value in value.items():
                expected = fields.get(child_key)
                child_path = f"{path}.{child_key}"
                if expected is None:
                    errors.append(f"{path} has unknown key {child_key!r}")
                else:
                    errors.extend(_validate_schema_value(child_value, expected, child_path))
        elif key == "floor_gaps":
            if not isinstance(value, list):
                errors.append(f"{path} has type {_schema_type(value)}, expected array")
                continue
            if constants and len(value) > constants.get("MAX_FLOOR_GAPS", 16):
                errors.append(f"{path} has {len(value)} items (max {constants['MAX_FLOOR_GAPS']})")
            for index, item in enumerate(value):
                errors.extend(_validate_schema_value(item, "integer", f"{path}[{index}]"))
        else:
            if not isinstance(value, list):
                errors.append(f"{path} has type {_schema_type(value)}, expected array")
                continue
            if constants:
                max_name = COUNT_LIMITS[key]
                max_count = constants.get(max_name)
                if max_count is not None and len(value) > max_count:
                    errors.append(f"{path} has {len(value)} items (max {max_count})")
            fields = SCHEMA_ARRAY_FIELDS[key]
            for index, item in enumerate(value):
                item_path = f"{path}[{index}]"
                if not isinstance(item, dict):
                    errors.append(
                        f"{item_path} has type {_schema_type(item)}, expected table"
                    )
                    continue
                for child_key, child_value in item.items():
                    expected = fields.get(child_key)
                    child_path = f"{item_path}.{child_key}"
                    if expected is None:
                        errors.append(f"{item_path} has unknown key {child_key!r}")
                    else:
                        errors.extend(_validate_schema_value(child_value, expected, child_path))
                for required_key in REQUIRED_ARRAY_FIELDS.get(key, set()):
                    if required_key not in item:
                        errors.append(f"{item_path} missing required key {required_key!r}")

    return errors


def load_max_constants() -> dict[str, int]:
    constants: dict[str, int] = {}
    define_re = re.compile(
        r"^\s*#define\s+([A-Z][A-Z0-9_]*)\s+([0-9]+)\b"
    )
    shared_names = {"GAME_W", "GAME_H", "TILE_SIZE"} | {
        f"{kind}_{dimension}" for kind in ("VINE", "LADDER", "ROPE")
        for dimension in ("W", "H", "STEP")
    }

    for header in (ROOT / "src").rglob("*.h"):
        for line in header.read_text(encoding="utf-8").splitlines():
            match = define_re.match(line)
            if match and (match.group(1).startswith("MAX_") or
                          match.group(1) in shared_names):
                constants[match.group(1)] = int(match.group(2))

    return constants


def load_level(path: Path) -> dict:
    try:
        with path.open("rb") as fp:
            return tomllib.load(fp)
    except tomllib.TOMLDecodeError as exc:
        raise ValueError(f"{path.relative_to(ROOT)}: TOML parse failed: {exc}") from exc


def normalize_campaign_path(value) -> str | None:
    if (
        not isinstance(value, str)
        or not value
        or "\x00" in value
        or "\\" in value
        or ":" in value
    ):
        return None
    candidate = Path(value)
    if (
        candidate.is_absolute()
        or candidate.parent.as_posix() != "levels"
        or candidate.suffix != ".toml"
        or candidate.name in {"", ".toml", "..toml"}
        or candidate.as_posix() != value
    ):
        return None
    return candidate.as_posix()


def campaign_manifest_entries(
    manifest_path: Path | None = None,
) -> tuple[list[tuple[str, Path, dict]], list[str]]:
    errors: list[str] = []
    entries: list[tuple[str, Path, dict]] = []
    manifest_path = manifest_path or CAMPAIGN_MANIFEST
    manifest_label = manifest_path.relative_to(ROOT)

    if not manifest_path.is_file():
        return [], [f"{manifest_label}: mandatory manifest is missing"]
    try:
        manifest = load_level(manifest_path)
    except ValueError as exc:
        return [], [str(exc)]

    nul_errors = _embedded_nul_errors(manifest, "manifest")
    if nul_errors:
        return [], [f"{manifest_label}: {error}" for error in nul_errors]

    unknown = sorted(set(manifest) - CAMPAIGN_MANIFEST_FIELDS)
    if unknown:
        errors.append(
            f"{manifest_label}: unsupported fields: {', '.join(unknown)}"
        )

    version = manifest.get("format_version")
    if isinstance(version, bool) or not isinstance(version, int) or version != CAMPAIGN_MANIFEST_VERSION:
        errors.append(
            f"{manifest_label}: format_version must be explicit integer "
            f"{CAMPAIGN_MANIFEST_VERSION}"
        )

    listed = manifest.get("levels")
    if not isinstance(listed, list) or not listed:
        errors.append(f"{manifest_label}: levels must be a nonempty array")
        return [], errors

    seen: set[str] = set()
    for index, value in enumerate(listed):
        normalized = normalize_campaign_path(value)
        if normalized is None:
            errors.append(
                f"{manifest_label}: levels[{index}] must be a safe "
                f"levels/*.toml path"
            )
            continue
        if normalized in seen:
            errors.append(
                f"{manifest_label}: duplicate level path {normalized}"
            )
            continue
        seen.add(normalized)
        level_path = ROOT / normalized
        if not level_path.is_file():
            errors.append(
                f"{manifest_label}: missing level file {normalized}"
            )
            continue
        try:
            data = load_level(level_path)
        except ValueError as exc:
            errors.append(str(exc))
            continue
        entries.append((normalized, level_path, data))

    for index, (path, _, data) in enumerate(entries):
        actual = ""
        last_star = data.get("last_star")
        if isinstance(last_star, dict):
            actual = str(last_star.get("next_phase") or "")
        if index + 1 < len(entries):
            expected = entries[index + 1][0]
            if normalize_campaign_path(actual) != expected:
                errors.append(
                    f"{path}: next_phase must reference manifest successor {expected}"
                )
        elif actual:
            errors.append(f"{path}: final campaign level must not have next_phase")

    return entries, errors


def load_asset_manifest() -> set[str]:
    manifest: set[str] = set()

    for path in ASSET_DIR.rglob("*"):
        rel_parts = path.relative_to(ASSET_DIR).parts
        if not path.is_file() or any(part.startswith(".") for part in rel_parts):
            continue
        manifest.add(path.relative_to(ROOT).as_posix())

    return manifest


def field_leaf(field: str) -> str:
    return field.rsplit(".", 1)[-1]


def validate_safe_repo_path(level_path: Path, field: str, value: str) -> tuple[str | None, list[str]]:
    errors: list[str] = []
    if value == "":
        return None, errors

    if "\\" in value:
        errors.append(f"{level_path.relative_to(ROOT)}: {field} must use forward slashes: {value}")
        return None, errors

    candidate = Path(value)
    if candidate.is_absolute() or ".." in candidate.parts:
        errors.append(f"{level_path.relative_to(ROOT)}: {field} must stay inside repo: {value}")
        return None, errors

    return candidate.as_posix(), errors


def validate_asset_reference(
    level_path: Path,
    field: str,
    value: str,
    asset_manifest: set[str],
) -> list[str]:
    repo_path, errors = validate_safe_repo_path(level_path, field, value)
    if repo_path is None:
        return errors

    leaf = field_leaf(field)
    rule = ASSET_PATH_RULES.get(leaf)
    if rule:
        prefixes, suffixes = rule
        if not any(repo_path.startswith(prefix) for prefix in prefixes):
            allowed = " or ".join(prefixes)
            errors.append(
                f"{level_path.relative_to(ROOT)}: {field} must live under {allowed}: {value}"
            )
        suffix = Path(repo_path).suffix
        if suffix not in suffixes:
            allowed = ", ".join(sorted(suffixes))
            errors.append(
                f"{level_path.relative_to(ROOT)}: {field} must use {allowed}: {value}"
            )

    if repo_path not in asset_manifest:
        errors.append(f"{level_path.relative_to(ROOT)}: {field} not in asset manifest: {value}")

    return errors


def validate_phase_reference(level_path: Path, field: str, value: str) -> list[str]:
    repo_path, errors = validate_safe_repo_path(level_path, field, value)
    if repo_path is None:
        return errors

    if not repo_path.startswith("levels/") or Path(repo_path).suffix != ".toml":
        errors.append(f"{level_path.relative_to(ROOT)}: {field} must reference levels/*.toml: {value}")
        return errors

    full_path = ROOT / repo_path
    if not full_path.is_file():
        errors.append(f"{level_path.relative_to(ROOT)}: {field} missing file: {value}")

    return errors


def validate_paths(
    level_path: Path,
    value,
    asset_manifest: set[str],
    field_path: str = "",
) -> list[str]:
    errors: list[str] = []

    if isinstance(value, dict):
        for key, child in value.items():
            child_path = f"{field_path}.{key}" if field_path else key
            if key in PATH_KEYS and isinstance(child, str):
                if key == "next_phase":
                    errors.extend(validate_phase_reference(level_path, child_path, child))
                else:
                    errors.extend(validate_asset_reference(level_path, child_path, child, asset_manifest))
            else:
                errors.extend(validate_paths(level_path, child, asset_manifest, child_path))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(validate_paths(level_path, child, asset_manifest, f"{field_path}[{index}]"))

    return errors


def validate_source_asset_literals(asset_manifest: set[str]) -> list[str]:
    errors: list[str] = []

    for source_path in sorted((ROOT / "src").rglob("*.[ch]")):
        rel_source = source_path.relative_to(ROOT)
        for line_no, line in enumerate(source_path.read_text(encoding="utf-8").splitlines(), 1):
            for value in ASSET_LITERAL_RE.findall(line):
                if value not in asset_manifest:
                    errors.append(f"{rel_source}:{line_no}: asset literal not in manifest: {value}")

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
            return
        rail = rails[rail_index]
        if not isinstance(rail, dict):
            return
        w, h = rail.get("w", 2), rail.get("h", 2)
        if not isinstance(w, int) or not isinstance(h, int):
            return
        count = 2 * w + 2 * (h - 2) if rail.get("layout", "RECT") == "RECT" else w
        t = value.get("t_offset", 0.0)
        if not _is_finite_number(t) or not 0 <= t < count or (rail.get("layout") == "HORIZ" and t > count - 1):
            errors.append(f"{field}[{index}].t_offset must lie on the referenced rail")

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


def validate_rail_geometry(level_path: Path, data: dict, constants: dict[str, int]) -> list[str]:
    errors: list[str] = []
    rails = data.get("rails", [])
    max_tiles = constants.get("MAX_RAIL_TILES", 128)

    if not isinstance(rails, list):
        return errors

    for index, rail in enumerate(rails):
        if not isinstance(rail, dict):
            continue
        layout = rail.get("layout", "RECT")
        w = rail.get("w")
        h = rail.get("h")
        if not isinstance(w, int) or isinstance(w, bool):
            errors.append(f"{level_path.relative_to(ROOT)}: rails[{index}].w must be an integer")
            continue
        if layout == "RECT":
            if not isinstance(h, int) or isinstance(h, bool):
                errors.append(f"{level_path.relative_to(ROOT)}: rails[{index}].h must be an integer")
                continue
            if w < 2 or w > max_tiles:
                errors.append(f"{level_path.relative_to(ROOT)}: rails[{index}].w {w} out of range (2..{max_tiles})")
            if h < 2 or h > max_tiles:
                errors.append(f"{level_path.relative_to(ROOT)}: rails[{index}].h {h} out of range (2..{max_tiles})")
            tile_count = w * 2 + (h - 2) * 2
            if tile_count > max_tiles:
                errors.append(f"{level_path.relative_to(ROOT)}: rails[{index}] has {tile_count} tiles (max {max_tiles})")
        elif layout == "HORIZ":
            if w < 2 or w > max_tiles:
                errors.append(f"{level_path.relative_to(ROOT)}: rails[{index}].w {w} out of range (2..{max_tiles})")
            end_cap = rail.get("end_cap", 0)
            if end_cap not in (0, 1):
                errors.append(f"{level_path.relative_to(ROOT)}: rails[{index}].end_cap must be 0 or 1")
        else:
            errors.append(f"{level_path.relative_to(ROOT)}: rails[{index}].layout invalid: {layout}")

    return errors


def validate_nested_dimensions(level_path: Path, data: dict, constants: dict[str, int]) -> list[str]:
    errors: list[str] = []
    max_spike_tiles = constants.get("MAX_SPIKE_TILES", 16)
    max_bridge_bricks = constants.get("MAX_BRIDGE_BRICKS", 16)

    def check_range(array_name: str, key: str, lo: int, hi: int) -> None:
        items = data.get(array_name, [])
        if not isinstance(items, list):
            return
        for index, item in enumerate(items):
            if not isinstance(item, dict):
                continue
            value = item.get(key)
            if isinstance(value, bool) or not isinstance(value, int):
                errors.append(f"{level_path.relative_to(ROOT)}: {array_name}[{index}].{key} must be an integer")
            elif value < lo or value > hi:
                errors.append(f"{level_path.relative_to(ROOT)}: {array_name}[{index}].{key} {value} out of range ({lo}..{hi})")

    check_range("spike_rows", "count", 1, max_spike_tiles)
    check_range("spike_platforms", "tile_count", 1, max_spike_tiles)
    check_range("float_platforms", "tile_count", 1, max_spike_tiles)
    check_range("bridges", "brick_count", 1, max_bridge_bricks)

    screens = data.get("screen_count", 4)
    screens = screens if isinstance(screens, int) and 0 < screens <= 99 else 4
    # Match level_validate.c's rendered climbable rectangles, not just counts.
    for array, kind in (("vines", "VINE"), ("ladders", "LADDER"), ("ropes", "ROPE")):
        items = data.get(array, [])
        if not isinstance(items, list):
            continue
        for index, item in enumerate(items):
            if not isinstance(item, dict):
                continue
            count = item.get("tile_count", 1)
            x, y = item.get("x", 0), item.get("y", 0)
            if not isinstance(count, int) or isinstance(count, bool) or not _is_finite_number(x) or not _is_finite_number(y):
                continue  # Type errors are reported by the schema pass.
            x, y = ctypes.c_float(float(x)).value, ctypes.c_float(float(y)).value
            height = constants[f"{kind}_H"] + (count - 1) * constants[f"{kind}_STEP"]
            field = f"{level_path.relative_to(ROOT)}: {array}[{index}]"
            if count < 1:
                errors.append(f"{field}.tile_count must be positive")
            elif x < 0 or x + constants[f"{kind}_W"] > screens * constants["GAME_W"]:
                errors.append(f"{field}.x plus width is outside world bounds")
            elif y < 0 or y + height > constants["GAME_H"]:
                errors.append(f"{field}.y plus rendered height {height} is outside world bounds")
    check_range("platforms", "tile_height", 1, (300 - 48 + 16) // 48)
    # Width may be omitted/zero (one tile), unlike height.
    for item in data.get("platforms", []) if isinstance(data.get("platforms", []), list) else []:
        if isinstance(item, dict):
            width = item.get("tile_width", 0)
            if isinstance(width, (int, float)) and (width < 0 or width > screens * 400 // 48):
                errors.append("platforms[].tile_width is outside world bounds")

    max_motion = constants.get("MAX_LEVEL_MOTION", 10000)
    physics = data.get("physics", {})
    values = list(physics.items()) if isinstance(physics, dict) else []
    for array, items in data.items():
        if not isinstance(items, list):
            continue
        for item in items:
            if not isinstance(item, dict):
                continue
            values.extend((f"{array}.{key}", item[key]) for key in ("vx", "launch_vy", "speed") if key in item)
            if array in {"spiders", "birds", "faster_birds"}:
                frame = item.get("frame_index", 0)
                if isinstance(frame, int) and not 0 <= frame < 3:
                    errors.append(f"{array}.frame_index is outside the sprite sheet")
    for key, value in values:
        if not _is_finite_number(value) or abs(value) > max_motion:
            errors.append(f"{key} must be finite and within +/-{max_motion}")

    return errors


def validate_checkpoints(level_path: Path, data: dict, constants: dict[str, int]) -> list[str]:
    errors: list[str] = []
    checkpoints = data.get("checkpoints", [])
    if not isinstance(checkpoints, list):
        return errors
    max_count = constants.get("MAX_CHECKPOINTS", 99)
    if len(checkpoints) > max_count:
        errors.append(
            f"{level_path.relative_to(ROOT)}: checkpoints has {len(checkpoints)} "
            f"items (max {max_count})"
        )

    screen_count = data.get("screen_count")
    screens = screen_count if isinstance(screen_count, int) and not isinstance(screen_count, bool) and screen_count > 0 else 4
    world_w = screens * constants.get("GAME_W", 400)
    tile_size = constants.get("TILE_SIZE", 48)
    start_x_value = data.get("player_start_x", 0.0)
    start_y_value = data.get("player_start_y", 0.0)
    start_x = 80.0
    if (
        isinstance(start_x_value, (int, float))
        and not isinstance(start_x_value, bool)
        and isinstance(start_y_value, (int, float))
        and not isinstance(start_y_value, bool)
        and _is_finite_number(start_x_value)
        and _is_finite_number(start_y_value)
        and (start_x_value != 0.0 or start_y_value != 0.0)
    ):
        start_x = ctypes.c_float(float(start_x_value)).value
    seen_x: set[float] = set()

    for index, checkpoint in enumerate(checkpoints):
        path = f"{level_path.relative_to(ROOT)}: checkpoints[{index}]"
        if not isinstance(checkpoint, dict):
            errors.append(f"{path} must be a table")
            continue
        x = checkpoint.get("x")
        y = checkpoint.get("y")
        if "x" not in checkpoint:
            errors.append(f"{path} missing required key 'x'")
        if "y" not in checkpoint:
            errors.append(f"{path} missing required key 'y'")
        if not _is_finite_number(x):
            errors.append(f"{path}.x must be a finite number")
        if not _is_finite_number(y):
            errors.append(f"{path}.y must be a finite number")
        if not _is_finite_number(x) or not _is_finite_number(y):
            continue
        x_value = ctypes.c_float(float(x)).value if isinstance(x, (int, float)) else 0.0
        y_value = ctypes.c_float(float(y)).value if isinstance(y, (int, float)) else 0.0
        if x_value in seen_x:
            errors.append(f"{path}.x duplicates an earlier checkpoint")
        seen_x.add(x_value)
        if x_value <= start_x:
            errors.append(f"{path}.x must be strictly after effective start x {start_x:g}")
        if x_value < 0 or x_value > world_w - tile_size:
            errors.append(f"{path}.x {x_value} out of range (0..{world_w - tile_size})")
        if y_value < 0 or y_value > 300:
            errors.append(f"{path}.y {y_value} out of range (0..300)")

    return errors


def validate_level(
    level_path: Path,
    constants: dict[str, int],
    asset_manifest: set[str],
    *,
    require_explicit_version: bool = False,
) -> list[str]:
    data = load_level(level_path)
    nul_errors = _embedded_nul_errors(data)
    if nul_errors:
        return [
            f"{level_path.relative_to(ROOT)}: {error}"
            for error in nul_errors
        ]

    errors: list[str] = []

    errors.extend(
        f"{level_path.relative_to(ROOT)}: {error}"
        for error in validate_schema(
            data,
            require_explicit_version=require_explicit_version,
            constants=constants,
        )
    )

    screen_count = data.get("screen_count")
    if isinstance(screen_count, bool) or not isinstance(screen_count, int) or screen_count <= 0:
        errors.append(f"{level_path.relative_to(ROOT)}: screen_count must be a positive integer")
    elif screen_count > constants.get("MAX_LEVEL_SCREENS", 99):
        errors.append(
            f"{level_path.relative_to(ROOT)}: screen_count has {screen_count} "
            f"screens (max {constants.get('MAX_LEVEL_SCREENS', 99)})"
        )

    errors.extend(validate_paths(level_path, data, asset_manifest))
    errors.extend(validate_counts(level_path, data, constants))
    errors.extend(validate_rail_geometry(level_path, data, constants))
    errors.extend(validate_rail_links(level_path, data))
    errors.extend(validate_nested_dimensions(level_path, data, constants))
    errors.extend(validate_checkpoints(level_path, data, constants))

    return errors


def main() -> int:
    constants = load_max_constants()
    asset_manifest = load_asset_manifest()
    level_paths = sorted(LEVEL_DIR.glob("*.toml")) + sorted((LEVEL_DIR / "labs").glob("*.toml"))

    if not level_paths:
        sys.stderr.write("validate_levels: no levels/*.toml files found\n")
        return 1
    if not asset_manifest:
        sys.stderr.write("validate_levels: no assets found\n")
        return 1

    errors: list[str] = []
    _, campaign_errors = campaign_manifest_entries()
    errors.extend(campaign_errors)
    for level_path in level_paths:
        try:
            errors.extend(
                validate_level(
                    level_path,
                    constants,
                    asset_manifest,
                    require_explicit_version=True,
                )
            )
        except ValueError as exc:
            errors.append(str(exc))

    errors.extend(validate_source_asset_literals(asset_manifest))

    if errors:
        for error in errors:
            sys.stderr.write(f"validate_levels: {error}\n")
        return 1

    print(f"validate_levels: ok ({len(level_paths)} levels, {len(asset_manifest)} assets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
