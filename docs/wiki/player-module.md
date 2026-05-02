# Player Module

<a id="home"></a>

---

The player module spans `player.h` and `player.c` and owns the full lifecycle of the player character: texture loading, keyboard input, physics simulation, sprite animation, rendering, and cleanup.

---

## Lifecycle at a Glance

```
player_init        ← called once from game_init
  └── IMG_LoadTexture (player.png → GPU)
  └── set initial position, speed, animation state

per frame (game_loop):
  player_handle_input   ← sample keyboard and gamepad, set vx / vy / on_ground
  player_update         ← apply gravity, integrate position, collide, animate
  player_render         ← draw the current frame to the renderer
  player_get_hitbox     ← return physics hitbox used by game_loop for collision

player_reset       ← called from game_loop when the player loses a life
  └── reset position, velocity, and animation state (texture is reused, not reloaded)

player_cleanup     ← called once from game_cleanup
  └── SDL_DestroyTexture
```

---

## Initialization -- `player_init`

```c
void player_init(Player *player, SDL_Renderer *renderer);
```

| Action | Detail |
|--------|--------|
| Load texture | `IMG_LoadTexture(renderer, "assets/player.png")` -- 192x288 sheet |
| Frame rect | `{x=0, y=0, w=48, h=48}` -- first cell (row 0, col 0) |
| Display size | `w = h = 48` px (logical coordinates) |
| Start position | On pillar 0: `x = 80.0f + (TILE_SIZE - 48) / 2.0f` = `80` |
| Start Y | `FLOOR_Y - 2*TILE_SIZE + 16 - 48 + FLOOR_SINK` = `172` (on top of 2-high pillar) |
| Speed | `160.0f` px/s horizontal |
| Initial velocity | `vx = vy = 0.0f` |
| `on_ground` | `1` (starts on the floor) |
| Animation | `ANIM_IDLE`, frame 0, facing right |

**`FLOOR_SINK = 16`:** The sprite sheet has transparent padding at the bottom of each 48x48 frame. Sinking 16 px makes the character's feet visually rest on the grass, even though the physics edge (`y + h`) is 16 px above `FLOOR_Y`.

---

## Input -- `player_handle_input`

```c
void player_handle_input(Player *player, Mix_Chunk *snd_jump,
                         SDL_GameController *ctrl,
                         const VineDecor *vines, int vine_count,
                         const LadderDecor *ladders, int ladder_count,
                         const RopeDecor *ropes, int rope_count);
```

Called **once per frame** before `player_update`. Uses `SDL_GetKeyboardState` to read the instantaneous keyboard state (held keys), not events. This gives smooth, continuous movement. The `ctrl` parameter is the active gamepad handle; pass `NULL` when no controller is connected -- keyboard input still works normally. The `vines`, `ladders`, and `ropes` arrays are used for climbable grab detection when the player presses UP.

### Key Bindings

| Input | Action |
|--------|--------|
| Left Arrow / A | Move left (`vx -= speed`), `facing_left = 1` |
| Right Arrow / D | Move right (`vx += speed`), `facing_left = 0` |
| Up Arrow / W | Grab nearest vine / climb up on vine |
| Down Arrow / S | Climb down on vine |
| D-Pad left / right | Move left / right (gamepad) |
| D-Pad up / down | Grab vine / climb up / climb down (gamepad) |
| Left analog stick (X-axis) | Move left / right (dead-zone: 8000 / 32767) |
| Left analog stick (Y-axis) | Climb up / down on vine (gamepad) |
| Space | Jump (`-325` px/s impulse -- ground and vine dismount) |
| `A` button / Cross (gamepad) | Jump |
| ESC | Quit (handled in `game_loop`, not here) |
| `Start` button (gamepad) | Quit (handled in `game_loop`, not here) |

### Jump Logic

