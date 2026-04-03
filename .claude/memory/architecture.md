---
name: Architecture & GameState contract
description: Core architectural rules that must never be violated — single GameState, no heap, fixed arrays
type: project
---

## Rule

`GameState` (defined in `src/game.h`) is the **single source of truth** passed by pointer to every function. All game resources live inside it **by value** — no heap allocation, no global variables.

**Why:** The Emscripten (WebAssembly) target requires a single-frame callback model. GameState must survive across frame boundaries without dynamic memory.

**How to apply:**
- New entities → fixed-size arrays + count field inside GameState, e.g. `Spider spiders[MAX_SPIDERS]; int spider_count;`
- New textures → pointer field inside GameState, loaded in `game_init`, freed in `game_cleanup`
- New sounds → `Mix_Chunk *snd_<name>` inside GameState; non-fatal load
- Never use `malloc/free` for game objects

## Key constants (all in game.h)

| Constant | Value | Meaning |
|----------|-------|---------|
| GAME_W/H | 400×300 | Logical canvas — ALL game math uses this |
| WINDOW_W/H | 800×600 | OS window — never use for game logic |
| TILE_SIZE | 48 | Sprite frame size in logical px |
| FLOOR_Y | GAME_H - TILE_SIZE | Top of floor |
| GRAVITY | 800 px/s² | Downward accel |
| WORLD_W | 1600 | Total level width (4 screens) |

## Entry flow

```
main() → SDL/IMG/TTF/Mix init
  └── game_init(gs)   — load all resources
       └── game_loop(gs) → game_loop_frame() per frame
            └── game_cleanup(gs) — free in reverse init order
  └── Mix/TTF/IMG/SDL teardown
```

## Module pattern

Each entity is a self-contained `.h/.c` pair:
- `<entity>_init(entity*, renderer, x, y)`
- `<entity>_update(entity*, dt)`
- `<entity>_render(entity*, renderer)`
- `<entity>_cleanup(entity*)`

Wired into `game_init` / `game_loop` / `game_cleanup` in `game.c`.
Makefile `src/*.c` wildcard picks up new files automatically.
