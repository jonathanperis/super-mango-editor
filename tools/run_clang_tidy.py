#!/usr/bin/env python3
"""Run clang-tidy over the project's own C files and summarise what it finds.

  make tidy                      (writes compile_commands.json first)
  python3 tools/run_clang_tidy.py [--summary FILE] [--strict]

The checks come from the repository's .clang-tidy. Files are taken from
compile_commands.json; only those under src/ and tools/ are analysed, and
only findings in those folders are reported, so vendor/ (tomlc17) and
raylib's headers under out/ never show up. A header included by many files
is reported once.

By default the script prints the findings and exits 0: the CI job is
informational. --strict exits 1 when there is any finding. --summary appends
a Markdown table to FILE (CI passes $GITHUB_STEP_SUMMARY). CLANG_TIDY names
the program (default: clang-tidy).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT_DIRS = ("src", "tools")

# path:line:col: warning: message [check-name]
# The path may start with a Windows drive letter ("C:/..."), whose colon is
# part of the path and not the separator before the line number.
FINDING_RE = re.compile(r"^(?P<path>(?:[A-Za-z]:)?[^\s:][^:]*):(?P<line>\d+):(?P<col>\d+): "
                        r"(?P<level>warning|error): (?P<message>.*?) \[(?P<check>[\w.,-]+)\]$")


def project_relative(path: str) -> str | None:
    """The path relative to the repository when it is project code, else None."""
    try:
        rel = Path(path).resolve().relative_to(ROOT)
    except ValueError:
        return None
    return rel.as_posix() if rel.parts and rel.parts[0] in PROJECT_DIRS else None


def parse_findings(output: str) -> list[tuple[str, int, int, str, str, str]]:
    """(file, line, column, level, check, message) for each project finding."""
    findings = []
    for line in output.splitlines():
        match = FINDING_RE.match(line.strip())
        if not match:
            continue
        rel = project_relative(match["path"])
        if rel is None:
            continue
        findings.append((rel, int(match["line"]), int(match["col"]),
                         match["level"], match["check"], match["message"]))
    return findings


def sources_to_check(database: Path) -> list[str]:
    entries = json.loads(database.read_text(encoding="utf-8"))
    return sorted({entry["file"] for entry in entries if project_relative(entry["file"])})


def run_one(tidy: str, source: str) -> str:
    result = subprocess.run([tidy, "--quiet", "-p", str(ROOT), source],
                            capture_output=True, text=True, check=False)
    return result.stdout + result.stderr


def markdown_summary(findings: list[tuple[str, int, int, str, str, str]], files: int) -> str:
    lines = ["### clang-tidy (informational)", ""]
    if not findings:
        lines.append(f"No findings in {files} files under src/ and tools/.")
        return "\n".join(lines) + "\n"
    lines.append(f"{len(findings)} findings in {files} files under src/ and tools/.")
    lines += ["", "| File | Line | Check | Message |", "| --- | --- | --- | --- |"]
    for rel, line, _col, _level, check, message in findings:
        cell = message.replace("|", "\\|")  # a | would end the table cell
        lines.append(f"| `{rel}` | {line} | `{check}` | {cell} |")
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--summary", type=Path, help="append a Markdown table to this file")
    parser.add_argument("--strict", action="store_true", help="exit 1 when anything is found")
    args = parser.parse_args(argv)

    database = ROOT / "compile_commands.json"
    if not database.is_file():
        sys.stderr.write("run_clang_tidy: compile_commands.json is missing; run `make compile-commands`\n")
        return 2
    tidy = os.environ.get("CLANG_TIDY", "clang-tidy")
    sources = sources_to_check(database)

    # One clang-tidy process per file, a few at a time; each is single-threaded.
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as pool:
        outputs = list(pool.map(lambda source: run_one(tidy, source), sources))

    findings = sorted(set(f for output in outputs for f in parse_findings(output)))
    for rel, line, col, level, check, message in findings:
        print(f"{rel}:{line}:{col}: {level}: {message} [{check}]")
    print(f"run_clang_tidy: {len(findings)} findings in {len(sources)} files")
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as out:
            out.write(markdown_summary(findings, len(sources)))
    return 1 if args.strict and findings else 0


if __name__ == "__main__":
    sys.exit(main())
