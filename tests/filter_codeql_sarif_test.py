#!/usr/bin/env python3
"""Check that only non-security third-party CodeQL results are filtered."""

from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("filter_codeql_sarif", ROOT / "tools" / "filter_codeql_sarif.py")
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def result(rule: str, uri: str) -> dict:
    return {"ruleId": rule, "locations": [{"physicalLocation": {"artifactLocation": {"uri": uri}}}]}


def main() -> None:
    sarif = {"runs": [{
        "tool": {
            "driver": {"rules": [{"id": "cpp/commented-out-code", "properties": {"tags": ["maintainability"]}}]},
            "extensions": [{"rules": [
                {"id": "cpp/path-injection", "properties": {"security-severity": "7.5"}},
                {"id": "cpp/overflow", "properties": {"tags": ["security", "external/cwe/cwe-190"]}},
            ]}],
        },
        "results": [
            result("cpp/commented-out-code", "out/raylib/build/raylib/include/rlgl.h"),  # dropped
            result("cpp/commented-out-code", "vendor/tomlc17/tomlc17.c"),               # dropped
            result("cpp/commented-out-code", "src/editor/tools.c"),                     # our code: kept
            result("cpp/path-injection", "vendor/tomlc17/tomlc17.c"),                   # security: kept
            result("cpp/overflow", "out/raylib/build/raylib/include/raymath.h"),        # security: kept
            {"ruleId": "cpp/commented-out-code"},                                       # no location: kept
        ],
    }]}
    assert module.filter_sarif(sarif) == 2
    kept = [(r["ruleId"], module.result_path(r)) for r in sarif["runs"][0]["results"]]
    assert kept == [
        ("cpp/commented-out-code", "src/editor/tools.c"),
        ("cpp/path-injection", "vendor/tomlc17/tomlc17.c"),
        ("cpp/overflow", "out/raylib/build/raylib/include/raymath.h"),
        ("cpp/commented-out-code", ""),
    ], kept
    print("filter_codeql_sarif_test: ok")


if __name__ == "__main__":
    main()
