# Entity Walkthrough: From TOML to a Collectible

Start with [Learning Path](../learning-path/) labs 1–5. This walkthrough follows
the existing coin end-to-end, then gives the complete integration map for adding
a second coin-like collectible named **Token**. It is an exercise, not an entity
already included in the game.

## 1. Follow one existing coin

Open `levels/labs/01_collision.toml`. Its `[[coins]]` record contains x/y values.

1. `src/shared/serializer_parse.c` checks the key, element types and maximum count.
2. `serializer_load_collectibles()` copies those values into `LevelDef.coins`.
3. `level_validate_runtime()` checks placement bounds.
4. `src/levels/level_loader.c` converts each immutable `CoinPlacement` into an active runtime `Coin`.
5. `game_collide()` tests the player's hitbox, deactivates the coin and calls `game_award_score()`.
6. `coins_render()` draws only active entries, subtracting camera x at draw time.
7. The editor saves the same `LevelDef` through `src/shared/serializer_save.c`.

The parser does **not** live in `level_loader.c`: that module translates an
already validated model into runtime state. The shared serializer belongs to
both applications.

## 2. Define the Token contract

For this exercise, Token is a bounded x/y collectible worth 250 points, uses a
tinted existing coin texture, and does not restore health. Put the values in
named constants. Texture tint must be restored after drawing so shared coins
are not recolored. Keep sprite ownership in `TextureResources`; a Token borrows
that texture and must never free it.

```c
/* src/collectibles/token.h */
#pragma once
#include <SDL.h>
#define MAX_TOKENS 32
#define TOKEN_SCORE 250
typedef struct { float x, y; int active; } Token;
void tokens_render(const Token *items, int count, SDL_Renderer *renderer,
                   SDL_Texture *texture, int camera_x);
```

Implement `tokens_render()` by following `coins_render()` in
`src/collectibles/coin.c`: skip inactive entries, build a 16×16 destination,
subtract `camera_x` and render the borrowed texture. The Makefile discovers a
new `.c` in `src/collectibles/` automatically.

## 3. Add data and round-trip support

| File | Required change |
|------|-----------------|
| `src/levels/level.h` | Add `TokenPlacement { float x, y; }`, bounded placement array and count to `LevelDef` |
| `src/game.h` | Include the Token header; add runtime array and count to `GameState` |
| `src/shared/serializer_parse.c` | Register `tokens` as an array of XY tables bounded by `MAX_TOKENS` |
| `src/shared/serializer_load_collectibles.c` | Add `LOAD_XY_ARRAY("tokens", token_count, MAX_TOKENS, tokens)` |
| `src/shared/serializer_save.c` | Emit every token as `[[tokens]]` with x/y using the existing float formatter |
| `src/levels/level_validate.c` | Validate the count before indexing and validate each placement's world bounds |
| `tools/validate_levels.py` | Mirror the schema/count/placement contract in the Python validator |

Keep `format_version = 1` at the document root. Unknown fields must still be
rejected; do not loosen schema validation to make the exercise pass.

## 4. Add runtime behavior

In `level_loader.c`, add a small `load_tokens()` that copies placement positions
and sets `active = 1`. Call it from both initial load and death/reset paths.
Wire `tokens_render()` into `src/render/game_render.c` beside coins.

In `game_collide()`, follow the coin collision loop. On overlap, clear `active`,
call `game_award_score(gs, TOKEN_SCORE)` and optionally play the existing pickup
sound. Use that score helper rather than adding directly: it implements bonus
lives and saturation. Keep the existing early returns after death/respawn so a
stale hitbox cannot collect an item at the previous location.

Add Token hitboxes to `src/core/debug.c`. A paused frame must draw without
mutating the Token. If you later supply a distinct sprite, wire its ownership
into `game_resources.c`, require it when used, and release it exactly once.

## 5. Complete editor integration

Follow the existing `ENT_COIN` cases; each row has a distinct responsibility:

| File | Integration |
|------|-------------|
| `editor.h`, `entity_meta.c` | Add `ENT_TOKEN`, display/category metadata, palette ordering, count and singleton handling |
| `tools.c` | Position access, maximum count, hit testing, placement defaults and deletion |
| `canvas.c`, `palette.c` | Actual preview, placement ghost, selection bounds and thumbnail |
| `properties.c` | x/y property controls using staged field commits |
| `undo.h` | Add Token placement storage to `PlacementData` |
| `editor_undo_apply.c` | Capture a Token snapshot; use the existing array operation pattern for undo/redo |
| `editor_clipboard.c` | Copy/paste the new placement and enforce the count limit |
| `editor_session.c` | Include active token placements in the document hash |

These editor paths all operate on `LevelDef`, not live runtime objects. Copy/paste
and undo must preserve selection indices after array insertion/removal. Do not
change fields directly from widgets without the established change-tracking
callbacks, or a visually successful edit may disappear from undo history.

## 6. Prove the whole path

Create an editor copy of the collision lab, place two Tokens, edit one, save,
reopen, playtest and collect them. Undo/redo placement, deletion and a property
edit; save and verify that returning to the save point clears the dirty marker.

Extend the existing rich serializer fixture with Tokens and extend its comparison
to prove the round-trip. Add one editor operation case only if the generic
coverage does not exercise Token. A gameplay case should prove one pickup awards
250 points exactly once. Reuse the count/bounds fixtures for invalid data.

```sh
make builder test CC=clang
make validate-levels
make level-catalog content-inventory
make docs-drift
make sanitize CC=clang
```

Update the collectible count group in `tools/generate_level_catalog.py`, the
source map and relevant manual references. You are finished when Token works
through file load, runtime, editor, undo, clipboard, validation and save/load—not
merely when the sprite appears on screen.
