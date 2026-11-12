#!/usr/bin/env python3
"""tools/check_roadmap_quality.py passes on this repository and catches breakage.

Each check appends its messages to the module's FAILURES list. A case runs
one check with one file's text replaced, through the same read() function
the checks use, and expects a message that names the problem.
"""

from __future__ import annotations

import sys
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import check_roadmap_quality as quality  # noqa: E402

REAL_READ = quality.read


def failures(check, replacements: dict[str, tuple[str, str]] | None = None) -> list[str]:
    """Messages from one check, with (old, new) text swapped in the named files."""
    replacements = replacements or {}

    def read(path: Path) -> str:
        text = REAL_READ(path)
        rel = path.relative_to(ROOT).as_posix()
        if rel in replacements:
            old, new = replacements[rel]
            assert old in text, f"test is stale: {old!r} is not in {rel}"
            text = text.replace(old, new)
        return text

    quality.FAILURES.clear()
    with patch.object(quality, "read", read):
        check()
    found, quality.FAILURES[:] = list(quality.FAILURES), []
    return found


def expect(found: list[str], needle: str) -> None:
    assert any(needle in message for message in found), f"no failure mentions {needle!r}: {found}"


def main() -> None:
    checks = (quality.check_screen_word_parser, quality.check_level_line_endings,
              quality.check_overlay_snapshot_doc, quality.check_release_and_wasm_guardrails,
              quality.check_scripted_smoke_target)
    for check in checks:
        assert failures(check) == [], f"{check.__name__}: {failures(check)}"

    build, docs = ".github/workflows/build.yml", ".github/workflows/docs.yml"
    checklist, manual = "docs/wiki/release-checklist.md", "docs/wiki/build-system.md"
    guard = quality.check_release_and_wasm_guardrails
    expect(failures(guard, {build: ("tools/check_wasm_artifacts.py", "tools/skip.py")}),
           "WebAssembly job must run tools/check_wasm_artifacts.py")
    expect(failures(guard, {build: ("docs/out/super-mango.wasm", "docs/out/x.wasm")}),
           "missing Pages WASM smoke for `docs/out/super-mango.wasm`")
    expect(failures(guard, {docs: ("tools/check_wasm_artifacts.py", "tools/x.py")}),
           "docs path filter must include tools/check_wasm_artifacts.py")
    expect(failures(guard, {checklist: ("make dist-wasm", "make dist-web")}), "missing `make dist-wasm`")
    expect(failures(guard, {checklist: ("WebAssembly.compile", "WebAssembly.check")}),
           "missing `WebAssembly.compile`")

    smoke = quality.check_scripted_smoke_target
    expect(failures(smoke, {"Makefile": ("scripted-smoke:", "scripted-smok:")}), "Makefile target")
    expect(failures(smoke, {build: ("make scripted-smoke", "make smoke-scripted")}),
           "CI must run scripted smoke on Linux")
    expect(failures(smoke, {manual: ("--replay-script", "--replay-file")}), "missing replay-script smoke docs")
    expect(failures(smoke, {docs: ("tools/run_scripted_smoke.py", "tools/x.py")}),
           "docs drift path filter must include `tools/run_scripted_smoke.py`")
    expect(failures(smoke, {"src/input/game_replay.c": ("input_push", "input_drop")}),
           "replay smoke must inject semantic commands")

    expect(failures(quality.check_level_line_endings,
                    {".gitattributes": ("levels/**/*.toml text eol=lf", "levels/**/*.toml text")}),
           "must enforce LF checkouts")
    # The old top-folder-only pattern is not enough any more.
    expect(failures(quality.check_level_line_endings,
                    {".gitattributes": ("levels/**/*.toml text eol=lf", "levels/*.toml text eol=lf")}),
           "must enforce LF checkouts")
    # A CRLF file in a subfolder is found, not only in levels/ itself.
    for rel in ("levels/labs/02_moving_platforms.toml", "levels/campaigns/main.toml"):
        target = ROOT / rel
        assert target.is_file(), f"test is stale: {rel} does not exist"
        real = quality.read_bytes

        def crlf(path: Path, target=target, real=real) -> bytes:
            data = real(path)
            return data.replace(b"\n", b"\r\n") if path == target else data

        with patch.object(quality, "read_bytes", crlf):
            expect(failures(quality.check_level_line_endings), f"{rel}: normalize level TOML line endings")
    expect(failures(quality.check_overlay_snapshot_doc,
                    {"docs/wiki/overlay-snapshots.md": ("tools/generate_overlay_snapshots.py", "a script")}),
           "missing generated-file source note")

    # main() reports every failure and exits non-zero.
    with patch.object(quality, "check_level_line_endings", lambda: quality.fail("planted failure")), \
            patch("builtins.print") as printed:
        assert quality.main() == 1
    assert any("planted failure" in str(call) for call in printed.call_args_list)
    quality.FAILURES.clear()
    with patch("builtins.print"):
        assert quality.main() == 0
    print("roadmap_quality_test: ok")


if __name__ == "__main__":
    main()
