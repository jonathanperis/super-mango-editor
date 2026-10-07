# Entities & Hazards

<a id="home"></a>

---

Super Mango has six enemy types and seven hazard types. All are stored as fixed-size arrays inside `GameState`, filled from the level by `src/levels/level_loader.c`, then updated and rendered every frame. Updates run in fixed 1/60 s simulation steps, so every speed below (px/s, °/s) means the same on any display. Positions are in **logical pixels** (400×300 space).

Losing a life resets every enemy and hazard to its authored placement.

All enemies patrol between `patrol_x0` and `patrol_x1`; the range must be at least as wide as the enemy's sprite. The TOML `vx` is only the starting velocity: its sign picks the first direction, and after the first turn the enemy moves at its type's speed constant below.

---

## Enemies

### Spider

**File:** `src/entities/spider.c` / `spider.h`  
**Sprite:** `assets/sprites/entities/spider.png` — 192×48 px, 3 frames of 64×48 px  
**Behaviour:** Horizontal ground patrol. Walks back and forth between `patrol_x0` and `patrol_x1`. No gravity — stays on the ground floor. Reverses direction and flips sprite when it hits a patrol boundary, or when its art centre would move over a floor gap.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_SPIDERS` | 16 | Slots in the `GameState` array |
| `SPIDER_FRAMES` | 3 | Animation frames |
| `SPIDER_FRAME_W` | 64 | Width of one frame slot in px |
| `SPIDER_ART_W` | 25 | Width of visible art (cols 20–44) |
| `SPIDER_ART_H` | 10 | Height of visible art (rows 22–31) |
| `SPIDER_SPEED` | 50.0 | Walk speed in logical px/s |
| `SPIDER_FRAME_MS` | 150 | ms per animation frame |

**TOML placement:**
```toml
[[spiders]]
x          = 600.0
vx         = 50.0        # positive = starts moving right
patrol_x0  = 592.0
patrol_x1  = 750.0
frame_index = 0          # starting animation frame (0–2)
```

---

### Jumping Spider

**File:** `src/entities/jumping_spider.c` / `jumping_spider.h`  
**Sprite:** `assets/sprites/entities/jumping_spider.png`  
**Behaviour:** Like the spider (55 px/s, `JSPIDER_SPEED`) but leaps when its art centre reaches a floor gap: an upward impulse of −200 px/s under its own 600 px/s² gravity, with the attack sound when on screen. Normal spiders reverse at gaps; jumping spiders continue across them. Neither variant follows the player.

**TOML placement:**
```toml
[[jumping_spiders]]
x          = 130.0
vx         = 55.0
patrol_x0  = 46.0
patrol_x1  = 310.0
```

---

### Bird

**File:** `src/entities/bird.c` / `bird.h`; movement, sound, hitbox and render are shared with the faster bird in `src/entities/bird_variant.c`, tuned by a `BirdVariantSpec`  
**Sprite:** `assets/sprites/entities/bird.png` — 144×48 px, 3 frames of 48×48 px  
**Behaviour:** Slow sine-wave sky patrol. Flies horizontally while oscillating vertically around `base_y` using a sine curve. The wing-flap sound effect plays once per animation cycle with distance-based volume.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_BIRDS` | 16 | Slots in `GameState` |
| `BIRD_FRAMES` | 3 | Animation frames |
| `BIRD_FRAME_W` | 48 | Frame slot width in px |
| `BIRD_ART_W` | 15 | Visible art width (cols 17–31) |
| `BIRD_ART_H` | 14 | Visible art height (rows 17–30) |
| `BIRD_SPEED` | 45.0 | Horizontal speed in px/s |
| `BIRD_WAVE_AMP` | 20.0 | Sine-wave vertical amplitude in px |
| `BIRD_WAVE_FREQ` | 0.015 | Sine phase in radians per horizontal px |
| `BIRD_FRAME_MS` | 140 | ms per animation frame |

**TOML placement:**
```toml
[[birds]]
x          = 100.0
base_y     = 60.0    # vertical centre of the sine wave
vx         = 45.0
patrol_x0  = 0.0
patrol_x1  = 700.0
frame_index = 0
```

