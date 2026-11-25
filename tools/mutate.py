#!/usr/bin/env python3
"""Mutation testing: change the code a little and see whether a test notices.

  python3 tools/mutate.py [--target GLOB]... [--tests PROGRAM]... [--max N]
      [--seed N] [--outdir out/mutate] [--cc clang] [--report FILE] [--list]

Coverage says which lines the tests run. It cannot say whether a test would
fail if one of those lines were wrong: a line can run a thousand times under
a test that never looks at its result. Mutation testing asks that question
directly. It makes one small change (a "mutant") at a time, of the kind a
real slip would make, rebuilds, and runs the tests:

  <  <=  >  >=    flipped to their neighbour (an off-by-one boundary)
  ==  !=          swapped
  &&  ||          swapped
  +  -            swapped (binary only, also += and -=)
  integer N       replaced by N + 1

A mutant is *killed* when some test fails (or crashes, or hangs past the
timeout) and *survives* when every test still passes. A survivor is either
a gap in the tests, worth a new check, or an "equivalent mutant" whose change
the program cannot observe (`i < n` and `i != n` in a loop that counts up by
one). Only a person can tell which, so the report lists every survivor with
its line. A mutant that does not compile is counted apart: it says nothing
about the tests.

The kill rate is killed / (killed + survived). Every mutant rebuilds a few
objects and reruns the test programs, so a whole file takes minutes; --max
picks that many mutants at random (with --seed, the same ones every time).

By default the targets are src/collision/*.c and the serializer's parsing
code (src/shared/serializer_parse*.c), each with the test programs that
exercise it. The build uses raylib's Memory backend in --outdir, so it needs
no window and does not disturb an ordinary build. The source file is always
put back, even after Ctrl+C; run `git status` if in doubt.

`make mutate` runs it with the defaults; `make mutate MUTATE_ARGS="--max 50"`
passes options through. The weekly Mutation workflow (mutation.yml) runs it
and publishes the report; it is informational and never blocks a merge.
"""

from __future__ import annotations

import argparse
import fnmatch
import glob
import os
import random
import re
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Which test programs exercise which files (Makefile names, without OUTDIR).
# The first matching pattern wins.
DEFAULT_TARGETS = ["src/collision/*.c", "src/shared/serializer_parse*.c"]
TESTS_FOR = [
    ("src/collision/collision_damage.c",
     ["collision-test", "gameplay-damage-test", "gameplay-mechanics-test", "session-test"]),
    ("src/collision/*.c", ["collision-test", "gameplay-mechanics-test", "session-test"]),
    ("src/shared/serializer*.c",
     ["level-serializer-test", "level-validate-test", "runtime-load-test", "session-test"]),
]
FALLBACK_TESTS = ["level-serializer-test", "collision-test", "gameplay-mechanics-test",
                  "session-test"]


@dataclass
class Mutant:
    path: str        # repository-relative file
    offset: int      # byte offset of the replaced text
    old: str
    new: str
    line: int
    operator: str

    def describe(self) -> str:
        return f"{self.path}:{self.line}: {self.operator}: `{self.old}` -> `{self.new}`"


def code_mask(text: str) -> str:
    """The text with comments, string and character literals and preprocessor
    lines blanked out (same length), so operators are only found in code."""
    out = list(text)
    i, n = 0, len(text)
    line_start = True
    while i < n:
        c = text[i]
        if line_start and c == "#":
            # A preprocessor line, with its backslash continuations.
            while i < n and text[i] != "\n":
                if text[i] == "\\" and i + 1 < n and text[i + 1] == "\n":
                    out[i] = " "
                    i += 2
                    continue
                out[i] = " "
                i += 1
            continue
        if c == "\n":
            line_start = True
            i += 1
            continue
        if c in " \t":
            i += 1
            continue
        line_start = False
        if text.startswith("//", i):
            while i < n and text[i] != "\n":
                out[i] = " "
                i += 1
        elif text.startswith("/*", i):
            end = text.find("*/", i + 2)
            end = n if end < 0 else end + 2
            for j in range(i, end):
                if text[j] != "\n":
                    out[j] = " "
            i = end
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            for k in range(i, min(j + 1, n)):
                if text[k] != "\n":
                    out[k] = " "
            i = j + 1
        else:
            i += 1
    return "".join(out)


RELATIONAL = re.compile(r"<=|>=|==|!=|&&|\|\||<<=?|>>=?|->|<|>")
INTEGER = re.compile(r"(?<![\w.])(\d+)(?![\w.])")