```c
int want_jump = keys[SDL_SCANCODE_SPACE];
if (ctrl) {
    want_jump |= SDL_GameControllerGetButton(ctrl, SDL_CONTROLLER_BUTTON_A);
}
if (player->on_ground && want_jump) {
    player->vy        = -325.0f;   // upward impulse (negative = up in SDL)
    player->on_ground  = 0;
    if (snd_jump) Mix_PlayChannel(-1, snd_jump, 0);
}
```

- Keyboard jump impulse is `-325.0f` px/s (upward) for both ground and vine dismount.
- Gamepad jump impulse is `-500.0f` px/s (upward) for both ground and vine dismount.
- `on_ground` is set to `0` immediately so the jump condition fires only once.
- The sound is guarded by `if (snd_jump)` to tolerate a failed WAV load.

### Vine Climbing

When the player presses UP (and is not holding Space), the input handler searches for the nearest vine within grab range (`VINE_W + 2 x VINE_GRAB_PAD = 24 px` wide). If found:

1. `player->on_vine = 1`, `player->vine_index` is set to the vine's index.
2. The player snaps horizontally to centre on the vine.
3. Gravity is disabled while `on_vine == 1`.
4. UP/DOWN keys control vertical movement at `CLIMB_SPEED` (80 px/s).
5. LEFT/RIGHT keys allow horizontal drift at `CLIMB_H_SPEED` (80 px/s).
6. Pressing Space while on a vine triggers a dismount: `on_vine = 0`, `vy = -325.0f` (keyboard) or `-500.0f` (gamepad).
7. The `ANIM_CLIMB` state plays (row 4, 2 frames at 100 ms each); the animation freezes when the player is stationary on the vine.

### Horizontal velocity reset

`player->vx` is reset to `0.0f` at the start of each call. The player stops instantly when no horizontal key is held -- no friction/deceleration is applied in the current MVP.

---

## Physics -- `player_update`

```c
void player_update(Player *player, float dt,
                   const Platform *platforms, int platform_count,
                   const FloatPlatform *float_platforms, int float_platform_count,
                   const Bouncepad *bouncepads, int bouncepad_count,
                   const VineDecor *vines, int vine_count,
                   const LadderDecor *ladders, int ladder_count,
                   const RopeDecor *ropes, int rope_count,
                   const Bridge *bridges, int bridge_count,
                   const SpikePlatform *spike_platforms, int spike_platform_count,
                   const int *floor_gaps, int floor_gap_count,
                   int *out_bounce_idx,
                   int *out_fp_landed_idx,
                   int prev_fp_landed_idx,
                   int world_w);
```

`dt` is the time in **seconds** since the last frame (e.g. `0.016` for 60 FPS). Multiplying speed by `dt` makes movement frame-rate independent. The function resolves collisions against the floor, one-way platforms, float platforms, bridges, spike platforms, bouncepads, vines, ladders, and ropes.

- `*out_bounce_idx` is set to the bouncepad index if the player lands on one; callers initialise to `-1`.
- `*out_fp_landed_idx` is set to the float platform index if the player lands on one; used to drive the crumble timer and nudge the player along with moving rail platforms.
- `prev_fp_landed_idx` is the float platform the player was standing on last frame -- needed for the "stay on" check when a platform moves upward.

### Gravity

```c
player->on_ground = 0;          // reset every frame — walk-off edges start falling immediately
player->vy += GRAVITY * dt;     // GRAVITY = 800.0f px/s²; runs unconditionally
```

`on_ground` is cleared to `0` at the start of every `player_update` call so the player immediately begins falling when they walk off a platform edge. Gravity then runs unconditionally; the floor/platform snap below cancels the tiny fall each frame while the player stands on a surface, keeping them rock-solid on the ground.

### Position Integration

```c
player->x += player->vx * dt;
player->y += player->vy * dt;
```

### Floor Collision

```c
float floor_snap = (float)(FLOOR_Y - player->h + FLOOR_SINK);
// = 252 - 48 + 16 = 220

if (player->y >= floor_snap) {
    player->y        = floor_snap;
    player->vy       = 0.0f;
    player->on_ground = 1;
}
```

When the player's bottom edge reaches the floor surface, position is snapped, `vy` is zeroed, and `on_ground` becomes `1`.

