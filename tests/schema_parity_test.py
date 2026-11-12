#!/usr/bin/env python3
"""tools/validate_levels.py describes the same level schema as the C table.

The game's serializer keeps one table of every key a level file may hold
(src/shared/serializer_parse.c).  tools/validate_levels.py keeps a second
copy for checking the TOML snippets in the manual.  Two copies of one fact
drift: this test runs `level-check --dump-schema`, which prints the C table,
and fails for any key, type, enum value, string capacity, required key or
array limit that the Python copy states differently or not at all.

Usage: schema_parity_test.py <path to level-check>
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import validate_levels  # noqa: E402  (needs the path set above)


def parse_dump(text: str) -> dict:
    """Turn the dump's lines into the shapes validate_levels.py uses."""
    schema = {
        "scalars": {},      # key -> type
        "tables": {},       # table -> {key: type or ("enum", values)}
        "arrays": {},       # array -> {key: type or ("enum", values)}
        "required": {},     # owner -> {keys}
        "limits": {},       # array -> max item count
        "capacities": {},   # key -> bytes
    }
    kind_of_owner: dict[str, str] = {}
    for line in text.splitlines():
        words = line.split()
        if not words:
            continue
        extras = dict(word.split("=", 1) for word in words if "=" in word)
        if words[0] == "value":
            schema["scalars"][words[1]] = words[2]
            if "capacity" in extras:
                schema["capacities"][words[1]] = int(extras["capacity"])
        elif words[0] == "integers":
            schema["limits"][words[1]] = int(extras["max"])
        elif words[0] == "table":
            schema["tables"][words[1]] = {}
            kind_of_owner[words[1]] = "tables"
        elif words[0] == "tables":
            schema["arrays"][words[1]] = {}
            schema["limits"][words[1]] = int(extras["max"])
            kind_of_owner[words[1]] = "arrays"
        elif words[0] == "field":
            owner, key, value_type = words[1], words[2], words[3]
            expected = value_type
            if value_type == "enum":
                expected = ("enum", tuple(extras["values"].split(",")))
            schema[kind_of_owner[owner]][owner][key] = expected
            if "required" in words:
                schema["required"].setdefault(owner, set()).add(key)
            if "capacity" in extras:
                # The same key may appear in several tables (every layer has
                # a "path"); the Python copy keeps one capacity per key name.
                known = schema["capacities"].setdefault(key, int(extras["capacity"]))
                if known != int(extras["capacity"]):
                    raise AssertionError(
                        f"C schema gives key {key!r} two capacities; "
                        "validate_levels.py keys capacities by name alone"
                    )
        else:
            raise AssertionError(f"unknown schema dump line: {line}")
    return schema


def as_plain(fields: dict) -> dict:
    """Enum values as tuples, so the two copies compare equal."""
    return {
        key: (value[0], tuple(value[1])) if isinstance(value, (tuple, list)) else value
        for key, value in fields.items()
    }


def compare(c_schema: dict) -> list[str]:
    problems: list[str] = []

    def differ(what: str, python_value, c_value) -> None:
        if python_value != c_value:
            problems.append(f"{what}: validate_levels.py has {python_value!r}, C has {c_value!r}")

    differ("level-wide keys", validate_levels.SCHEMA_SCALAR_FIELDS, c_schema["scalars"])
    differ("single tables", sorted(validate_levels.SCHEMA_TABLE_FIELDS), sorted(c_schema["tables"]))
    for name in sorted(set(validate_levels.SCHEMA_TABLE_FIELDS) & set(c_schema["tables"])):
        differ(f"[{name}] keys", as_plain(validate_levels.SCHEMA_TABLE_FIELDS[name]),
               c_schema["tables"][name])
    differ("arrays of tables", sorted(validate_levels.SCHEMA_ARRAY_FIELDS),
           sorted(c_schema["arrays"]))
    for name in sorted(set(validate_levels.SCHEMA_ARRAY_FIELDS) & set(c_schema["arrays"])):
        differ(f"[[{name}]] keys", as_plain(validate_levels.SCHEMA_ARRAY_FIELDS[name]),
               c_schema["arrays"][name])
    differ("required keys", validate_levels.REQUIRED_ARRAY_FIELDS, c_schema["required"])

    for key, capacity in sorted(c_schema["capacities"].items()):
        differ(f"capacity of {key!r}", validate_levels.string_capacity(key), capacity)

    constants = validate_levels.load_max_constants()
    differ("arrays with a limit", sorted(validate_levels.COUNT_LIMITS), sorted(c_schema["limits"]))
    for name, constant in sorted(validate_levels.COUNT_LIMITS.items()):
        if name in c_schema["limits"]:
            differ(f"limit of {name} ({constant})", constants.get(constant),
                   c_schema["limits"][name])
    return problems


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    dump = subprocess.run([argv[1], "--dump-schema"], check=True, capture_output=True,
                          text=True).stdout
    c_schema = parse_dump(dump)
    if len(c_schema["arrays"]) < 20 or len(c_schema["scalars"]) < 10:
        raise AssertionError("the schema dump is nearly empty; its format changed")
    problems = compare(c_schema)
    if problems:
        raise AssertionError("schema drift:\n  " + "\n  ".join(problems))
    keys = sum(len(fields) for fields in c_schema["arrays"].values())
    print(f"schema_parity_test: ok ({len(c_schema['arrays'])} arrays, {keys} keys)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
