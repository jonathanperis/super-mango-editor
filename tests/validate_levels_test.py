#!/usr/bin/env python3
"""Coverage for tools/validate_levels.py: cross-file checks and the docs schema.

Per-level runtime rules belong to the C validator; level_serializer_test.c
loads every fixture below through the real loader.
"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import validate_levels  # noqa: E402


FIXTURE_DIR = ROOT / "tests" / "fixtures" / "serializer_v1"
CAMPAIGN_FIXTURE_DIR = ROOT / "tests" / "fixtures" / "campaign_manifest"
# Fixtures whose type shape is wrong, so the docs-snippet schema rejects them
# too. bad_checkpoint_bounds/duplicate and bad_screen_count_max_plus_one break
# runtime rules only the C validator applies.
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
    "bad_checkpoint_array.toml",
    "bad_utf8_overlong.toml",
    "bad_utf8_surrogate.toml",
    "bad_utf8_above_max.toml",
    "bad_utf8_truncated.toml",
    "bad_utf8_stray_continuation.toml",
    "bad_utf8_legacy.toml",
)


# Mirrors level_reference_rule() in tests/level_validate_test.c.
LEVEL_REFERENCE_CASES = (
    ("levels/01_lugio_01.toml", True),
    ("levels/café.toml", True),
    ("levels/console.toml", True),
    ("levels/com10.toml", True),
    ("levels/x.y.toml", True),
    ("levels/labs/01_collision.toml", False),
    ("levels/con.toml", False),
    ("levels/CON.toml", False),
    ("levels/nul.x.toml", False),
    ("levels/Aux .toml", False),
    ("levels/prn.toml", False),
    ("levels/com1.toml", False),
    ("levels/LPT9.toml", False),
    ("levels/com0.toml", False),
    ("levels/com¹.toml", False),
    ("levels/lpt³.toml", False),
    ("levels/a:b.toml", False),
    ("levels/a?.toml", False),
    ("levels/a\x7f.toml", False),
    ("levels/a\\b.toml", False),
    ("levels/.toml", False),
    ("levels/..toml", False),
    ("levels/.hidden.toml", False),
    ("levels/a.txt", False),
    ("assets/a.toml", False),
)


def check_level_references() -> None:
    level = FIXTURE_DIR / "valid_v1.toml"
    for value, valid in LEVEL_REFERENCE_CASES:
        if validate_levels.level_ref_valid(value) != valid:
            raise AssertionError(f"level reference rule mismatch: {value!r}")
        if (validate_levels.normalize_campaign_path(value) is not None) != valid:
            raise AssertionError(f"campaign path rule mismatch: {value!r}")
        # Valid names may not exist on disk; only shape errors matter here.
        errors = [
            error for error in validate_levels.validate_phase_reference(level, "next_phase", value)
            if "missing file" not in error
        ]
        if (not errors) != valid:
            raise AssertionError(f"next_phase rule mismatch: {value!r}: {errors}")


def load_fixture(name: str) -> dict:
    return validate_levels.load_level(FIXTURE_DIR / name)


def main() -> int:
    check_level_references()
    constants = validate_levels.load_max_constants()
    asset_manifest = validate_levels.load_asset_manifest()

    if validate_levels.validate_schema(load_fixture("valid_legacy.toml")):
        raise AssertionError("valid legacy fixture rejected")
    for value in (float("nan"), float("inf"), 1e30, 2**63 - 1):
        if not validate_levels.validate_schema({"music_volume": value}):
            raise AssertionError("unsafe legacy integer conversion accepted")
    if validate_levels.validate_schema(
        load_fixture("valid_v1.toml"), constants=constants
    ):
        raise AssertionError("valid v1 fixture rejected")
    for name in ("valid_v1.toml", "valid_utf8.toml", "valid_screen_count_max.toml"):
        errors = validate_levels.validate_level(FIXTURE_DIR / name, asset_manifest)
        if errors:
            raise AssertionError(f"valid fixture rejected: {name}: {errors}")
    # The loader still reads legacy files; checked-in levels must not be one.
    if not any("format_version" in error for error in validate_levels.validate_level(
            FIXTURE_DIR / "valid_legacy.toml", asset_manifest)):
        raise AssertionError("checked-in level policy accepted a missing format_version")

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
        try:
            errors = validate_levels.validate_schema(
                load_fixture(name),
                require_explicit_version=True,
                constants=constants,
            )
        except ValueError as exc:  # unreadable file; main() reports it the same way
            errors = [str(exc)]
        if not errors:
            raise AssertionError(f"invalid fixture accepted: {name}")

    # Campaign entries share the C loader's 256-byte buffer (255 + NUL),
    # counted in UTF-8 bytes: "é" is two.
    def campaign_path(size: int, fill: str = "a") -> str:
        stem = fill * ((size - len("levels/.toml")) // len(fill.encode("utf-8")))
        return f"levels/{stem}.toml"
    longest = campaign_path(255)
    if len(longest.encode("utf-8")) != 255 or validate_levels.normalize_campaign_path(longest) is None:
        raise AssertionError("255-byte campaign path rejected")
    for too_long in (campaign_path(256), campaign_path(256, "é")):
        if len(too_long.encode("utf-8")) != 256:
            raise AssertionError(f"bad test path length: {len(too_long.encode('utf-8'))}")
        if validate_levels.normalize_campaign_path(too_long) is not None:
            raise AssertionError("256-byte campaign path accepted")

    for name in (
        "bad_format_version_key_embedded_nul.toml",
        "bad_levels_key_embedded_nul.toml",
        "bad_path_embedded_nul.toml",
        "bad_path_too_long.toml",
    ):
        _, errors = validate_levels.campaign_manifest_entries(
            CAMPAIGN_FIXTURE_DIR / name
        )
        if not errors:
            raise AssertionError(f"invalid campaign fixture accepted: {name}")

    check_campaign_display_names()

    print("validate_levels_test: ok")
    return 0


def check_campaign_display_names() -> None:
    """A campaign entry must have something to show in the level selector.

    The game (campaign_derive_display_name in level_session.c) shows `name`,
    or the file name when `name` is blank, and refuses a campaign whose
    entry would show nothing.  `levels/   .toml` passes every path rule, so
    with an empty `name` it used to pass validation and then fail in game.
    """
    show = validate_levels.campaign_display_name
    if show("levels/   .toml", {"name": "Lava Run"}) != "Lava Run":
        raise AssertionError("visible name not used")
    if show("levels/lava.toml", {"name": " \t "}) != "lava":
        raise AssertionError("blank name did not fall back to the file name")
    if show("levels/   .toml", {"name": ""}).strip(validate_levels.C_SPACE_CHARS):
        raise AssertionError("spaces-only file name counted as visible")

    original_root = validate_levels.ROOT
    with tempfile.TemporaryDirectory() as scratch:
        root = Path(scratch)
        (root / "levels").mkdir()
        level_text = (FIXTURE_DIR / "valid_legacy.toml").read_text(encoding="utf-8")
        (root / "levels" / "   .toml").write_text(level_text, encoding="utf-8")
        manifest = root / "main.toml"
        manifest.write_text(
            'format_version = 1\nlevels = ["levels/   .toml"]\n', encoding="utf-8"
        )
        validate_levels.ROOT = root
        try:
            for name, expect_error in (("", True), ("Spaces", False)):
                body = "\n".join(
                    line for line in level_text.splitlines()
                    if not line.startswith("name ")
                )
                (root / "levels" / "   .toml").write_text(
                    f'name = "{name}"\n{body}\n', encoding="utf-8"
                )
                _, errors = validate_levels.campaign_manifest_entries(manifest)
                found = any("no display name" in error for error in errors)
                if found != expect_error:
                    raise AssertionError(
                        f"display-name check for name={name!r}: {errors}"
                    )
        finally:
            validate_levels.ROOT = original_root


if __name__ == "__main__":
    raise SystemExit(main())
