---
title: C in This Codebase
description: Each C idea the game relies on, mapped to the file and function where you can read it.
---

# C in This Codebase

A C textbook explains `offsetof` or `goto` on a ten-line example. This page
points at where Super Mango actually uses each idea, so you can read it in a
real program and see why it was the right tool there. Open the file next to
this page; the comments in the code explain the details.

On this page:

- [Pointers and ownership](#pointers-and-ownership)
- [Structs by value and fixed-size arrays](#structs-by-value-and-fixed-size-arrays)
- [Tables instead of repeated code](#tables-instead-of-repeated-code)
- [Checks the compiler does for us](#checks-the-compiler-does-for-us)
- [Cleaning up after a failure](#cleaning-up-after-a-failure)
- [Integers that must not overflow](#integers-that-must-not-overflow)
- [Files that are never half written](#files-that-are-never-half-written)

## Pointers and ownership

C has no garbage collector, so every allocation needs exactly one owner: the
piece of code that will free it. Everything else only *borrows* the pointer.

| Idea | Where to read it | What to notice |
|------|------------------|----------------|
| An owning pointer | `texture_load()` and `texture_unload()` in `src/shared/graphics.h` | Load mallocs a small `Texture2D` handle and returns `NULL` on failure; unload frees it. Neither function can clear the caller's variable, which is why the next row exists. |
| Clearing a pointer after the free | `DESTROY_TEX` and `FREE_CHUNK` in `src/game.h` | `do { texture_unload(tex); (tex) = NULL; } while (0)`: the owner frees, then forgets the address, so a second cleanup is harmless. |
| Release in reverse order | `destroy_texture_specs_reverse()` in `src/core/game_resources.c`; `game_cleanup()` in `src/core/game_lifecycle.c` | Things are freed in the opposite order they were created, so nothing is released while something else still points into it. |
| Borrowed pointers | `coins_render()` in `src/collectibles/coin.c` | The renderer receives `Texture2D *` but never frees it; `TextureResources` in `GameState` owns it. |
| Handing ownership over | `undo_group_begin()` and `undo_push()` in `src/editor/undo.c` | Begin mallocs room for a group's entries and keeps the pointer in `reserve`. The group's first push hands that pointer to its step and sets `reserve` to `NULL`, so exactly one owner frees it: the step when it is dropped, or `undo_group_end()` when the group recorded nothing. |
| A pointer to a struct, read-only | `undo_push(UndoStack *stack, const Command *cmd)` | A `Command` is about 13 KB. Passing `const Command *` avoids copying it and promises the function only reads it. |

The [Debugging C](../debugging-c/) page shows what goes wrong when these
rules are broken: a use-after-free caught by AddressSanitizer, and the null
dereference you get once the owner clears its pointer.

## Structs by value and fixed-size arrays

| Idea | Where to read it | What to notice |
|------|------------------|----------------|
| One big struct that holds everything | `GameState` in `src/game.h` | `Player player;` and `Coin coins[MAX_COINS];` are stored by value inside its `world` part (`GameWorld`), and that part by value inside `GameState`, not as separate allocations. The parts only group fields: `gs->world.coins[i]` is still one struct member reached through another. One `calloc` in `session_make_game()` (`src/core/app_session.c`) creates all of it, already zeroed. |
| Fixed limits instead of resizing | `MAX_COINS` in `src/collectibles/coin.h`; `MAX_FLOOR_GAPS` and friends in `src/game_constants.h` | A level can never need more memory than the limits allow, so there is no `realloc` during play and nothing to free per entity. |
| Checking the count before copying | `level_validate_counts()` in `src/levels/level_validate.c`; `LOAD_XY_ARRAY` in `src/shared/serializer_load_collectibles.c` | A count larger than the array is rejected before the copy loop runs. A fixed array is only safe if every writer checks this. |
| Immutable data versus live state | `CoinPlacement` in `LevelDef` versus `Coin` in `GameState`; `load_coins()` in `src/levels/level_loader.c` | The level file's data stays untouched; `load_coins()` copies it into the runtime array and sets `active = 1`. A restart copies again instead of "undoing" changes. |
| Designated initializers | `main()` in `src/main.c` (`AppSessionConfig config = {.profile_enabled = 1};`) | Named fields are set and every other member starts at zero. |
| A union for "one of several kinds" | `PlacementData` in `src/editor/undo.h` | An undo entry stores whichever placement type was edited, in the space of the largest one. `entity_type` says which member is valid. |

## Tables instead of repeated code

When twenty things differ only in a name and a number, the code puts those
pairs in a table and writes the logic once.

| Idea | Where to read it | What to notice |
|------|------------------|----------------|
| `offsetof` to name a struct field | `TEX_FIELD` and `CHUNK_FIELD` in `src/core/game_resources.c` | Each row of `s_required_textures` stores *where* in `TextureResources` the texture goes, as a byte offset. `texture_slot()` turns the offset back into a `Texture2D **` with `(char *)&gs->assets.textures + offset`. One loop loads every texture, one loop frees them. |
| The same trick for tuning values | the `fields` table and its `FIELD` macro in `src/core/game_inspector.c` | `#name` turns the field name into a string for the inspector's label, and `offsetof(Player, name)` finds the value. |
| Macros that build a schema | `ROOT_TABLE_ARRAY` in `src/shared/serializer_parse.c` | `ROOT_FIELDS` lists every key a level file may contain, with its type and maximum count. The parser walks that table, so adding a key is one line. |
| A macro wrapped in `do { ... } while (0)` | `LOAD_XY_ARRAY` in `src/shared/serializer_load_collectibles.c` | The wrapper makes a multi-line macro behave like one statement, so it is safe after an `if` without braces. |
| Per-type metadata | `s_entity_meta` in `src/editor/entity_meta.c` | The editor's names, categories, capacities, `LevelDef` storage and placement preview for every entity type live in one array indexed by `EntityType`, one row per type written with designated initializers (`[ENT_COIN] = { .type_name = "Coin", ... }`). |
| `offsetof` in a table | `STORED_IN` in `src/editor/entity_meta.c` | A static table cannot point into a `LevelDef` that does not exist yet, so each row stores `offsetof(LevelDef, coins)`; `editor_entity_array()` adds that to a real level's address. `sizeof(((LevelDef *)0)->coins[0])` gets an element's size without evaluating the null pointer. |
| A table of function pointers | `s_property_panels` in `src/editor/properties.c` | Indexed by `EntityType`, it replaces a 30-case `switch`: `properties_render()` looks up the selected type's `draw_<type>_properties` function and calls it. |
| A table walked in order | `s_level_loaders` in `src/levels/level_loader.c`; `s_damage_sources` in `src/collision/game_collision.c` | Every row's function has the same signature, so one `for` loop calls them all, top to bottom. The row order is the run order, and a flag (`LOAD_EVERY_LIFE`) or an `offsetof(GameWorld, spider_count)` column says what differs between rows. |

## Checks the compiler does for us

| Idea | Where to read it | What to notice |
|------|------------------|----------------|
| `_Static_assert` on a table's length | `s_hit_order` in `src/editor/hit_test.c` | `sizeof(s_hit_order) / sizeof(s_hit_order[0]) == ENT_COUNT`: add an entity type and forget this list, and the build fails instead of the editor silently ignoring clicks on it. The same check guards `s_palette_order` and `s_entity_meta` in `entity_meta.c`, `s_property_panels` in `properties.c` and the inspector's `fields` table. |
| Letting the compiler check `printf` formats | `PRINTF_FORMAT` in `src/shared/printf_format.h`, used by `editor_set_status()` | A function taking `const char *fmt, ...` hides its arguments from the compiler; GCC and Clang's `format(printf, ...)` attribute makes `-Wformat` check each call as it does `printf`. |
| `static` at file scope | `s_required_textures` in `game_resources.c`; `state` in `src/core/game_random.c` | `static` outside a function means "visible only in this file". Other files cannot reach the random generator's state; they call `game_random_seed()` and `game_random()`. Helpers like `texture_slot()` are `static` for the same reason. |
| Warnings as a safety net | `-Wall -Wextra -Wpedantic` in the Makefile | The code is kept warning-free; see the [Developer Guide](../developer-guide/). |

## Cleaning up after a failure

| Idea | Where to read it | What to notice |
|------|------------------|----------------|
| `goto` to a single cleanup label | `campaign_manifest_load_entries()` in `src/levels/level_session.c` | Every check after the TOML file is parsed jumps to `done:`, which calls `toml_free(parsed)` once. Without the `goto`, each of the dozen error paths would need its own free, and one would eventually be forgotten. |
| The same pattern at startup | `game_init()` in `src/core/game_lifecycle.c` | Any failed step jumps to `fail:`, which runs `game_cleanup()`. That works because cleanup tolerates a half-built `GameState`: every slot is either `NULL` or owned. |
| Staging before committing | `campaign_catalog_load()` in `src/levels/level_session.c` | The manifest is loaded into a staged catalog first; the caller's data only changes when everything succeeded. |

## Integers that must not overflow

Signed overflow is undefined behaviour in C, so the code checks *before* doing
the arithmetic.

| Idea | Where to read it | What to notice |
|------|------------------|----------------|
| Saturating addition | `game_award_score()` in `src/core/game_score.c` | `amount > INT_MAX - gs->world.score ? INT_MAX : gs->world.score + amount` never computes a sum that does not fit. |
| Wider arithmetic for the hard part | the bonus-life lines in `game_award_score()` | The threshold maths uses `int64_t`, which has room for any product here, then clamps back to `int`. |
| Parsing numbers safely | `unsigned_argument()` in `src/main.c` | `strtoul` instead of `atoi`: `errno` catches overflow, `end` catches trailing junk like `7x`, and `value > UINT_MAX` is checked before narrowing to `unsigned int`. |
| Unsigned wraparound on purpose | `editor_hash_bytes()` in `src/editor/editor_session.c`; `game_random()` in `src/core/game_random.c` | Unsigned overflow is defined (it wraps), which the FNV-1a hash and the xorshift generator rely on. |

[Debugging C](../debugging-c/) shows the UBSan report you get from the naive
version of the score addition.

## Files that are never half written

| Idea | Where to read it | What to notice |
|------|------------------|----------------|
| Write a temporary file, then rename | `level_save_toml_internal()` in `src/shared/serializer_save.c` | The level is written to `<path>.tmp.<pid>` (`serializer_make_temp_path()`), flushed and closed, and only then moved over the real file. A crash mid-save leaves the old file intact. |
| The rename itself | `serializer_replace_file()` in `src/shared/serializer_io.c` | On POSIX `rename()` swaps the file in one step; Windows uses `ReplaceFileW`/`MoveFileExW`. The profile (`src/core/game_profile.c`) and experiment captures (`src/core/game_experiment.c`) save the same way; the profile installs its file through `serializer_install_temp()`, which keeps the temporary file when a Windows replace stops halfway. |
| Checking every step | the `serializer_flush()` and `fclose` results in `level_save_toml_internal()` | A full disk often shows up only at flush or close, so both results are checked before the rename. |

When you find another idea worth a row, add it here with the file and function,
and keep the explanation next to the code as a comment too.
