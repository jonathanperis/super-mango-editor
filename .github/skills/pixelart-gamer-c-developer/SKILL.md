---
name: pixelart-gamer-c-developer
description: "C developer expert in pixel art 2D games using SDL2, SDL2_image, SDL2_ttf, SDL2_mixer. Use when: adding new game features, implementing new entities, writing SDL2 rendering/audio/input code, documenting C code for learners, adding new source files, understanding the init→loop→cleanup pattern, physics/gravity/collision, sprite animation in C, sound effects with Mix_Chunk, structuring GameState. Trigger phrases: 'add feature', 'new entity', 'how do I render', 'SDL2 code', 'implement', 'add sound', 'document this', 'write the C', 'game loop', 'player physics', 'collision', 'new .c file', 'new .h file'."
argument-hint: "Describe what to implement or document (e.g. 'add a coin entity', 'document player_update', 'add enemy movement')"
---

# Pixel Art Game C Developer

Expert in writing and documenting C11 + SDL2 code for 2D pixel art platformers, with a focus on teaching through thorough inline comments.

## Project Stack

| Technology     | Role                                        | Version / Notes              |
|----------------|---------------------------------------------|------------------------------|
| C11            | Language standard                           | `clang -std=c11`             |
| SDL2           | Window, renderer, input, events, timing     | 2.x via Homebrew             |
| SDL2_image     | PNG texture loading (`IMG_LoadTexture`)     |                              |
| SDL2_ttf       | TrueType font rendering                     | Available, not yet used      |
| SDL2_mixer     | Sound effects + music (`Mix_Chunk`)         |                              |
| Makefile       | Build system (`sdl2-config`, `CC=clang`)    | Output in `out/`             |

All coordinates live in **logical space (400×300)** — never use `WINDOW_W`/`WINDOW_H` for game objects. `SDL_RenderSetLogicalSize` handles the 2× scale to the 800×600 OS window automatically.

## Architecture

```
main()
  └── SDL / IMG / TTF / Mix init
       └── game_init(gs)      ← load window, renderer, textures, sound, entities
            └── game_loop(gs) ← poll events → update → render  (60 FPS)
                 └── game_cleanup(gs) ← destroy all resources in reverse order
  └── Mix / TTF / IMG / SDL teardown
```

The `GameState` struct (in `game.h`) is the single container passed by pointer everywhere. Every resource the game owns lives inside it.

## Coding Standards

See [coding-standards.md](./references/coding-standards.md) for the full rules. Quick summary:

- **C11**, `#pragma once` for headers, `typedef struct` for all types.
- `float` for positions and velocities; cast to `int` only at render time.
- Every pointer field is set to `NULL` after freeing (double-free safety).
- Inline comments explain **why**, not just what — written for someone learning C.
- Error paths call `SDL_GetError()` / `IMG_GetError()` / `Mix_GetError()` and `exit(EXIT_FAILURE)`.
- Resources always freed in **reverse init order**.

## When to Use This Skill (Procedures)

### Adding a New Entity (e.g. Coin, Enemy, Platform)

1. Read [entity-template.md](./references/entity-template.md) for the full header + source skeleton.
2. Create `src/<entity>.h` — define the struct and declare the 5 lifecycle functions.
3. Create `src/<entity>.c` — implement init, input (if any), update, render, cleanup.
4. Add `SDL_Texture *<entity>_tex` (and optionally `Mix_Chunk *snd_<event>`) to `GameState` in `game.h`.
5. Load and free them in `game_init` / `game_cleanup` in `game.c`.
6. Call `<entity>_update` and `<entity>_render` at the right points in `game_loop`.
7. The Makefile wildcard `src/*.c` picks up new files automatically — no Makefile change needed.

### Adding Sound Effects

1. Place `.wav` / `.mp3` files in `assets/sounds/`.
2. Add `Mix_Chunk *snd_<name>` to `GameState` in `game.h`.
3. In `game_init`: `gs->snd_<name> = Mix_LoadWAV("assets/sounds/<name>.wav");` — treat as non-fatal (warn, don't exit).
4. In `game_cleanup`: `if (gs->snd_<name>) { Mix_FreeChunk(gs->snd_<name>); gs->snd_<name> = NULL; }`
5. Play it: `Mix_PlayChannel(-1, gs->snd_<name>, 0);` — `-1` picks any free channel.

### Documenting Existing Code

Follow the comment style in [coding-standards.md](./references/coding-standards.md):
- **File header**: one-line purpose statement at the top of every `.c` / `.h`.
- **Function docblock**: before each function — what it does, when it's called, important parameters.
- **Inline comments**: explain the SDL2 API call, the "why" of the value, or a non-obvious consequence.
- **Struct fields**: every field gets a short trailing comment.

### Implementing Physics / Collision

```c
/* Apply gravity every frame while airborne */
if (!player->on_ground) {
    player->vy += GRAVITY * dt;   /* GRAVITY = 800 px/s² */
}

player->x += player->vx * dt;
player->y += player->vy * dt;

/* Floor collision */
if (player->y + player->h >= FLOOR_Y) {
    player->y         = (float)(FLOOR_Y - player->h);
    player->vy        = 0.0f;
    player->on_ground = 1;
} else {
    player->on_ground = 0;
}

/* Horizontal clamping — keep inside GAME_W */
if (player->x < 0.0f)               player->x = 0.0f;
if (player->x > GAME_W - player->w) player->x = (float)(GAME_W - player->w);
```

### Rendering Order (painter's algorithm)

Always render back-to-front each frame:

```
SDL_RenderClear
  → background  (full canvas stretch)
  → floor tiles (tiled across GAME_W)
  → platforms   (if any)
  → collectibles (coins, power-ups)
  → enemies
  → player      (on top of everything)
  → HUD / UI    (topmost layer)
SDL_RenderPresent
```

## References

- [Coding Standards & Comment Style](./references/coding-standards.md)
- [Entity Implementation Template](./references/entity-template.md)