def find_mutants(path: str) -> list[Mutant]:
    text = (ROOT / path).read_text(encoding="utf-8")
    code = code_mask(text)
    found: list[Mutant] = []

    def line_of(offset: int) -> int:
        return text.count("\n", 0, offset) + 1

    swaps = {"<": ("<=", "boundary"), "<=": ("<", "boundary"), ">": (">=", "boundary"),
             ">=": (">", "boundary"), "==": ("!=", "equality"), "!=": ("==", "equality"),
             "&&": ("||", "logic"), "||": ("&&", "logic")}
    for match in RELATIONAL.finditer(code):
        token = match.group(0)
        if token not in swaps:
            continue   # shifts and -> are not mutated
        new, operator = swaps[token]
        found.append(Mutant(path, match.start(), token, new, line_of(match.start()), operator))

    for i, c in enumerate(code):
        if c not in "+-":
            continue
        after = code[i + 1] if i + 1 < len(code) else ""
        before_raw = code[i - 1] if i else ""
        if after in "+-" or before_raw in "+-" or (c == "-" and after == ">"):
            continue   # ++, --, ->
        j = i - 1
        while j >= 0 and code[j] in " \t\n":
            j -= 1
        prev = code[j] if j >= 0 else ""
        if not (prev.isalnum() or prev in "_)]"):
            continue   # unary minus or plus
        if prev in "eE" and j > 0 and (code[j - 1].isdigit() or code[j - 1] == "."):
            continue   # the exponent of 1e-5
        other = "-" if c == "+" else "+"
        found.append(Mutant(path, i, c, other, line_of(i), "arithmetic"))

    for match in INTEGER.finditer(code):
        value = match.group(1)
        if len(value) > 1 and value.startswith("0"):
            continue   # octal; not worth the confusion
        new = str(int(value) + 1)
        found.append(Mutant(path, match.start(1), value, new, line_of(match.start(1)), "constant"))

    found.sort(key=lambda m: m.offset)
    return found


def tests_for(path: str) -> list[str]:
    for pattern, tests in TESTS_FOR:
        if fnmatch.fnmatch(path, pattern):
            return tests
    return FALLBACK_TESTS


class Restorer:
    """Put every touched file back, also on Ctrl+C or a kill."""

    def __init__(self) -> None:
        self.saved: dict[Path, str] = {}

    def save(self, path: Path) -> None:
        self.saved.setdefault(path, path.read_text(encoding="utf-8"))

    def restore(self) -> None:
        for path, text in self.saved.items():
            if path.read_text(encoding="utf-8") != text:
                path.write_text(text, encoding="utf-8")

    def install(self) -> None:
        def handler(signum, _frame):
            self.restore()
            sys.exit(128 + signum)
        signal.signal(signal.SIGINT, handler)
        signal.signal(signal.SIGTERM, handler)


def build(programs: list[str], args) -> tuple[bool, str]:
    targets = [f"{args.outdir}/{p}" for p in programs]
    cmd = ["make", *targets, f"OUTDIR={args.outdir}", "RAYLIB_PLATFORM=memory", f"CC={args.cc}",
           f"-j{args.jobs}"]
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    return result.returncode == 0, result.stdout[-2000:] + result.stderr[-2000:]


def run_tests(programs: list[str], args) -> tuple[bool, str]:
    """(all passed, the name of the first program that did not)."""
    for program in programs:
        try:
            result = subprocess.run([str(ROOT / args.outdir / program)], cwd=ROOT,
                                    capture_output=True, timeout=args.timeout)
        except subprocess.TimeoutExpired:
            return False, f"{program} (timed out)"
        if result.returncode != 0:
            return False, program
    return True, ""


