# Animation Sequence Conventions

## What Is an Animation Sequence?

An animation sequence is a set of consecutive frames (from a single row or a defined range) that play in order
to create the illusion of movement. The engine advances the source rect across the sheet at a fixed rate.

## Typical State → Row Mapping

Most pixel art packs follow this convention (adapt per pack):

| State         | Row | Direction Notes                            | Loop? |
|--------------|-----|---------------------------------------------|-------|
| Idle          | 0   | Facing right; flip horizontally for left   | Yes   |
| Walk / Run    | 1   | Same flip convention                        | Yes   |
| Jump (rise)   | 2   | One-shot, hold last frame at apex           | No    |
| Fall          | 3   | Hold last frame until landing               | No    |
| Attack        | 4   | One-shot; return to idle on completion      | No    |
| Hurt / Hitstun| 5   | Brief flash, then back to idle              | No    |

> The Super Mango `player.png` has 6 rows — map them by visual inspection using the analysis script, then confirm here.

## Animation Timing

| Style       | Frame Duration | Effective FPS |
|-------------|---------------|---------------|
| Very slow   | 200 ms        | 5 fps         |
| Slow        | 133 ms        | 7.5 fps       |
| Normal walk | 100 ms        | 10 fps        |
| Fast run    | 67 ms         | 15 fps        |
| Attack      | 50 ms         | 20 fps        |

Always drive animation with **accumulated delta time**, not frame count — so it stays frame-rate independent:

```c
player->anim_timer += dt;                     /* accumulate seconds */
if (player->anim_timer >= FRAME_DURATION) {
    player->anim_timer -= FRAME_DURATION;
    player->anim_col = (player->anim_col + 1) % anim_frame_count;
}
player->frame.x = player->anim_col * FRAME_W;
player->frame.y = anim_row        * FRAME_H;
```

## State Machine for Animation

```
[Idle] ──── move key held ────► [Walk]
  │                                │
  └── jump pressed ──► [Jump] ─── land ──► [Fall] ─── floor collision ──► [Idle]
                                     │
                                   (apex: vy > 0) ──► [Fall]
```

Rules:
- Only transition on **state entry** — set `anim_col = 0` when switching states.
- For one-shot animations (jump, attack): do not loop; clamp `anim_col` at `frame_count - 1`.
- Horizontal flip: store a `facing` flag (`LEFT = -1`, `RIGHT = 1`); use `SDL_RenderCopyEx` with `SDL_FLIP_HORIZONTAL` when facing left.

## SDL2 Flip Example

```c
SDL_RendererFlip flip = (player->facing < 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
SDL_RenderCopyEx(
    renderer,
    player->texture,
    &player->frame,   /* source rect: the current animation frame */
    &dst,             /* destination rect on screen                */
    0.0,              /* rotation angle                            */
    NULL,             /* rotation center (NULL = center of rect)   */
    flip              /* horizontal flip when facing left          */
);
```

## Adding a New State (Checklist)

1. Identify the row in the sprite sheet (script or manual inspection).
2. Count the frames in that row.
3. Add a new enum value to the player state enum.
4. Add a transition rule in the state machine.
5. Set `anim_row`, `frame_count`, and `loop` flag on state entry.
6. Test with slow motion (reduce `TARGET_FPS` to 10 temporarily).
