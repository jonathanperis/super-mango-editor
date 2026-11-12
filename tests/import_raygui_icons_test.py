#!/usr/bin/env python3
"""tools/import_raygui_icons.py copies icon bitmaps out of a raygui.h.

The real raygui.h is not downloaded. A small stand-in with the same two
blocks the tool reads (the icon name enum and the bitmap table) gives every
icon a recognisable pattern, and the generated header must carry exactly
those bits under the editor's names. The header that is checked in must
also still match the tool's icon list and the pinned commit.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import import_raygui_icons as icons  # noqa: E402

PIN = {"version": "9.9", "commit": "c" * 40}


def fake_raygui(names: list[str], blank: str | None = None) -> str:
    """A raygui.h with one icon per name; icon i is eight words of value i+1.

    Ids are spread out (every third number) so a tool that used an icon's
    position in ICONS instead of its raygui id would copy the wrong rows.
    """
    ids = {name: 3 * index + 2 for index, name in enumerate(names)}
    count = max(ids.values()) + 1
    rows = []
    for icon_id in range(count):
        owner = next((name for name, value in ids.items() if value == icon_id), None)
        word = 0 if owner is None or owner == blank else ids[owner] + 1
        rows.append("    " + ", ".join(f"0x{word:08x}" for _ in range(icons.WORDS)) +
                    f",      // {owner or 'ICON_UNUSED'}")
    enum = ",\n".join(f"    {name} = {value}" for name, value in ids.items())
    return ("typedef enum {\n    OTHER_THING = 1\n} Other;\n"
            "typedef enum {\n" + enum + "\n} GuiIconName;\n"
            "static unsigned int guiIcons[RAYGUI_ICON_MAX_ICONS*RAYGUI_ICON_DATA_ELEMENTS] = {\n"
            + "\n".join(rows) + "\n};\n")


def main() -> None:
    theirs = [name for name, _ in icons.ICONS]
    source = fake_raygui(theirs)
    ids = icons.icon_ids(source)
    assert ids == {name: 3 * index + 2 for index, name in enumerate(theirs)}
    assert "OTHER_THING" not in ids, "read a name from another enum"
    assert len(icons.icon_words(source)) == (max(ids.values()) + 1) * icons.WORDS

    header = icons.render(source, PIN)
    assert header.endswith("};\n") and "#pragma once" in header
    assert "raygui 9.9" in header and "c" * 40 in header, "the header does not say where it came from"
    # The enum lists the editor's names in ICONS order, after UI_ICON_NONE.
    names = re.findall(r"^    (UI_ICON_\w+)", header, re.M)
    assert names == ["UI_ICON_NONE"] + [f"UI_ICON_{ours}" for _, ours in icons.ICONS] + ["UI_ICON_COUNT"]
    # Row 0 is the empty icon; every other row is that icon's own eight words.
    rows = re.findall(r"^    \{(.*)\},(?: /\* (\w+) \*/)?$", header, re.M)
    assert rows[0] == ("0", ""), rows[0]
    assert len(rows) == len(icons.ICONS) + 1
    for (theirs_name, ours), (words, label) in zip(icons.ICONS, rows[1:]):
        expected = ", ".join([f"0x{ids[theirs_name] + 1:08x}"] * icons.WORDS)
        assert label == ours and words == expected, f"{ours}: wrong bitmap copied"

    # An icon with no pixels means the table and the names no longer line up.
    try:
        icons.render(fake_raygui(theirs, blank=theirs[3]), PIN)
    except SystemExit as error:
        assert theirs[3] in str(error)
    else:
        raise AssertionError("accepted an icon without pixels")

    # The checked-in header was generated from this ICONS list and this pin.
    committed = (ROOT / "vendor/raygui/raygui_icons.h").read_text(encoding="utf-8")
    pin = json.loads((ROOT / "vendor/raygui/manifest.json").read_text(encoding="utf-8"))
    assert pin["commit"] in committed and pin["commit"] in pin["url"]
    assert re.findall(r"^    (UI_ICON_\w+)", committed, re.M) == names, \
        "vendor/raygui/raygui_icons.h is stale: run `make raygui-icons`"
    for theirs_name, ours in icons.ICONS:
        assert re.search(rf"UI_ICON_{ours},\s*/\* raygui {theirs_name} \(\d+\) \*/", committed), ours
    print("import_raygui_icons_test: ok")


if __name__ == "__main__":
    main()
