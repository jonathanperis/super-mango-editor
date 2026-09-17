# Asset Provenance

Code and media do not automatically share a license. Release archives include
the project MIT license, `THIRD_PARTY_NOTICES.md` and the vendored parser's full
MIT notice. See [Asset Inventory](../asset-inventory/) for generated file sizes.

| Component | Source / evidence | Status |
|-----------|-------------------|--------|
| Project C/Python/JS | Repository `LICENSE` | MIT, Jonathan Peris |
| tomlc17 | [Upstream R260821](https://github.com/cktan/tomlc17/releases/tag/R260821); `vendor/tomlc17/LICENSE` | MIT, CK Tan; `vendor/tomlc17/README.md` records the exact upstream commit and retained project-patch inventory |
| Original sprites | [JuhoSprite's Super Mango pack](https://juhosprite.itch.io/super-mango-2d-pixelart-platformer-asset-pack16x16), checked 2026-09-16 | Page metadata lists CC BY 4.0; page prose also says free use/no credit needed. Preserve attribution and the license link |
| Recolored/derived sprites | `tools/gen_fire_sprites.py`, `tools/analyze_sprite.py`, repository history | Derived from the original sprites; retain JuhoSprite credit and identify modifications |
| `round9x13.ttf` | Pack page identifies Round 9x13 and explicitly says the font was not made by JuhoSprite | Author/license evidence remains unresolved in this checkout |
| WAV audio | `assets/sounds/` | Original source/license records remain unresolved in this checkout |
| SDL libraries | SDL2 and companion libraries; installed platform packages | zlib-licensed SDL code; transitive dependency notices are supplied by their packages |

The pack metadata links to [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
This project modifies colors and organization for its biomes. The source page's
font caveat means its sprite attribution cannot establish the font's license.
Resolve missing audio/font records from the original acquisition sources before
claiming a complete media-license inventory. Do not relabel them MIT.

Windows builder archives copy the MSYS2 environment's available package notices
under `licenses/msys2/`. macOS/Linux use separately installed runtime libraries.
Reserve assets remain in the checkout for learning, but `unused/` files are not
included in game/editor release bundles or WebAssembly preload data.
