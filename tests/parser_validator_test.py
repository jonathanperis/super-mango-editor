#!/usr/bin/env python3
"""Encoding boundaries shared by tomlc17 and the Python level validator."""

import sys
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import validate_levels  # noqa: E402
import check_docs_drift  # noqa: E402
import generate_level_catalog  # noqa: E402
import run_scripted_smoke  # noqa: E402


def main() -> int:
    bom = b"\xef\xbb\xbf"
    document = b'format_version=1\nscreen_count=4\nname="Mango"\n'
    path = ROOT / "tests" / "parser-encoding.toml"
    cases = (
        (document, True),
        (bom + document, True),
        (bom + bom + document, False),
        (b"\n" + bom + document, False),
        (bom[:1], False),
        (bom[:2], False),
        (b"format_version=1\rscreen_count=4", False),
        (bom + document.replace(b"\n", b"\r"), False),
        (bom + document.replace(b"\n", b"\r\n"), True),
        (bom + b'format_version=1\nname="' + bom + b'"', True),
    )
    readers = (
        ("validator", validate_levels.load_level),
        ("docs drift", check_docs_drift.load_level),
        ("catalog", generate_level_catalog.load_level),
        ("scripted smoke", lambda path: run_scripted_smoke.check_builtin_behavior(
            {"elapsed": 5 / 60, "paused": 0, "complete": 0, "x": 81},
            "move-right", path, 5)),
    )
    for name, reader in readers:
        for data, valid in cases:
            with patch.object(Path, "read_bytes", return_value=data):
                try:
                    result = reader(path)
                except ValueError:
                    if valid:
                        raise
                else:
                    assert valid, f"{name}: invalid encoding accepted: {data!r}"
                    if result is not None:  # Smoke checks behavior, not a returned document.
                        assert result["format_version"] == 1
                        assert not validate_levels.validate_schema(result)
                        assert result["name"] == ("\ufeff" if data.endswith(bom + b'"') else "Mango")
    print(f"parser_validator_test: ok ({len(cases)} encoding cases x {len(readers)} readers)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
