# Bosser's Lessons Learned — Engineering Rules

Hard-won rules from real builds. Every Bosser instance must follow them.

---

## Lesson 1: Scalars before arrays in TOML

TOML requires scalar fields before `[[arrays]]`. The level loader will fail to parse a file that violates this. Learned the hard way when a generated level file had a `[[platforms]]` block before `screen_count`.

**Rule:** In every TOML file (level definitions, configs), write all scalar key-value pairs first. Then write `[[array]]` blocks. No exceptions.

---

## Lesson 2: Reverse init order for cleanup

Resources must always be freed in the exact reverse order they were initialized. Free textures before the renderer, free the renderer before SDL_Quit, free sounds before Mix_Quit. Violating this causes segfaults on exit.

**Rule:** `game_cleanup()` mirrors `game_init()` in reverse. If you add something in init, add its cleanup at the top of the cleanup function.

---

## Lesson 3: Logical space for game math, physical space for SDL

`GAME_W/H` (400×300) is the logical canvas. `WINDOW_W/H` (800×600) is the OS window. `SDL_RenderSetLogicalSize` handles the 2× scaling. Never use `WINDOW_W/H` for game object positions, sizes, or collision math.

**Rule:** All game logic uses `GAME_W`, `GAME_H`, `FLOOR_Y`, `TILE_SIZE`. Only SDL window creation and renderer setup use `WINDOW_W`, `WINDOW_H`.

---

## Lesson 4: Float for positions, int for rendering

Positions and velocities are `float`. `SDL_Rect` requires `int`. Cast only at render time — `(int)entity->x`, `(int)entity->y`. Never store positions as `int` — you'll lose sub-pixel precision and movement will feel choppy.

**Rule:** `float` in structs, `int` in `SDL_RenderCopy` calls. Cast at the last possible moment.

---

## Lesson 5: Delta-time physics at fixed framerate

The game loop runs at `TARGET_FPS` (60). Physics uses `dt = 1.0f / TARGET_FPS`. VSync is primary, manual frame pacing is fallback. Never tie physics to frame count — always multiply by `dt`.

**Rule:** `velocity * dt` for movement, `acceleration * dt` for forces. No hardcoded per-frame values.

---

## Lesson 6: Non-fatal asset loading

Asset loading (sounds, optional sprites) must never `exit()` on failure. Use `SDL_Log` warnings and set the pointer to NULL. The game should run without any single asset — missing sounds just mean silence, missing sprites mean invisible entities (which is a bug you'll catch in testing, but shouldn't crash the game).

**Rule:** `Mix_LoadWAV` and `IMG_LoadTexture` for non-critical assets: warn, set NULL, continue. Only fatal errors are SDL init, renderer creation, and core font loading.

---

## Lesson 7: NULL after free

Every pointer field must be set to `NULL` after freeing. Double-free safety. If `game_cleanup` runs twice (e.g., error path after partial cleanup), NULL pointers prevent crashes.

**Rule:** `Mix_FreeChunk(gs->snd_foo); gs->snd_foo = NULL;` — always both lines.

---

## Lesson 8: Compile with -Wall -Wextra -Wpedantic

Zero warnings. Every warning is a potential bug. The Makefile enforces this — if it compiles with warnings, you did something wrong. Fix the warning, don't suppress it.

**Rule:** `make` must produce zero warnings. If a warning appears, fix the code. Never add `-Wno-*` flags.

---

## Lesson 9: New .c files are picked up automatically

The Makefile uses `src/*.c` wildcards. New source files in existing subdirectories are compiled automatically — no Makefile changes needed. But new subdirectories need a wildcard pattern added.

**Rule:** Adding `src/new_module/foo.c` works immediately. Adding `src/new_module/` as a new directory requires updating the Makefile's wildcard patterns.

---

## Lesson 10: Delegate, don't duplicate

You have three specialists. Don't write TOML levels — that's Lugio. Don't design sprites — that's Goobma. Don't audit docs — that's Warro. Your job is the engine: C code, SDL, physics, architecture, build system.

**Rule:** If a request touches `.c`, `.h`, `Makefile`, or game architecture — handle it. If it's a level, sprite, or doc request — delegate. No exceptions.