---

### Faster Bird

**File:** `src/entities/faster_bird.c` / `faster_bird.h`  
**Sprite:** `assets/sprites/entities/faster_bird.png`  
**Behaviour:** Faster sky patrol with a tighter wave, through the same `bird_variant.c` code. Same schema as `Bird` but uses `[[faster_birds]]` in TOML.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_FASTER_BIRDS` | 16 | Slots in `GameState` |
| `FBIRD_SPEED` | 80.0 | Horizontal speed in px/s (bird: 45) |
| `FBIRD_WAVE_AMP` | 15.0 | Sine-wave vertical amplitude in px |
| `FBIRD_WAVE_FREQ` | 0.025 | Sine phase in radians per horizontal px |
| `FBIRD_FRAME_MS` | 90 | ms per animation frame |

```toml
[[faster_birds]]
x          = 600.0
base_y     = 50.0
vx         = -80.0
patrol_x0  = 300.0
patrol_x1  = 1100.0
frame_index = 0
```

---

### Fish

**File:** `src/entities/fish.c` / `fish.h`  
**Sprite:** `assets/sprites/entities/fish.png` — 96×48 px, 2 frames of 48×48 px  
**Behaviour:** Patrols horizontally in the water lane at the bottom of the screen. Periodically performs a random upward jump (impulse −280 px/s) that can reach the player on the ground floor. Jump interval is randomised between 1.4 s and 3.0 s.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_FISH` | 16 | Slots in `GameState` |
| `FISH_FRAMES` | 2 | Animation frames |
| `FISH_FRAME_W` | 48 | Frame slot width in px |
| `FISH_SPEED` | 70.0 | Horizontal patrol speed in px/s |
| `FISH_JUMP_VY` | −280.0 | Upward jump impulse in px/s |
| `FISH_JUMP_MIN` | 1.4 | Minimum seconds between jumps |
| `FISH_JUMP_MAX` | 3.0 | Maximum seconds between jumps |
| `FISH_HITBOX_PAD_X` | 16 | Left/right inset for collision |
| `FISH_HITBOX_PAD_Y` | 13 | Top inset for collision |
| `FISH_FRAME_MS` | 120 | ms per animation frame |

```toml
[[fish]]
x          = 700.0
vx         = 70.0
patrol_x0  = 500.0
patrol_x1  = 950.0
```

---

### Faster Fish

**File:** `src/entities/faster_fish.c` / `faster_fish.h`  
**Sprite:** `assets/sprites/entities/faster_fish.png`  
**Behaviour:** Same as fish (one shared implementation in `fish.c`, tuned by a `FishSpec`) but faster and jumpier: 120 px/s (`FFISH_SPEED`), a −420 px/s jump (`FFISH_JUMP_VY`) every 1.0–2.2 s, and 100 ms animation frames. Uses `[[faster_fish]]` in TOML.

```toml
[[faster_fish]]
x          = 1100.0
vx         = 120.0
patrol_x0  = 900.0
patrol_x1  = 1400.0
```

---

## Hazards

Enemy and active hazard hitboxes deal **1 heart of damage** on contact with knockback, subject to hurt immunity. `game_collide()` in `src/collision/game_collision.c` routes hits through `apply_damage()` in `src/collision/collision_damage.c`; waiting flames have no active damage hitbox. Collision uses each hazard's position after this step's update.

