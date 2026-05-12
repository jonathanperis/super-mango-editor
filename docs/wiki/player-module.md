# Player Module

<a id="home"></a>

---

The player module is split across focused files under `src/player/`. `player.h` is the public API; `player_internal.h` holds private sprite/hitbox constants shared by implementation files.

| File | Role |
|------|------|
| `player.h` | `Player` struct, `AnimState`, public function declarations |
| `player.c` | High-level `player_update` orchestration |
| `player_lifecycle.c` | init, render, hitbox, reset, cleanup, default physics |
| `player_input.c` | keyboard/gamepad intent, run state, climb input |
| `player_motion.c` | acceleration, friction, counter-accel, air control |
| `player_jump.c` | jump buffer, coyote time, jump cut |
| `player_climb.c` | vine/ladder/rope grab detection and climbing helpers |
| `player_surfaces.c` | floor/platform/float-platform/bridge/bouncepad surface collision helpers |
| `player_animation.c` | animation state selection and source frame updates |

---

## Lifecycle at a Glance

```text
player_init
  ├── IMG_LoadTexture("assets/sprites/player/player.png")
  ├── 48x48 frame setup from 192x288 sheet (4 cols x 6 rows)
  ├── default spawn (overridden by LevelDef)
  └── player_apply_default_physics

per frame
  ├── player_handle_input        -> move_dir, run, jump/climb intent
  ├── player_update              -> jump buffer, motion, gravity, collisions, animation
  ├── player_render              -> draw current frame with camera offset
  └── player_get_hitbox          -> inset AABB for damage/collision checks

player_reset                     -> restore spawn/state; keep texture + tunable physics
player_cleanup                   -> SDL_DestroyTexture
```

---

## Public API

```c
void player_init(Player *player, SDL_Renderer *renderer);
void player_apply_default_physics(Player *player);
void player_handle_input(Player *player, Mix_Chunk *snd_jump,
                         SDL_GameController *ctrl,
                         const VineDecor *vines, int vine_count,
                         const LadderDecor *ladders, int ladder_count,
                         const RopeDecor *ropes, int rope_count);
void player_update(Player *player, float dt, Mix_Chunk *snd_jump,
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
void player_render(Player *player, SDL_Renderer *renderer, int cam_x);
SDL_Rect player_get_hitbox(const Player *player);
void player_reset(Player *player);
void player_cleanup(Player *player);
```

---

## Initialization

| Action | Current code |
|--------|--------------|
| Texture | `assets/sprites/player/player.png` |
| Sheet | 192×288 px, 4 columns × 6 rows |
| Frame | 48×48 px (`PLAYER_FRAME_W/H`) |
| Default spawn | `spawn_x = 80`, `spawn_y = FLOOR_Y - 2*TILE_SIZE + 16` |
| Runtime spawn | `level_load` may override from `LevelDef.player_start_x/y` |
| Physics defaults | `player_apply_default_physics`, then optional LevelDef overrides |

Default display size is 48×48 logical pixels. `PLAYER_FLOOR_SINK = 16` sinks the frame into the floor to compensate for transparent sprite padding.

---

## Movement Physics

Current movement uses acceleration and friction, not instant velocity reset.

| Field / constant | Default | Meaning |
|------------------|---------|---------|
| `walk_max_speed` | 100.0 px/s | walking cap |
| `run_max_speed` | 250.0 px/s | running cap |
| `walk_ground_accel` | 750.0 px/s² | ground walk acceleration |
| `run_ground_accel` | 600.0 px/s² | ground run acceleration |
| `ground_friction` | 550.0 px/s² | braking with no input on ground |
| `ground_counter_accel` | 100.0 px/s² | extra brake when reversing direction |
| `air_accel_walk` | 350.0 px/s² | air control for walk jumps |
| `air_accel_run` | 180.0 px/s² | reduced air control for run jumps |
| `air_friction` | 80.0 px/s² | passive air drag with no input |

`LevelDef.physics` can override these values; `-1.0` keeps engine defaults and `0.0` is a valid override.

---

## Jumping

| Constant | Value | Meaning |
|----------|-------|---------|
| `JUMP_VY` | −325.0 px/s | jump impulse |
| `JUMP_BUFFER_TIME` | 0.10 s | pre-landing jump buffer |
| `PLAYER_COYOTE_TIME` | 0.10 s | post-edge jump grace |
| `JUMP_CUT_FACTOR` | 0.45 | shorten jump when button released early |

`player_update` receives `snd_jump` so buffered/coyote jumps can play the same guarded jump sound as direct jumps.

---

## Climbables

The same climb state supports vines, ladders, and ropes.

| Constant | Value | Meaning |
|----------|-------|---------|
| `CLIMB_SPEED` | 80.0 px/s | vertical climb speed |
| `CLIMB_H_SPEED` | 80.0 px/s | horizontal drift while climbing |
| `PLAYER_CLIMB_GRAB_PAD` | 4 px | extra grab width around climbable art |

`Player.climb_source` identifies the active climbable type: 0 = vine, 1 = ladder, 2 = rope.

---

## Animation

`player_animation.c` selects an `AnimState`, then maps it to a linear frame index on the 4-column sheet.

| State | Frames | ms/frame | First frame | Sheet row |
|-------|--------|----------|-------------|-----------|
| `ANIM_IDLE` | 4 | 150 | 0 | 0 |
| `ANIM_WALK` | 4 | 100 | 4 | 1 |
| `ANIM_JUMP` | 2 | 150 | 8 | 2 |
| `ANIM_FALL` | 1 | 200 | 12 | 3 |
| `ANIM_CLIMB` | 2 | 100 | 16 | 4 |

Walk animation starts only when horizontal velocity exceeds `WALK_ANIM_MIN_VX = 8.0f`; this avoids foot-shuffling during tiny acceleration/friction speeds.

---

## Hitbox

| Constant | Value | Effect |
|----------|-------|--------|
| `PLAYER_PHYS_PAD_X` | 15 px | left/right inset; physics width = 18 px |
| `PLAYER_PHYS_PAD_TOP` | 18 px | top inset |
| `PLAYER_FLOOR_SINK` | 16 px | bottom inset/sink; physics height = 14 px |

```c
r.x = (int)player->x + PLAYER_PHYS_PAD_X;
r.y = (int)player->y + PLAYER_PHYS_PAD_TOP;
r.w = player->w - 2 * PLAYER_PHYS_PAD_X;
r.h = player->h - PLAYER_PHYS_PAD_TOP - PLAYER_FLOOR_SINK;
```

Sprite analysis confirms the art envelope fits this inset: visible art is roughly 18×16 px inside the 48×48 frame.