### Horizontal Clamp

```c
if (player->x + PHYS_PAD_X < 0.0f)
    player->x = -(float)PHYS_PAD_X;
if (player->x + player->w - PHYS_PAD_X > WORLD_W)
    player->x = (float)(WORLD_W - player->w + PHYS_PAD_X);
```

Keeps the player's **physics body** (inset by `PHYS_PAD_X = 15` px on each side) inside the full `world_w` scrollable world. The transparent side-padding of the sprite frame is allowed to slide off-screen while the visible character stays flush with the world border.

### Ceiling Clamp

```c
if (player->y + PHYS_PAD_TOP < 0.0f) {
    player->y  = -(float)PHYS_PAD_TOP;
    player->vy = 0.0f;
}
```

Stops upward movement when the physics top edge (`y + PHYS_PAD_TOP`) hits the canvas ceiling. `PHYS_PAD_TOP = 18` lets the transparent head-room of the sprite frame slide above `y = 0` before the physics edge triggers.

---

## Animation -- `player_animate` (static)

Called at the end of `player_update`. Selects the correct `AnimState` based on physics state, advances the frame timer, and updates `player->frame` (the source rect cutting into the sprite sheet).

### State Selection

```c
AnimState target;
if (player->on_vine) {
    target = ANIM_CLIMB;
} else if (!player->on_ground) {
    target = (player->vy < 0.0f) ? ANIM_JUMP : ANIM_FALL;
} else if (player->vx != 0.0f) {
    target = ANIM_WALK;
} else {
    target = ANIM_IDLE;
}
```

| Condition | Selected State |
|-----------|---------------|
| Climbing a vine (`on_vine == 1`) | `ANIM_CLIMB` |
| Airborne + moving up (`vy < 0`) | `ANIM_JUMP` |
| Airborne + moving down (`vy >= 0`) | `ANIM_FALL` |
| On ground + horizontal velocity | `ANIM_WALK` |
| On ground + no movement | `ANIM_IDLE` |

> **Climb freeze:** When `ANIM_CLIMB` is active but the player has zero vertical velocity (stationary on vine), the animation timer is paused -- the climb sprite holds its current frame.

### Frame Timer

```c
player->anim_timer_ms += dt_ms;
if (player->anim_timer_ms >= frame_duration) {
    player->anim_timer_ms -= frame_duration;   // carry-over, not reset
    player->anim_frame_index =
        (player->anim_frame_index + 1) % ANIM_FRAME_COUNT[state];
}
```

Leftover time carries into the next frame to keep animation speed accurate across variable frame rates.

### Animation Table

| State | Row | Frames | ms/frame | Total cycle |
|-------|-----|--------|----------|-------------|
| `ANIM_IDLE` | 0 | 4 | 150 | 600 ms |
| `ANIM_WALK` | 1 | 4 | 100 | 400 ms |
| `ANIM_JUMP` | 2 | 2 | 150 | 300 ms |
| `ANIM_FALL` | 3 | 1 | 200 | 200 ms |
| `ANIM_CLIMB` | 4 | 2 | 100 | 200 ms |

### Source Rect Update

```c
player->frame.x = player->anim_frame_index * FRAME_W;   // col x 48
player->frame.y = ANIM_ROW[player->anim_state]  * FRAME_H;  // row x 48
```

---

## Rendering -- `player_render`

```c
void player_render(Player *player, SDL_Renderer *renderer, int cam_x);
```

```c
/* Invincibility blink: skip every alternate 100 ms window */
if (player->hurt_timer > 0.0f) {
    int interval = (int)(player->hurt_timer * 1000.0f) / 100;
    if (interval % 2 == 1) return;   /* blink off — skip this frame */
}

SDL_Rect dst = {
    .x = (int)player->x - cam_x,  // world → screen: subtract camera offset
    .y = (int)player->y,
    .w = player->w,         // 48
    .h = player->h          // 48
};

SDL_RendererFlip flip = player->facing_left
    ? SDL_FLIP_HORIZONTAL
    : SDL_FLIP_NONE;

SDL_RenderCopyEx(renderer, player->texture, &player->frame, &dst,
                 0.0, NULL, flip);
```

