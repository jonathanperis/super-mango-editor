#!/usr/bin/env python3
"""The fuzz dictionary names every key of the level schema.

tests/fuzz/toml.dict gives libFuzzer the words the level loader reads. A key
added to the schema table in src/shared/serializer_parse.c but not to the
dictionary would be a field the fuzzer almost never reaches, so this test
reads the key names out of the C table and fails for any that is missing.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "src" / "shared" / "serializer_parse.c"
DICTIONARY = ROOT / "tests" / "fuzz" / "toml.dict"

# A schema row is a macro call that starts with its key:
# `FLOAT_AT("patrol_x0", SpiderPlacement, patrol_x0)` for a field,
# `ROOT_TABLES("coins", ...` or `ROOT_INT("music_volume", ...` for a root key.
SCHEMA_ROW_RE = re.compile(
    r'\b(?:[A-Z_]*FIELD|[A-Z_]+_AT(?:_[A-Z_]+)?|ROOT_[A-Z_]+)\(\s*"([a-z_0-9]+)"'
)
DICT_TOKEN_RE = re.compile(r'^"((?:[^"\\]|\\.)*)"$')


def schema_keys() -> set[str]:
    return set(SCHEMA_ROW_RE.findall(SCHEMA.read_text(encoding="utf-8")))


def dictionary_tokens() -> set[str]:
    tokens: set[str] = set()
    for number, line in enumerate(DICTIONARY.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        match = DICT_TOKEN_RE.match(line)
        if not match:
            raise AssertionError(f"{DICTIONARY.name}:{number}: not a quoted token: {line}")
        tokens.add(match.group(1))
    return tokens


def main() -> int:
    keys = schema_keys()
    if len(keys) < 60:
        raise AssertionError(f"only {len(keys)} schema keys found; the table format changed")
    missing = sorted(keys - dictionary_tokens())
    if missing:
        raise AssertionError("tests/fuzz/toml.dict is missing schema keys: " + ", ".join(missing))
    print(f"fuzz_dict_test: ok ({len(keys)} schema keys)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
