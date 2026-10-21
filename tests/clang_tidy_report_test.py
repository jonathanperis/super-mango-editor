#!/usr/bin/env python3
"""Check that tools/run_clang_tidy.py keeps project findings and drops third-party ones."""

from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("run_clang_tidy", ROOT / "tools" / "run_clang_tidy.py")
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def main() -> None:
    output = "\n".join([
        f"{ROOT}/src/editor/tools.c:12:5: warning: Value stored to 'x' is never read "
        "[clang-analyzer-deadcode.DeadStores]",
        f"{ROOT}/src/editor/tools.c:12:5: note: Value stored to 'x' is never read",
        f"{ROOT}/vendor/tomlc17/tomlc17.c:40:1: warning: something [bugprone-branch-clone]",
        f"{ROOT}/out/raylib/build/raylib/include/raylib.h:9:1: warning: x [bugprone-macro-parentheses]",
        "/usr/include/stdio.h:1:1: warning: system header [bugprone-macro-parentheses]",
        f"{ROOT}/tools/level_check.c:3:9: error: 'x' | 'y' [cert-err34-c,bugprone-unused-return-value]",
        "1234 warnings generated.",
    ])
    findings = module.parse_findings(output)
    assert findings == [
        ("src/editor/tools.c", 12, 5, "warning", "clang-analyzer-deadcode.DeadStores",
         "Value stored to 'x' is never read"),
        ("tools/level_check.c", 3, 9, "error", "cert-err34-c,bugprone-unused-return-value", "'x' | 'y'"),
    ], findings

    # A Windows path keeps its drive letter instead of being cut at "C:".
    windows = module.FINDING_RE.match(
        "C:/a/super-mango/src/main.c:7:3: warning: w [bugprone-branch-clone]")
    assert windows and windows["path"] == "C:/a/super-mango/src/main.c" and windows["line"] == "7", windows

    table = module.markdown_summary(findings, 2)
    assert "2 findings in 2 files" in table, table
    assert "'x' \\| 'y'" in table, table  # a | in a message must not split the cell
    assert "No findings in 5 files" in module.markdown_summary([], 5)
    print("clang_tidy_report_test: ok")


if __name__ == "__main__":
    main()
