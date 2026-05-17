---
name: Coding standards & comment style
description: This codebase is a learning resource — every SDL2 call must be explained; specific error/comment rules apply
type: project
---

## Intent

This codebase is **intentionally verbose** as a learning resource for C+SDL2 beginners. Comments explain the *why*, not just the what. Never strip comments or make code more terse to save tokens.

## Comment rules

1. Every non-trivial SDL call gets a block comment explaining arguments and return value
2. Numeric literals always get a unit comment (`160.0f /* px/s */`)
3. Every pointer parameter gets a comment explaining the by-pointer vs by-value choice
4. `float` vs `int` distinction must be explained at point of use
5. `game_cleanup` must explain reverse-free order with a comment

## Error handling

| Severity | Pattern |
|----------|---------|
| Fatal (can't continue) | `fprintf(stderr, "...: %s\n", SDL_GetError()); exit(EXIT_FAILURE);` |
| Non-fatal (SFX, decorations) | `fprintf(stderr, "Warning: ...: %s\n", Mix_GetError());` — continue |

Sound effects are **always non-fatal**. Core textures and renderer are **fatal**.

## Naming conventions

| Thing | Pattern | Example |
|-------|---------|---------|
| Files | snake_case | `jumping_spider.c`, `float_platform.h` |
| Types | PascalCase | `Spider`, `FloatPlatform`, `GameState` |
| Functions | `<module>_<verb>` | `spider_update()`, `player_handle_input()` |
| Constants | UPPER_SNAKE | `GAME_W`, `MAX_COINS`, `TILE_SIZE` |
| Fields | snake_case | `vx`, `on_ground`, `anim_timer` |

## Physics pattern (copy exactly)

```c
/* Apply gravity */
if (!e->on_ground) e->vy += GRAVITY * dt;
e->x += e->vx * dt;
e->y += e->vy * dt;

/* Floor collision */
if (e->y + e->h >= FLOOR_Y) {
    e->y  = (float)(FLOOR_Y - e->h);
    e->vy = 0.0f;
    e->on_ground = 1;
} else {
    e->on_ground = 0;
}
```

## SDL_Rect casting rule

Positions use `float`. Cast to `int` only at render time:
```c
SDL_Rect dst = { (int)e->x, (int)e->y, e->w, e->h };
```
