# Entities & Hazards

[← Home](index.md)

---

Super Mango has six enemy types and seven hazard types. All are stored as fixed-size arrays inside `GameState` and managed via the standard `init / update / render` lifecycle. Positions are in **logical pixels** (400×300 space).

---

## Enemies

### Spider

**File:** `src/entities/spider.c` / `spider.h`  
**Sprite:** `assets/sprites/entities/spider.png` — 192×48 px, 3 frames of 64×48 px  
**Behaviour:** Horizontal ground patrol. Walks back and forth between `patrol_x0` and `patrol_x1`. No gravity — stays on the ground floor. Reverses direction and flips sprite when it hits a patrol boundary.

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
[spiders](LspidersE.md)
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
**Behaviour:** Like the spider but can leap across sea gaps. Detects approaching floor gaps and jumps before reaching them, allowing it to follow the player across openings where a normal spider would fall.

**TOML placement:**
```toml
[jumping_spiders](Ljumping_spidersE.md)
x          = 130.0
vx         = 55.0
patrol_x0  = 46.0
patrol_x1  = 310.0
```

---

### Bird

**File:** `src/entities/bird.c` / `bird.h`  
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
| `BIRD_WAVE_FREQ` | 0.015 | Sine cycles per horizontal px of travel |
| `BIRD_FRAME_MS` | 140 | ms per animation frame |

**TOML placement:**
```toml
[birds](LbirdsE.md)
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
**Behaviour:** Aggressive sky patrol with higher speed and a tighter wave. Same schema as `Bird` but uses `[faster_birds](Lfaster_birdsE.md)` in TOML. Typical `vx` is 70–100 px/s vs. the bird's 45 px/s.

```toml
[faster_birds](Lfaster_birdsE.md)
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
[fish](LfishE.md)
x          = 700.0
vx         = 70.0
patrol_x0  = 500.0
patrol_x1  = 950.0
```

---

### Faster Fish

**File:** `src/entities/faster_fish.c` / `faster_fish.h`  
**Behaviour:** Same as fish but patrols at 120 px/s and jumps more frequently. Uses `[faster_fish](Lfaster_fishE.md)` in TOML.

```toml
[faster_fish](Lfaster_fishE.md)
x          = 1100.0
vx         = 120.0
patrol_x0  = 900.0
patrol_x1  = 1400.0
```

---

## Hazards

All hazards deal **1 heart of damage** on contact with knockback (same `apply_damage` path in `player.c`).

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
[spike_rows](Lspike_rowsE.md)
x     = 780.0   # left edge of the strip
count = 4       # number of tiles
```

---

### Spike Block

**File:** `src/hazards/spike_block.c` / `spike_block.h`  
**Sprite:** `assets/sprites/hazards/spike_block.png`  
**Behaviour:** A rotating hazard that travels along a `Rail` path. References a rail by index and can be given an initial offset and speed. Visually rotates as it travels. The player is pushed on contact.

```toml
[spike_blocks](Lspike_blocksE.md)
rail_index = 0      # 0-based index into the [rails](LrailsE.md) list
t_offset   = 0.0    # starting position on the rail (0.0 = first tile)
speed      = 1.5    # traversal speed in tiles/s
```

---

### Spike Platform

**File:** `src/hazards/spike_platform.c` / `spike_platform.h`  
**Sprite:** `assets/sprites/hazards/spike_platform.png`  
**Behaviour:** Elevated static surface tiled across `tile_count` units. The player is damaged when landing on the top surface or touching the sides.

```toml
[spike_platforms](Lspike_platformsE.md)
x          = 370.0
y          = 200.0   # top edge in logical pixels
tile_count = 3
```

---

### Circular Saw

**File:** `src/hazards/circular_saw.c` / `circular_saw.h`  
**Sprite:** `assets/sprites/hazards/circular_saw.png` — 32×32 px  
**Behaviour:** Spins continuously and patrols a horizontal line at ground level. Does not ride a rail — it bounces between `patrol_x0` and `patrol_x1`. Faster than the player's walk speed. Pushes the player on contact.

