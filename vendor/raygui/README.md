# Vendored raygui icons

- Upstream: <https://github.com/raysan5/raygui>
- Release: [5.0](https://github.com/raysan5/raygui/releases/tag/5.0),
  published 2026-07-20.
- Commit: `020a61bebcbe288b4414de3416e219ef40af847a`.
- Source file: `src/raygui.h`; `manifest.json` pins its URL and SHA-256.
- License: zlib; see [LICENSE](LICENSE).

Only icon bitmaps are imported. raygui's widgets are not used: the editor
keeps its own `src/shared/ui.c`, which has text selection, Tab focus, undo
hooks and headless tests that raygui does not offer.

`raygui_icons.h` is generated, not edited. `tools/import_raygui_icons.py`
downloads the pinned `raygui.h` (or reads a copy given with `--source`),
checks its SHA-256, and copies the 16 x 16 bitmaps of the icons listed in its
`ICONS` table under the editor's own names (`UI_ICON_SELECT`, `UI_ICON_SAVE`
and so on). The bits are unchanged: `ui_draw_icon` (`src/shared/ui.c`) reads
them the way raygui's `GuiDrawIcon` does, and `editor_ui_test` checks every
icon pixel by pixel.

## When to run the importer

Nothing runs it automatically: the generated header is checked in, so a
normal build never downloads raygui. Run it by hand in two cases.

- **Adding an icon.** Append the pair (raygui's name, the editor's name) to
  `ICONS` in `tools/import_raygui_icons.py`, run `make raygui-icons`, and use
  the new `UI_ICON_*` name in the editor.
- **Updating raygui.** Change `manifest.json` (commit, URL and SHA-256), then
  run `make raygui-icons`.

`make raygui-icons` downloads the pinned `raygui.h` and refuses it when the
SHA-256 differs. To use a copy you already have (no network), pass its path:
`make raygui-icons RAYGUI_SOURCE=/path/to/raygui.h`; the checksum is still
checked. Commit the regenerated `raygui_icons.h` with the change that needed
it.

`tests/import_raygui_icons_test.py` (part of `make test`) feeds the importer
a small stand-in `raygui.h` and checks that each icon's bits land under the
right name. It also fails when the checked-in header no longer matches the
`ICONS` table or the pinned commit, which is the reminder to rerun the tool.
