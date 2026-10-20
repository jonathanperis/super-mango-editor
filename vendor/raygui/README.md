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

To add an icon, append it to `ICONS`, rerun the tool and use the new
`UI_ICON_*` name. To update raygui, change `manifest.json` (commit, URL and
SHA-256) and rerun the tool.
