# Third-party notices

Super Mango source is MIT-licensed; see `LICENSE`. That license does not replace
the licenses of third-party code or media.

## tomlc17

Copyright (c) 2024–2026 CK Tan. MIT license.
Source: https://github.com/cktan/tomlc17

The complete notice is in `vendor/tomlc17/LICENSE` in the source checkout and
`licenses/tomlc17.txt` in release archives. The vendored copy is based on upstream
**R260821** with retained project hardening. `vendor/tomlc17/README.md` records the
exact upstream commit and local patch inventory.

## raylib and native dependencies

raylib 6.0 is built from the source/checksum pin in `vendor/raylib/manifest.json`.
Source: https://github.com/raysan5/raylib/releases/tag/6.0
Copyright (c) 2013–2026 Ramon Santamaria (@raysan5). zlib/libpng license.

Native builds statically link raylib with bundled GLFW; browser builds use the
Web backend. Archives include `licenses/raylib.txt`, GLFW's notice and the
license-bearing external files under `licenses/raylib-dependencies/`.
Those bundled components have their own terms, retained in those files.
`vendor/raylib/README.md` documents the marked raylib/miniaudio source fixes.
Native OS graphics/audio libraries remain required. Windows packaging resolves
both executables' non-system runtime dependencies and includes their owning
MSYS2 packages' available license files under `licenses/msys2/`.

## Artwork, audio and font

Artwork is credited to JuhoSprite's Super Mango 2D Pixel Art Platformer Asset Pack:
https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16
Checked 2026-09-16: page metadata lists Creative Commons Attribution 4.0
(https://creativecommons.org/licenses/by/4.0/); its prose also says free use/no
credit needed. This project retains attribution. Some sprites are recolored or
derived for additional biomes; those modifications are recorded in Git history.

The pack author explicitly excludes authorship of the Round 9x13 font. This
checkout lacks original audio-source/license records and a license for
`round9x13.ttf`; those permissions remain unresolved. Do not assume the code's
MIT license covers audio or the font.

The documentation's Asset Provenance page records these evidence gaps. No new
license or ownership claim is made for existing media.