When a level has no authored `[[checkpoints]]`, automatic screen-edge checkpoints avoid floor gaps and the static hazards on this page (spike rows, spike platforms, blue and fire flames). Moving hazards and enemies are not considered; see [Authored Checkpoints](../level-design/#authored-checkpoints).

---

### Spike Row

**File:** `src/hazards/spike.c` / `spike.h`  
**Sprite:** `assets/sprites/hazards/spike.png` — 16×16 px per tile  
**Behaviour:** Static horizontal strip of spike tiles sitting on the ground floor. No movement or animation. Damages the player on any overlap.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_SPIKE_ROWS` | 16 | Rows in `GameState` |
| `MAX_SPIKE_TILES` | 16 | Max tiles per row |
| `SPIKE_TILE_W` | 16 | Width of one spike tile in px |
| `SPIKE_TILE_H` | 16 | Height of one spike tile in px |

```toml
[[spike_rows]]
x     = 780.0   # left edge of the strip
count = 4       # number of tiles
```

---

### Spike Block

**File:** `src/hazards/spike_block.c` / `spike_block.h`  
**Sprite:** `assets/sprites/hazards/spike_block.png`  
**Behaviour:** A rotating hazard (24×24 px, 360°/s spin) that travels along a `Rail` path. References a rail by index and can be given an initial offset and speed; 1.5 (slow), 3.0 (normal) and 6.0 (fast) tiles/s cover the useful pacing, and any speed must be above 0 and at most `MAX_RAIL_SPEED` (30). On a closed loop it circulates; on an open rail it bounces at a capped end. On an open rail without an end cap it waits at the start until the camera reaches it, then flies off the far end and falls. The player is pushed on contact.

```toml
[[spike_blocks]]
rail_index = 0      # 0-based index into the [[rails]] list
t_offset   = 0.0    # starting position on the rail (0.0 = first tile)
speed      = 1.5    # traversal speed in tiles/s
```

---

### Spike Platform

**File:** `src/hazards/spike_platform.c` / `spike_platform.h`  
**Sprite:** `assets/sprites/hazards/spike_platform.png`  
**Behaviour:** Elevated static surface tiled across `tile_count` 16 px pieces (1–16). The player can land on the spiked top, which damages them (the hitbox reaches 2 px above the surface so a standing player always overlaps it), and touching the sides also hurts. The smooth underside is a solid ceiling that deals no damage.

```toml
[[spike_platforms]]
x          = 370.0
y          = 200.0   # top edge in logical pixels
tile_count = 3
```

---

### Circular Saw

**File:** `src/hazards/circular_saw.c` / `circular_saw.h`  
**Sprite:** `assets/sprites/hazards/circular_saw.png` — 32×32 px  
**Behaviour:** Spins continuously and patrols a horizontal line. With `y = 0` it uses the default height, rolling on top of a 2-tile pillar (y 140); a non-zero `y` is its top edge. Does not ride a rail — it bounces between `patrol_x0` and `patrol_x1`. Faster than the player's walk speed. Pushes the player on contact.

| Constant | Value | Description |
|----------|-------|-------------|
| `SAW_FRAME_W/H` | 32 | Sprite dimensions in px |
| `SAW_SPIN_DEG_PER_SEC` | 720.0 | Rotation speed (2 full rotations/s) |
| `SAW_PATROL_SPEED` | 180.0 | Horizontal patrol speed in px/s |
| `SAW_PUSH_SPEED` | 220.0 | Push impulse magnitude on contact |
| `SAW_PUSH_VY` | −150.0 | Upward component of push |

```toml
[[circular_saws]]
x          = 1350.0
y          = 0.0        # 0 = default height (y 140)
patrol_x0  = 1350.0
patrol_x1  = 1446.0
direction  = 1          # 1 = starts right, -1 = starts left
```

---

### Axe Trap

**File:** `src/hazards/axe_trap.c` / `axe_trap.h`  
**Sprite:** `assets/sprites/hazards/axe_trap.png` — 48×64 px  
**Behaviour:** Swinging or spinning axe. The pivot sits at the horizontal centre of the 48 px column starting at `pillar_x`. With `y = 0` the pivot uses a fixed default height (y 124, the top of a 3-tile pillar); it is not measured from a pillar, so set `y` for other pillar heights. Two modes:

- **PENDULUM** — sinusoidal swing from −60° to +60° over a 2 s cycle. SFX plays at each extreme.
- **SPIN** — continuous 360° clockwise rotation at 180°/s. SFX plays each full rotation.

Collision uses a 28×28 px box centred on the blade, whose centre is rotated around the pivot with the current angle (`axe_trap_get_hitbox()`).

| Constant | Value | Description |
|----------|-------|-------------|
| `AXE_FRAME_W/H` | 48 / 64 | Sprite dimensions |
| `AXE_SWING_AMPLITUDE` | 60.0° | Max angle from vertical |
| `AXE_SWING_PERIOD` | 2.0 s | Full pendulum cycle duration |
| `AXE_SPIN_SPEED` | 180.0°/s | Full-rotation variant speed |

```toml
[[axe_traps]]
pillar_x = 256.0    # left x of the pillar column
y        = 0.0      # pivot y; 0 = default (y 124)
mode     = "PENDULUM"   # or "SPIN"
```

---

### Blue Flame

**File:** `src/hazards/blue_flame.c` / `blue_flame.h`  
**Sprite:** `assets/sprites/hazards/blue_flame.png` — 96×48 px, 2 frames of 48×48 px  
**Behaviour:** Erupts from a manually placed floor-gap position in four phases:

| Phase | Duration | Description |
|-------|----------|-------------|
| `WAITING` | 1.5 s | Hidden below the floor, counting down |
| `RISING` | Until apex (y = 60) | Launches at −550 px/s, decelerates at 800 px/s² |
| `FLIPPING` | 0.12 s | Rotates 180° at the apex |
| `FALLING` | Until below floor | Descends upside-down, accelerating with gravity |

Blue flames are placed explicitly with `[[blue_flames]]`. `x` is the gap's left edge and normally matches a `floor_gaps` entry; the flame is centred in the 32 px opening. `x = 0` (the gap at the left edge of the world) works like any other gap.

```toml
[[blue_flames]]
x = 192.0   # left edge of the sea gap
```

| Constant | Value | Description |
|----------|-------|-------------|
| `BLUE_FLAME_LAUNCH_VY` | −550.0 | Initial upward impulse in px/s |
| `BLUE_FLAME_APEX_Y` | 60.0 | Highest point in logical pixels |
| `BLUE_FLAME_FLIP_DURATION` | 0.12 s | Time to rotate 180° at apex |
| `BLUE_FLAME_WAIT_DURATION` | 1.5 s | Idle time between eruptions |
| `MAX_BLUE_FLAMES` | 16 | Maximum blue flame instances per level |
| `MAX_FIRE_FLAMES` | 16 | Maximum fire flame instances per level |

---

### Fire Flame

**File:** `src/hazards/blue_flame.c` / `blue_flame.h` (fire variant uses the shared blue-flame runtime type)  
**Sprite:** `assets/sprites/hazards/fire_flame.png`  
**Behaviour:** Same eruption cycle as the blue flame but with a fire-colored sprite. Used in volcanic or lava-themed levels. Shares the same phase constants, but stores placement in `fire_flames[MAX_FIRE_FLAMES]`.

---

## Collision Architecture

All entity–player collision uses **AABB (axis-aligned bounding box)** overlap tests on integer `IntRect`s. Entity hitboxes are inset from the sprite frame to match visible art bounds only — transparent padding is excluded.

```c
/* src/shared/geometry.h — half-open boxes: [x, x+w) */
static inline int rect_intersects(const IntRect *a, const IntRect *b)
{
    return a && b && a->w > 0 && a->h > 0 && b->w > 0 && b->h > 0 &&
        (int64_t)a->x < (int64_t)b->x + b->w &&
        (int64_t)b->x < (int64_t)a->x + a->w &&
        (int64_t)a->y < (int64_t)b->y + b->h &&
        (int64_t)b->y < (int64_t)a->y + a->h;
}
```

Boxes that merely touch an edge do not overlap, and the 64-bit widening keeps the additions from overflowing. Each module exposes a hitbox function, such as `bird_get_hitbox()`, `fish_get_hitbox()` or `circular_saw_get_hitbox()`; `game_collision.c` builds spider hitboxes with `spider_build_hitbox()`. See each entity's header for `ART_X`, `ART_W`, `ART_Y`, `ART_H`, and `HITBOX_PAD_*` constants.

---

## Adding a New Enemy or Hazard

See the [Developer Guide](../developer-guide/) for the full entity template and step-by-step checklist.
