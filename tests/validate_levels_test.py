#!/usr/bin/env python3
"""Table-driven coverage for Python v1 schema rejection."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import validate_levels  # noqa: E402


FIXTURE_DIR = ROOT / "tests" / "fixtures" / "serializer_v1"
CAMPAIGN_FIXTURE_DIR = ROOT / "tests" / "fixtures" / "campaign_manifest"
INVALID_FIXTURES = (
    "bad_version.toml",
    "bad_scalar_type.toml",
    "bad_fractional_integer.toml",
    "bad_integer_overflow.toml",
    "bad_string_type.toml",
    "bad_numeric_string.toml",
    "bad_nonfinite_number.toml",
    "bad_wrong_array_container.toml",
    "bad_non_table_element.toml",
    "bad_invalid_enum.toml",
    "bad_root_unknown.toml",
    "bad_nested_unknown.toml",
    "bad_floor_gaps.toml",
    "bad_nested_table_type.toml",
    "bad_root_key_embedded_nul.toml",
    "bad_nested_key_embedded_nul.toml",
    "bad_enum_embedded_nul.toml",
    "bad_path_embedded_nul.toml",
    "bad_string_embedded_nul.toml",
    "bad_legacy_format_version_key_embedded_nul.toml",
    "bad_checkpoint_missing_y.toml",
    "bad_checkpoint_type.toml",
    "bad_checkpoint_unknown.toml",
    "bad_checkpoint_nonfinite.toml",
    "bad_checkpoint_bounds.toml",
    "bad_checkpoint_duplicate.toml",
    "bad_checkpoint_array.toml",
    "bad_screen_count_max_plus_one.toml",
)


def load_fixture(name: str) -> dict:
    return validate_levels.load_level(FIXTURE_DIR / name)


def main() -> int:
    constants = validate_levels.load_max_constants()
    asset_manifest = validate_levels.load_asset_manifest()
    for array, valid_count, invalid_count in (("ropes", 6, 7), ("ladders", 14, 22), ("vines", 7, 9)):
        for count, rejected in ((valid_count, False), (invalid_count, True)):
            kind = {"ropes": "ROPE", "ladders": "LADDER", "vines": "VINE"}[array]
            height = constants[f"{kind}_H"] + (count - 1) * constants[f"{kind}_STEP"]
            y = 128 if rejected else 300 - height + 1e-7  # C narrows this to the exact edge.
            errors = validate_levels.validate_nested_dimensions(
                FIXTURE_DIR / "valid_v1.toml",
                {array: [{"x": 128, "y": y, "tile_count": count}]}, constants)
            if bool(errors) != rejected:
                raise AssertionError(f"climbable rendered bounds mismatch: {array}, {count}, {errors}")

    if validate_levels.validate_schema(load_fixture("valid_legacy.toml")):
        raise AssertionError("valid legacy fixture rejected")
    for value in (float("nan"), float("inf"), 1e30, 2**63 - 1):
        if not validate_levels.validate_schema({"music_volume": value}):
            raise AssertionError("unsafe legacy integer conversion accepted")
    if not validate_levels.validate_nested_dimensions(FIXTURE_DIR / "valid_v1.toml",
                                                     {"physics": {"run_max_speed": 1e30}}, constants):
        raise AssertionError("unsafe motion magnitude accepted")
    if validate_levels.validate_level(
        FIXTURE_DIR / "valid_legacy.toml", constants, asset_manifest
    ):
        raise AssertionError("validator rejected valid legacy fixture")
    if validate_levels.validate_schema(
        load_fixture("valid_v1.toml"), constants=constants
    ):
        raise AssertionError("valid v1 fixture rejected")
    if validate_levels.validate_level(
        FIXTURE_DIR / "valid_screen_count_max.toml", constants, asset_manifest,
        require_explicit_version=True,
    ):
        raise AssertionError("valid maximum screen-count fixture rejected")

    too_many = {
        "format_version": 1,
        "screen_count": 99,
        "checkpoints": [
            {"x": float(100 + index), "y": 100.0}
            for index in range(constants["MAX_CHECKPOINTS"] + 1)
        ],
    }
    if not validate_levels.validate_schema(too_many, constants=constants):
        raise AssertionError("checkpoint count overflow accepted")

    for name in INVALID_FIXTURES:
        errors = validate_levels.validate_level(
            FIXTURE_DIR / name,
            constants,
            asset_manifest,
            require_explicit_version=True,
        )
        if not errors:
            raise AssertionError(f"invalid fixture accepted: {name}")

    for name in (
        "bad_format_version_key_embedded_nul.toml",
        "bad_levels_key_embedded_nul.toml",
        "bad_path_embedded_nul.toml",
    ):
        _, errors = validate_levels.campaign_manifest_entries(
            CAMPAIGN_FIXTURE_DIR / name
        )
        if not errors:
            raise AssertionError(f"invalid campaign fixture accepted: {name}")

    print("validate_levels_test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