`SDL_RenderCopyEx` is used (instead of `SDL_RenderCopy`) to support horizontal flipping. Angle and center are `0` / `NULL` so no rotation is applied.

> **Invincibility blink:** While `player->hurt_timer > 0`, `player_render` converts the remaining time into a 100 ms cadence -- `interval = (int)(player->hurt_timer * 1000.0f) / 100`. On odd intervals the function returns early, skipping the draw call and making the sprite flash to indicate temporary invincibility.

---

## Hitbox -- `player_get_hitbox`

```c
SDL_Rect player_get_hitbox(const Player *player);
```

Returns an `SDL_Rect` representing the player's tightly-inset physics hitbox in logical pixels. The hitbox is smaller than the full 48x48 display frame to exclude transparent padding in the sprite sheet. It is used by `game_loop` for AABB intersection tests against spider enemies.

| Inset | Constant | Value | Effect |
|-------|----------|-------|--------|
| Left and Right | `PHYS_PAD_X` | `15` px | Physics width = 48 - 30 = 18 px |
| Top | `PHYS_PAD_TOP` | `18` px | Physics top tracks the character's head |
| Bottom | `FLOOR_SINK` | `16` px | Physics bottom tracks the character's feet |

```c
SDL_Rect r;
r.x = (int)(player->x) + PHYS_PAD_X;
r.y = (int)(player->y) + PHYS_PAD_TOP;
r.w = player->w - 2 * PHYS_PAD_X;
r.h = player->h - PHYS_PAD_TOP - FLOOR_SINK;
return r;
```

`game_loop` calls `player_get_hitbox` each frame (when `hurt_timer == 0`) and passes the result to `SDL_HasIntersection` alongside each spider's rect. On overlap, `hurt_timer` is set to `1.5` seconds.

---

## Cleanup -- `player_cleanup`

```c
void player_cleanup(Player *player) {
    if (player->texture) {
        SDL_DestroyTexture(player->texture);
        player->texture = NULL;
    }
}
```

Must be called **before** `SDL_DestroyRenderer`, because textures are owned by the renderer.

---

## Reset -- `player_reset`

```c
void player_reset(Player *player);
```

Resets the player's position and state to the starting values **without reloading the texture**. Called by `game_loop` when the player loses a life (hearts reach 0). Because the GPU texture is already loaded, only the position, velocity, `on_ground`, and animation fields need to be re-initialised -- the same `player.png` texture handle is reused directly.

| Action | Detail |
|--------|--------|
| Position | Reset to pillar 0 (x=80), snapped to pillar top surface |
| Velocity | `vx = vy = 0.0f` |
| `on_ground` | `1` |
| `hurt_timer` | `0.0f` (no invincibility) |
| Animation | `ANIM_IDLE`, frame 0 |
| Texture | **unchanged** -- reuses the already-loaded handle |

---

## Physics Constants Reference

| Constant | Value | Location |
|----------|-------|----------|
| `GRAVITY` | `800.0f` px/s^2 | `game.h` |
| `FLOOR_Y` | `252` px | `game.h` (`GAME_H - TILE_SIZE`) |
| Jump impulse `vy` (keyboard) | `-325.0f` px/s | `player.c` (hard-coded) |
| Jump impulse `vy` (gamepad) | `-500.0f` px/s | `player.c` (hard-coded) |
| `CLIMB_SPEED` | `80.0f` px/s | `player.c` (local `#define`) |
| `CLIMB_H_SPEED` | `80.0f` px/s | `player.c` (local `#define`) |
| `VINE_GRAB_PAD` | `4` px | `player.c` (local `#define`) |
| Horizontal speed | `160.0f` px/s | `player.c` (`player->speed`) |
| `FLOOR_SINK` | `16` px | `player.c` (local `#define`) |
| `FRAME_W` / `FRAME_H` | `48` px | `player.c` (local `#define`) |
