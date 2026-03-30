# Super Mango — Claude Code Guide

A 2D pixel art platformer written in C11 + SDL2, targeting macOS (Apple Silicon).

---

## Tech Stack

| Technology   | Role                                          | Notes                         |
|--------------|-----------------------------------------------|-------------------------------|
| C11          | Language standard                             | `clang -std=c11`              |
| SDL2         | Window, renderer, input, events, timing       | Homebrew `/opt/homebrew`      |
| SDL2_image   | PNG texture loading (`IMG_LoadTexture`)       |                               |
| SDL2_ttf     | TrueType font rendering                       | Available, not yet in use     |
| SDL2_mixer   | Sound effects + music (`Mix_Chunk`)           |                               |
| Makefile     | Build system (`sdl2-config`, `CC=clang`)      | Outputs to `out/`             |

---

## Build Commands

```sh
make          # compile → out/super-mango
make run      # compile + run
make clean    # remove .o files and out/
```

The Makefile uses `src/*.c` wildcards — **new `.c` files are picked up automatically**, no Makefile changes needed.

---

## Project Structure

```
super-mango-game/
├── CLAUDE.md               ← you are here
├── Makefile
├── assets/                 ← PNG sprites + round9x13.ttf (snake_case, named after components)
├── assets/unused/          ← unused assets for future use
├── sounds/                 ← .wav sound effects (snake_case, named after components)
├── sounds/unused/          ← unused sounds for future use
├── .claude/                ← slash commands, references, scripts
└── src/
    ├── main.c              ← SDL init/teardown, entry point
    ├── game.h / game.c     ← GameState, window, renderer, background, game loop
    ├── player.h / player.c ← Player struct, input, update, render, clamp
    ├── sandbox.c           ← Level layout and entity placement
    └── <entity>.h / .c     ← One module per entity (coin, spider, bird, etc.)
```

### Module responsibilities

| File           | Purpose                                                      |
|----------------|--------------------------------------------------------------|
| `main.c`       | Calls SDL/IMG/TTF/Mix init + teardown; owns `main()`         |
| `game.h`       | `GameState` struct + shared constants; included everywhere   |
| `game.c`       | Implements `game_init`, `game_loop`, `game_cleanup`          |
| `player.h/c`   | Player lifecycle: init, input, update, render, cleanup       |

---

## Architecture

```
main()
  └── SDL / IMG / TTF / Mix init
       └── game_init(gs)       ← load window, renderer, textures, sound, entities
            └── game_loop(gs)  ← poll events → update → render  (60 FPS)
                 └── game_cleanup(gs) ← destroy all resources in reverse init order
  └── Mix / TTF / IMG / SDL teardown
```

`GameState` (defined in `game.h`) is the **single container** passed by pointer to every function. Every resource the game owns lives inside it.

---

## Key Constants (game.h)

| Constant        | Value    | Meaning                                      |
|-----------------|----------|----------------------------------------------|
| `WINDOW_W/H`    | 800×600  | OS window size in physical pixels            |
| `GAME_W/H`      | 400×300  | Logical canvas — **use this for game math**  |
| `TARGET_FPS`    | 60       | Frame cap                                    |
| `TILE_SIZE`     | 48       | Grass tile render size in logical pixels     |
| `FLOOR_Y`       | GAME_H − TILE_SIZE | Y-coordinate of the floor top edge |
| `GRAVITY`       | 800 px/s² | Downward acceleration while airborne        |

> **Important:** All game object positions and sizes live in **logical space (400×300)**.  
> `SDL_RenderSetLogicalSize` scales that canvas 2× to fill the 800×600 OS window automatically.  
> Never use `WINDOW_W` / `WINDOW_H` for game object math.

---

## Current Game State (MVP)

- Parallax multi-layer scrolling background
- `player.png` centered at startup; 8-directional movement via WASD / arrow keys
- Player speed: 200 px/s (dt-based, frame-rate independent)
- ESC or window close → quit

---

## Developer Guidelines

### Adding a new entity (coin, enemy, platform…)

@.claude/references/entity-template.md

### Coding standards and comment style

@.claude/references/coding-standards.md

### Physics / collision pattern

```c
/* Apply gravity every frame while airborne */
if (!player->on_ground) {
    player->vy += GRAVITY * dt;
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

/* Horizontal clamp */
if (player->x < 0.0f)               player->x = 0.0f;
if (player->x > GAME_W - player->w) player->x = (float)(GAME_W - player->w);
```

### Adding sound effects

1. Place `.wav` files in `sounds/` (all sounds use `.wav` format, named after their component).
2. Add `Mix_Chunk *snd_<name>` to `GameState` in `game.h`.
3. Load: `gs->snd_<name> = Mix_LoadWAV("sounds/<name>.wav");` — non-fatal (warn, don't exit).
4. Free: `if (gs->snd_<name>) { Mix_FreeChunk(gs->snd_<name>); gs->snd_<name> = NULL; }`
5. Play: `Mix_PlayChannel(-1, gs->snd_<name>, 0);`

---

## Sprite Sheet Reference

### Analyzing a sprite sheet

```sh
python3 .claude/scripts/analyze_sprite.py assets/<sprite>.png
```

### Frame math

```
Sheet width  = cols × frame_w
Sheet height = rows × frame_h

source_x = (frame_index % cols) * frame_w
source_y = (frame_index / cols) * frame_h
```

### Standard row layout (most pixel art packs)

| Row | Animation   | Notes                        |
|-----|-------------|------------------------------|
| 0   | Idle        | 1–4 frames, subtle breathing |
| 1   | Walk / Run  | 6–8 frames, looping          |
| 2   | Jump (up)   | 2–4 frames, one-shot         |
| 3   | Fall / Land | 2–4 frames                   |
| 4   | Attack      | 4–8 frames, one-shot         |
| 5   | Death / Hurt| 4–6 frames, one-shot         |

Full animation reference: @.claude/references/animation-sequences.md

Full sprite sheet analysis notes: @.claude/references/sprite-sheet-analysis.md

---

## Safety Rules

- Every pointer field must be set to `NULL` after freeing (double-free safety).
- Error paths call `SDL_GetError()` / `IMG_GetError()` / `Mix_GetError()` then `exit(EXIT_FAILURE)`.
- Resources are always freed in **reverse init order**.
- `float` for positions and velocities; cast to `int` only at render time (`SDL_Rect` requires int).