| Constant | Value | Description |
|----------|-------|-------------|
| `SAW_FRAME_W/H` | 32 | Sprite dimensions in px |
| `SAW_SPIN_DEG_PER_SEC` | 720.0 | Rotation speed (2 full rotations/s) |
| `SAW_PATROL_SPEED` | 180.0 | Horizontal patrol speed in px/s |
| `SAW_PUSH_SPEED` | 220.0 | Push impulse magnitude on contact |
| `SAW_PUSH_VY` | −150.0 | Upward component of push |

```toml
[circular_saws](Lcircular_sawsE.md)
x          = 1350.0
y          = 0.0        # engine snaps to floor level
patrol_x0  = 1350.0
patrol_x1  = 1446.0
direction  = 1          # 1 = starts right, -1 = starts left
```

---

### Axe Trap

**File:** `src/hazards/axe_trap.c` / `axe_trap.h`  
**Sprite:** `assets/sprites/hazards/axe_trap.png` — 48×64 px  
**Behaviour:** Swinging or spinning axe mounted at the top centre of a platform pillar. Two modes:

- **PENDULUM** — sinusoidal swing from −60° to +60° over a 2 s cycle. SFX plays at each extreme.
- **SPIN** — continuous 360° clockwise rotation at 180°/s. SFX plays each full rotation.

Collision uses the full rotated bounding box of the blade region.

| Constant | Value | Description |
|----------|-------|-------------|
| `AXE_FRAME_W/H` | 48 / 64 | Sprite dimensions |
| `AXE_SWING_AMPLITUDE` | 60.0° | Max angle from vertical |
| `AXE_SWING_PERIOD` | 2.0 s | Full pendulum cycle duration |
| `AXE_SPIN_SPEED` | 180.0°/s | Full-rotation variant speed |

```toml
[axe_traps](Laxe_trapsE.md)
pillar_x = 256.0    # centre x of the pillar column
y        = 0.0      # pivot y (engine computes from pillar height)
mode     = "PENDULUM"   # or "SPIN"
```

---

### Blue Flame

**File:** `src/hazards/blue_flame.c` / `blue_flame.h`  
**Sprite:** `assets/sprites/hazards/blue_flame.png` — 96×48 px, 2 frames of 48×48 px  
**Behaviour:** Erupts from a sea gap in four phases:

| Phase | Duration | Description |
|-------|----------|-------------|
| `WAITING` | 1.5 s | Hidden below the floor, counting down |
| `RISING` | Until apex (y = 60) | Launches at −550 px/s, decelerates at 800 px/s² |
| `FLIPPING` | 0.12 s | Rotates 180° at the apex |
| `FALLING` | Until below floor | Descends upside-down, accelerating with gravity |

Blue flames are spawned automatically for each entry in `floor_gaps`. They can also be manually added via `[blue_flames](Lblue_flamesE.md)` to override position.

```toml
[blue_flames](Lblue_flamesE.md)
x = 192.0   # world-space x of the sea gap
```

| Constant | Value | Description |
|----------|-------|-------------|
| `BLUE_FLAME_LAUNCH_VY` | −550.0 | Initial upward impulse in px/s |
| `BLUE_FLAME_APEX_Y` | 60.0 | Highest point in logical pixels |
| `BLUE_FLAME_FLIP_DURATION` | 0.12 s | Time to rotate 180° at apex |
| `BLUE_FLAME_WAIT_DURATION` | 1.5 s | Idle time between eruptions |

---

### Fire Flame

**File:** `src/hazards/fire_flame.c` / `fire_flame.h`  
**Sprite:** `assets/sprites/hazards/fire_flame.png`  
**Behaviour:** Same eruption cycle as the blue flame but with a fire-colored sprite. Used in volcanic or lava-themed levels. Shares the same phase constants.

---

## Collision Architecture

All entity–player collision uses **AABB (axis-aligned bounding box)** overlap tests. Entity hitboxes are inset from the sprite frame to match visible art bounds only — transparent padding is excluded.

```c
/* Example: is the player overlapping a coin? */
static int aabb_overlap(float ax, float ay, int aw, int ah,
                         float bx, float by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}
```

For entities with non-rectangular visible art (birds, fish, spiders), `get_hitbox()` functions return a pre-computed inset `SDL_Rect` that matches the actual art bounds. See each entity's header for `ART_X`, `ART_W`, `ART_Y`, `ART_H`, and `HITBOX_PAD_*` constants.

---

## Adding a New Enemy or Hazard

See the [Developer Guide](developer_guide) for the full entity template and step-by-step checklist.