def try_mutant(programs: list[str], args) -> tuple[str, str]:
    """Build and run one program at a time, quickest first, and stop at the
    first failure: most mutants die in the first, small program, and the
    large ones are then never relinked."""
    for index, program in enumerate(programs):
        built, _ = build([program], args)
        if not built:
            # A compile error shows up in the first build; a later one means
            # this program alone does not link, which also says nothing.
            return "no-compile", ""
        passed, detail = run_tests([program], args)
        if not passed:
            return "killed", detail
    return "survived", ""


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--target", action="append", help="source glob (repeatable)")
    parser.add_argument("--tests", action="append", help="test program to run (repeatable); "
                        "default: the programs that exercise each target")
    parser.add_argument("--max", type=int, default=0, help="test at most N mutants (random sample)")
    parser.add_argument("--seed", type=int, default=1, help="sample seed (default 1)")
    parser.add_argument("--outdir", default="out/mutate", help="build folder (default out/mutate)")
    parser.add_argument("--cc", default=os.environ.get("CC", "clang"))
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    parser.add_argument("--timeout", type=float, default=120.0, help="seconds per test program")
    parser.add_argument("--report", type=Path, help="write a Markdown report here")
    parser.add_argument("--list", action="store_true", help="only list the mutants")
    args = parser.parse_args(argv)

    files = sorted({str(Path(f).as_posix()) for pattern in (args.target or DEFAULT_TARGETS)
                    for f in glob.glob(pattern, root_dir=ROOT)})
    if not files:
        print("mutate: no target files")
        return 2
    mutants = [m for f in files for m in find_mutants(f)]
    if args.max and len(mutants) > args.max:
        mutants = sorted(random.Random(args.seed).sample(mutants, args.max),
                         key=lambda m: (m.path, m.offset))
    if args.list:
        for m in mutants:
            print(m.describe())
        print(f"mutate: {len(mutants)} mutants in {len(files)} files")
        return 0

    restorer = Restorer()
    restorer.install()
    programs_of = {f: (args.tests or tests_for(f)) for f in files}
    every_program = sorted({p for f in files for p in programs_of[f]})
    print(f"mutate: baseline build and tests ({', '.join(every_program)})", flush=True)
    ok, log = build(every_program, args)
    if not ok:
        print(log)
        print("mutate: the unmutated code does not build")
        return 2
    ok, which = run_tests(every_program, args)
    if not ok:
        print(f"mutate: {which} fails without any mutation; fix that first")
        return 2

    results: list[tuple[Mutant, str, str]] = []
    started = time.time()
    try:
        for index, mutant in enumerate(mutants, 1):
            path = ROOT / mutant.path
            restorer.save(path)
            original = restorer.saved[path]
            assert original[mutant.offset:mutant.offset + len(mutant.old)] == mutant.old
            path.write_text(original[:mutant.offset] + mutant.new +
                            original[mutant.offset + len(mutant.old):], encoding="utf-8")
            status, detail = try_mutant(programs_of[mutant.path], args)
            path.write_text(original, encoding="utf-8")
            results.append((mutant, status, detail))
            print(f"mutate: [{index}/{len(mutants)}] {status:10} {mutant.describe()}"
                  + (f" ({detail})" if detail else ""), flush=True)
    finally:
        restorer.restore()
        # Rebuild once more so OUTDIR holds the real code again.
        build(every_program, args)

    killed = sum(1 for _, s, _ in results if s == "killed")
    survived = [m for m, s, _ in results if s == "survived"]
    no_compile = sum(1 for _, s, _ in results if s == "no-compile")
    tested = killed + len(survived)
    rate = 100.0 * killed / tested if tested else 0.0
    summary = (f"mutate: {len(results)} mutants, {killed} killed, {len(survived)} survived, "
               f"{no_compile} did not compile; kill rate {rate:.1f}% "
               f"({time.time() - started:.0f} s)")
    print(summary)
    for m in survived:
        print(f"mutate: SURVIVED {m.describe()}")

    if args.report:
        lines = ["## Mutation testing", "", summary.replace("mutate: ", ""), "",
                 "| File | Kill rate | Killed | Survived | No compile |",
                 "| --- | ---: | ---: | ---: | ---: |"]
        for f in files:
            mine = [(m, s) for m, s, _ in results if m.path == f]
            k = sum(1 for _, s in mine if s == "killed")
            sv = sum(1 for _, s in mine if s == "survived")
            nc = sum(1 for _, s in mine if s == "no-compile")
            fr = f"{100.0 * k / (k + sv):.1f}%" if k + sv else "-"
            lines.append(f"| `{f}` | {fr} | {k} | {sv} | {nc} |")
        lines += ["", "### Survivors", "",
                  "Each is a change no test noticed: a missing check, or a change the "
                  "program cannot observe (an equivalent mutant).", ""]
        for m in survived:
            source = (ROOT / m.path).read_text(encoding="utf-8").splitlines()[m.line - 1].strip()
            lines.append(f"- `{m.path}:{m.line}` {m.operator} `{m.old}` -> `{m.new}`: `{source}`")
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"mutate: report written to {args.report}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
