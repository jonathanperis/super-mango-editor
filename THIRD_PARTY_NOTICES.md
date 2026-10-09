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

## Artwork

Artwork is credited to JuhoSprite's Super Mango 2D Pixel Art Platformer Asset Pack:
https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16
Checked 2026-09-16: page metadata lists Creative Commons Attribution 4.0
(https://creativecommons.org/licenses/by/4.0/); its prose also says free use/no
credit needed. This project retains attribution. Some sprites are recolored or
derived for additional biomes; those modifications are recorded in Git history.

## Audio

Every WAV under `assets/sounds/` is original project work, synthesized from
code by `tools/gen_sounds.py` (oscillators, envelopes, seeded noise and simple
filters; no recorded or sampled material). The generated files are covered by
the project's MIT license in `LICENSE`. `make sounds` regenerates them;
`make docs-drift` fails if a committed sound differs from the generator's output
by more than 1 LSB per sample (platform math libraries may round differently).

## Font

All game and editor text uses raylib's built-in default font, compiled into
raylib and covered by raylib's zlib/libpng license above. No font file is
bundled with the game, the editor or their release archives.

## Website fonts

The documentation website (`docs/`, published on GitHub Pages) serves its own
copies of three typefaces, installed from the [Fontsource](https://fontsource.org/)
npm packages and bundled into the site by Astro, so visitors' browsers never
contact Google Fonts:

| Typeface | Package | Copyright | License |
|----------|---------|-----------|---------|
| Pixelify Sans | `@fontsource/pixelify-sans` | Copyright 2021 The Pixelify Sans Project Authors | SIL Open Font License 1.1 |
| Atkinson Hyperlegible | `@fontsource/atkinson-hyperlegible` | Copyright 2020 Braille Institute of America, Inc. | SIL Open Font License 1.1 |
| DM Mono | `@fontsource/dm-mono` | Copyright 2020 The DM Mono Project Authors | SIL Open Font License 1.1 |

The fonts are used unmodified. The full license texts are in
`docs/public/licenses/fonts/`, which the site publishes beside the font files
(`/licenses/fonts/`), and in each package's `LICENSE`.

The documentation's Asset Provenance page records each component's evidence.
