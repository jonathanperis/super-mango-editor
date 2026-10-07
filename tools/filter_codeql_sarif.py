#!/usr/bin/env python3
"""Drop CodeQL code-quality results reported inside third-party code.

The C/C++ CodeQL job compiles the game, so it also extracts the pinned raylib
build headers under out/ and the vendored tomlc17 parser under vendor/. For
compiled languages CodeQL's `paths-ignore` setting does not filter results, so
style notes about raylib's commented-out code or tomlc17's layout would sit in
the Security tab forever, where nobody can fix them.

This script edits the SARIF file between analysis and upload:

  * results located under an ignored prefix are removed, but only when their
    rule is NOT a security rule;
  * security results are always kept, wherever they are. The vendored parser
    reads untrusted level files, so a real security finding there must stay
    visible.

A rule counts as "security" when CodeQL gives it a security-severity score or
a "security" tag, which is how GitHub decides what is a security alert.

Usage: filter_codeql_sarif.py SARIF_FILE [SARIF_FILE ...]   (edited in place)
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

IGNORED_PREFIXES = ("out/", "vendor/")


def rule_is_security(rule: dict) -> bool:
    properties = rule.get("properties", {})
    tags = properties.get("tags", [])
    return "security-severity" in properties or "security" in tags


def security_rules(run: dict) -> set[str]:
    """Collect the ids of security rules from the tool and its extensions."""
    tool = run.get("tool", {})
    components = [tool.get("driver", {})] + tool.get("extensions", [])
    return {rule["id"] for component in components
            for rule in component.get("rules", [])
            if "id" in rule and rule_is_security(rule)}


def result_path(result: dict) -> str:
    """Repository-relative path of the result's primary location, or ''."""
    locations = result.get("locations") or [{}]
    artifact = locations[0].get("physicalLocation", {}).get("artifactLocation", {})
    return artifact.get("uri", "")


def filter_sarif(sarif: dict) -> int:
    """Remove ignorable results in place and return how many were removed."""
    removed = 0
    for run in sarif.get("runs", []):
        security = security_rules(run)
        kept = []
        for result in run.get("results", []):
            third_party = result_path(result).startswith(IGNORED_PREFIXES)
            if third_party and result.get("ruleId") not in security:
                removed += 1
                continue
            kept.append(result)
        run["results"] = kept
    return removed


def main(paths: list[str]) -> int:
    if not paths:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    for name in paths:
        path = Path(name)
        sarif = json.loads(path.read_text(encoding="utf-8"))
        removed = filter_sarif(sarif)
        path.write_text(json.dumps(sarif), encoding="utf-8")
        print(f"{path}: removed {removed} non-security result(s) in third-party code")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
