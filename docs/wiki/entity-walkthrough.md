# Entity Walkthrough: From TOML to a Collectible

Start with [Sandbox School](../learning-path/) labs 1–5. This walkthrough follows
the existing coin end-to-end, then gives the complete integration map for adding
a second coin-like collectible named **Token**. It is an exercise, not an entity
already included in the game. It is also the manual's one checklist for adding
*any* entity: [section 7](#7-entities-that-move-or-hurt) covers the extra steps
for enemies and hazards, and [the checklist](#checklist) sums everything up.

## The shape of an entity module

Every entity module works on a whole array and owns no resources:

```text
level_loader.c         -> copy validated LevelDef placements into the GameState array
<entities>_update      -> move, animate, detect events (one fixed step of dt)
<entities>_render      -> draw every active instance with a borrowed shared texture
<entity>_get_hitbox    -> IntRect used by collision and the debug overlay
```

There is no per-entity `_init` or `_cleanup`. Placement happens in
`level_loader.c` (for example `load_coins()`), and the shared texture slot in
`gs->textures` is loaded and released by `src/core/game_resources.c`. Static
entities need even less: a coin stores only its placement state in `Coin` and
exposes just `coins_render()`, and collection is handled in `src/collision/`.
Only the player has `player_init()` and `player_cleanup()`, because it owns its
sprite.

## 1. Follow one existing coin

Open `levels/labs/01_collision.toml`. Its `[[coins]]` record contains x/y values.

1. `src/shared/serializer_parse.c` declares the schema entry
   `ROOT_TABLE_ARRAY("coins", XY_FIELDS, MAX_COINS)`: each element must be a
   table whose only keys are `x` and `y`, each a finite number (an omitted one
   reads as 0), and the array may not exceed `MAX_COINS`. Unknown fields are
   rejected.
2. `serializer_load_collectibles()` (`src/shared/serializer_load_collectibles.c`)
   copies those values into `LevelDef.coins` with its `LOAD_XY_ARRAY` macro.
3. `level_validate_runtime()` (`src/levels/level_validate.c`) first checks every
   count in `level_validate_counts()`, then checks that each coin lies inside
   the world.
4. `load_coins()` in `src/levels/level_loader.c` converts each immutable
   `CoinPlacement` into an active runtime `Coin`. Only `level_load()` calls it:
   `level_reset()` (a life loss) deliberately leaves collected coins gone, and
   `game_restart_after_game_over()` in `src/collision/collision_damage.c`
   reactivates them for Retry.
5. `game_collide()` in `src/collision/game_collision.c` tests the player's
   hitbox with `rect_intersects()`, deactivates the coin, calls
   `game_award_score(gs, gs->rules.coin_score)` and plays the pickup sound.
6. `coins_render()` in `src/collectibles/coin.c` draws only active entries,
   subtracting camera x at draw time; `src/render/game_render.c` calls it.
7. The editor saves the same `LevelDef` through `src/shared/serializer_save.c`,
   where `write_world_and_collectibles()` emits each `[[coins]]` table.

The parser does **not** live in `level_loader.c`: that module translates an
already validated model into runtime state. The shared serializer belongs to
both applications.

Compare the health stars. Yellow, green and red stars are one module,
`src/collectibles/health_star.c`: one `HealthStar` struct, one
`health_stars_render()` and one `health_star_get_hitbox()`, while the level
file keeps three arrays (`[[star_yellows]]`, `[[star_greens]]`,
`[[star_reds]]`). A single `collect_health_stars()` loop in
`game_collision.c` serves all three colours. Stars award no score, so
`level_reset()` brings them back after every life loss. Coins award score, so
they stay collected until a fresh attempt.

## 2. Define the Token contract

For this exercise, Token is a bounded x/y collectible worth 250 points. It uses
the existing coin texture with a tint and does not restore health. Put the
values in named constants. Pass the tint as the `sprite_draw()` tint argument:
raylib tints one draw call, so the shared coin texture is not recolored. Keep
sprite ownership in `TextureResources`; a Token borrows that texture and must
never free it.

```c
/* src/collectibles/token.h */
#pragma once
#include "../shared/graphics.h"
#define MAX_TOKENS 32
#define TOKEN_SCORE 250
typedef struct { float x, y; int active; } Token;
void tokens_render(const Token *items, int count, Texture2D *texture, int camera_x);
```

Implement `tokens_render()` by following `coins_render()` in
`src/collectibles/coin.c`: skip inactive entries, build a 16×16 destination,
subtract `camera_x` and draw the borrowed texture. The Makefile discovers a
new `.c` in `src/collectibles/` automatically.

## 3. Add data and round-trip support

| File | Required change |
|------|-----------------|
| `src/levels/level.h` | Add `TokenPlacement { float x, y; }`, a bounded placement array and a count to `LevelDef` |
| `src/game.h` | Include the Token header; add the runtime array and count to `GameState` |
| `src/shared/serializer_parse.c` | Register `ROOT_TABLE_ARRAY("tokens", XY_FIELDS, MAX_TOKENS)` |
| `src/shared/serializer_load_collectibles.c` | Add `LOAD_XY_ARRAY("tokens", token_count, MAX_TOKENS, tokens)` |
| `src/shared/serializer_save.c` | In `write_world_and_collectibles()`, emit every token as `[[tokens]]` with x/y through the existing float formatter |
| `src/levels/level_validate.c` | Add `CHECK_COUNT(token_count, MAX_TOKENS)` to `level_validate_counts()` and a world-bounds check per placement to `level_validate_runtime()` |
| `tools/validate_levels.py` | Add `"tokens"` to `COUNT_LIMITS` (it reads `MAX_TOKENS` from the headers) and to the schema table as `XY_FIELDS` |

Keep `format_version = 1` at the document root. Unknown fields must still be
rejected; do not loosen schema validation to make the exercise pass.

## 4. Add runtime behavior

In `level_loader.c`, add a small `load_tokens()` that copies placement
positions and sets `active = 1`, and call it from `level_load()`. Token awards
score, so follow the coin rule: do **not** call it from `level_reset()`, or a
player could farm points and bonus lives by dying on purpose. Reactivate tokens
beside the coins in `game_restart_after_game_over()` so Retry starts a fresh
attempt. Wire `tokens_render()` into `src/render/game_render.c` beside coins.

In `game_collide()`, follow the coin collision loop. On overlap, clear `active`,
call `game_award_score(gs, TOKEN_SCORE)` and optionally play the existing pickup
sound. Use that score helper rather than adding directly: it implements bonus
lives and saturation. Keep the existing early returns after death/respawn so a
stale hitbox cannot collect an item at the previous location.

The simulation advances in fixed 1/60 s steps, so `game_collide()` runs once
per step, not once per rendered frame. Keep pickup logic free of frame timing.

Add Token hitboxes to `draw_collision_boxes()` in `src/core/debug.c`. Prefer
the entity's `_get_hitbox` helper when it has one, so the box you see is the
box collision uses:

```c
for (int i = 0; i < gs->token_count; i++)
    if (gs->tokens[i].active)
        outline((IntRect){(int)gs->tokens[i].x, (int)gs->tokens[i].y, 16, 16},
                cam, (Color){255, 255, 0, 255});
```

A frozen (F2) frame must draw without mutating the Token. Log significant
events with `debug_log()` in the module that owns them, as the coin pickup does
in `game_collision.c`.

If you later give Token its own sprite, add one row to a texture table in
`src/core/game_resources.c`; loading and the reverse-order release then happen
in the shared loops, with no hand-written load/free code:

```c
{ TEX_FIELD(token), "assets/sprites/collectibles/token.png", "token.png" },
```

`s_required_textures` is for sprites every level needs. A sprite only some
levels use goes in `s_optional_textures`, plus a `REQUIRE(token, def->token_count)`
line in `game_resources_require_level_textures()`, so a level that places
Tokens is rejected with the asset path when the file is missing.

## 5. Complete editor integration

Follow the existing `ENT_COIN` cases; each row has a distinct responsibility:

| File | Integration |
|------|-------------|
| `editor.h` | Add `ENT_TOKEN` to `EntityType` before `ENT_COUNT` |
| `entity_meta.c` | Add a row to the `s_entity_meta` table (names, category, singleton flag, `MAX_TOKENS` capacity), a slot in `s_palette_order`, and a case in `editor_entity_array()`; read/write/insert/remove, counts and capacity checks then work through the shared helpers |
| `hit_test.c` | Add a bounds case to `editor_entity_bounds()` and put `ENT_TOKEN` into `s_hit_order`, which is the exact reverse of the canvas draw order (a `_Static_assert` fails the build if a type is missing) |
| `canvas.c` | A render call in `canvas_render()` and a placement-ghost case |
| `tools.c` | Clamp case in `editor_clamp_placement()`, move case in `move_placement()` and defaults in `default_placement()` |
| `properties.c` | A `draw_token_properties()` function with x/y fields that call `editor_commit_change()`, plus its case in `properties_render()` |
| `undo.h` | Add `TokenPlacement token;` to the `PlacementData` union |
| `editor_clipboard.c` | Add the `offset_pasted_copy()` case; the shared `editor_add_placement()` already refuses a full array |
| `editor_session.c` | Add `EDITOR_HASH_ARRAY` for tokens to the document hash so the dirty marker sees Token edits |
| `editor_chrome.c` | Include `token_count` in the status-bar entity total |

The palette lists types from `entity_meta.c`, so `palette.c` needs no change.
Undo needs no new code in `editor_undo_apply.c`: commands are built with
`undo_push(stack, const Command *)` and store `PlacementData` snapshots that
are applied through the shared `editor_entity_*` helpers.

These editor paths all operate on `LevelDef`, not live runtime objects.
Copy/paste and undo must preserve selection indices after array
insertion/removal. Do not change fields directly from widgets without the
established change-tracking callbacks, or a visually successful edit may
disappear from undo history.

## 6. Prove the whole path

Create an editor copy of the collision lab, place two Tokens, edit one, save,
reopen, playtest and collect them. Undo/redo placement, deletion and a property
edit; save and verify that returning to the save point clears the dirty marker.

Extend the rich serializer fixture (`fill_rich_roundtrip_fixture()` in
`tests/level_serializer_test.c`) with Tokens and extend
`compare_rich_roundtrip()` to prove the round-trip.
`tests/editor_validation_test.c` already loops over every palette type for
storage and placement, so it covers `ENT_TOKEN` once it is in the palette.
A gameplay case should prove that one pickup awards 250 points exactly once,
and `tests/runtime_load_test.c` should prove a life-loss `level_reset()` keeps
collected Tokens gone, as it does for coins. Reuse the count/bounds fixtures
for invalid data.

```sh
make builder test CC=clang
make validate-levels
make level-catalog content-inventory
make docs-drift
make sanitize CC=clang
```

Add `tokens` to the Collectibles count group in
`tools/generate_level_catalog.py`, then update the source map and relevant
manual references. You are finished when Token works through file load,
runtime, editor, undo, clipboard, validation and save/load, not merely when
the sprite appears on screen.

## 7. Entities that move or hurt

A Token sits still. An enemy or hazard adds three things:

- **An update function.** Write `tokens_update(Token *items, int count, float dt)`
  over the whole array and call it from the focused helper in `src/core/`:
  `game_actors.c` updates enemies (it calls `spiders_update()`), `game_hazards.c`
  updates hazards (it calls `circular_saws_update()`). `dt` is always the fixed
  1/60 s step; see [Developer Guide](../developer-guide/#adding-physics-to-an-entity)
  for the physics pattern.
- **Damage.** Add one `COLLIDE_DAMAGE` (or `COLLIDE_DAMAGE_ACTIVE`) line in
  `game_collide()` (`src/collision/game_collision.c`) with your `_get_hitbox`
  helper, beside the saw's. It calls `apply_damage()` in
  `src/collision/collision_damage.c`, which handles hearts, knockback and the
  hurt-immunity timer.
- **Render order.** Put the render call in `src/render/game_render.c` at the
  right depth; the [render order](../architecture/#render-order-back-to-front)
  table lists every layer.

New `.c` files in an existing `src/` subdirectory are picked up by the Makefile
automatically. A brand-new source directory needs its own wildcard line there.

## Checklist

- [ ] `src/<category>/<entity>.h` and `.c` with render (and update) functions over the whole array, plus a `_get_hitbox` helper if it collides
- [ ] Placement struct, array and count in `LevelDef` (`src/levels/level.h`); runtime array and count in `GameState` (`src/game.h`), stored by value
- [ ] Schema entry, loader, saver and C/Python validation (section 3)
- [ ] `load_<entities>()` in `level_loader.c`, called from `level_load()` and, if it does not award score, from `level_reset()` too
- [ ] Update call in the matching `src/core/` helper; pickup or damage in `src/collision/`
- [ ] Render call in `src/render/game_render.c` at the right layer
- [ ] Texture row in `game_resources.c` (and `game_resources_require_level_textures()` if optional)
- [ ] Hitbox in `src/core/debug.c` and `debug_log()` calls for significant events
- [ ] Editor integration (section 5): metadata, hit test, canvas, tools, properties, undo union, clipboard, document hash, status bar
- [ ] Tests (section 6), then `make builder test CC=clang`, `make validate-levels`, `make docs-drift`
- [ ] Play it with `make run-level-debug LEVEL=...` and check the hitboxes match the sprite
