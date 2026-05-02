# Collectibles & Surfaces

[← Home](index.md)

---

Collectibles are items the player can pick up. Surfaces are interactive terrain the player can stand on, jump from, or climb. All positions are in **logical pixels** (400×300 space).

---

## Collectibles

### Coin

**File:** `src/collectibles/coin.c` / `coin.h`  
**Sprite:** `assets/sprites/collectibles/coin.png` — 16×16 px display size  
**Pickup:** AABB overlap with player. Plays `snd_coin` on collection.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_COINS` | 64 | Coin slots in `GameState` |
| `COIN_DISPLAY_W/H` | 16 | Render size in logical px |
| `COIN_SCORE` | 100 | Points awarded per coin |
| `SCORE_PER_LIFE` | 1000 | Score threshold for a bonus life |

```toml
[coins](LcoinsE.md)
x = 46.0
y = 236.0   # top edge in logical pixels
```

Every 1000 points (10 coins) the player earns a bonus life. The score threshold is configurable via `score_per_life` in the level file.

---

### Star Yellow

**File:** `src/collectibles/star_yellow.c` / `star_yellow.h`  
**Sprite:** `assets/sprites/collectibles/star_yellow.png` — 16×16 px display size  
**Pickup:** AABB overlap. Restores 1 heart (up to `MAX_HEARTS`). No score awarded.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_STAR_YELLOWS` | 16 | Slots in `GameState` |
| `STAR_YELLOW_DISPLAY_W/H` | 16 | Render size in logical px |

```toml
[star_yellows](Lstar_yellowsE.md)
x = 272.0
y = 108.0
```

---

### Star Green

**File:** `src/collectibles/star_green.h`  
**Sprite:** `assets/sprites/collectibles/star_green.png` — 16×16 px  
**Pickup:** Same as star yellow — restores 1 heart.

```toml
[star_greens](Lstar_greensE.md)
x = 500.0
y = 80.0
```

---

### Star Red

**File:** `src/collectibles/star_red.h`  
**Sprite:** `assets/sprites/collectibles/star_red.png` — 16×16 px  
**Pickup:** Same as star yellow — restores 1 heart.

```toml
[star_reds](Lstar_redsE.md)
x = 800.0
y = 100.0
```

---

### Last Star

**File:** `src/collectibles/last_star.c` / `last_star.h`  
**Sprite:** `assets/sprites/screens/hud_coins.png` (reuses the HUD star icon)  
**Display size:** 24×24 px  
**Pickup:** Collecting it sets `collected = 1` and triggers the level-complete event. Only one instance per level, defined with `[last_star]` (not `[last_star](Llast_starE.md)`).

| Constant | Value | Description |
|----------|-------|-------------|
| `LAST_STAR_DISPLAY_W/H` | 24 | Render size in logical px |

```toml
[last_star]
x = 1492.0
y = 100.0
```

---

## Surfaces

### Platform (Ground Pillar)

**File:** `src/surfaces/platform.c` / `platform.h`  
**Sprite:** `assets/sprites/surfaces/Platform.png` — 48×48 tile, 9-slice rendered  
**Behaviour:** Static ground pillar. The player can land on the top surface. Pillars are positioned on the floor and extend upward. Rendered before the floor so the pillar base sinks into the ground naturally.

```toml
[platforms](LplatformsE.md)
x           = 80.0   # left edge in logical pixels
tile_height = 2      # height in 48px tiles (1–3)
tile_width  = 1      # width in 48px tiles (usually 1)
```

Top surface Y for a pillar: `FLOOR_Y − (tile_height × TILE_SIZE)` = `252 − (h × 48)`.

| `tile_height` | Top Y | Typical use |
|---------------|-------|-------------|
| 1 | 204 | Step / obstacle |
| 2 | 156 | Standard platform — medium bouncepads clear this |
| 3 | 108 | Tall — only reachable via high bouncepad or vine |

---

### Float Platform

**File:** `src/surfaces/float_platform.c` / `float_platform.h`  
**Sprite:** `assets/sprites/surfaces/float_platform.png` — 48×16 px, 3-slice (left cap | centre fill | right cap)  
**Behaviour:** Hovering one-way surface. Player lands on the top face only (one-way collision — can jump through from below). Three modes:

| Mode | Behaviour |
|------|-----------|
| `STATIC` | Fixed position, never moves |
| `CRUMBLE` | Begins falling after player stands on it for 0.75 s |
| `RAIL` | Travels along a rail path at constant speed |

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_FLOAT_PLATFORMS` | 16 | Slots in `GameState` |
| `FLOAT_PLATFORM_PIECE_W` | 16 | Width of each 3-slice piece |
| `FLOAT_PLATFORM_H` | 16 | Platform height in px |
| `CRUMBLE_STAND_LIMIT` | 0.75 s | Time before crumble fall starts |
| `CRUMBLE_FALL_GRAVITY` | 250 px/s² | Downward acceleration during fall |

```toml
[float_platforms](Lfloat_platformsE.md)
mode       = "STATIC"   # "STATIC" | "CRUMBLE" | "RAIL"
x          = 172.0
y          = 200.0
tile_count = 4          # width in 16px pieces
rail_index = 0          # RAIL mode only: index into [rails](LrailsE.md)
t_offset   = 0.0        # RAIL mode only: starting position on rail
speed      = 0.0        # RAIL mode only: traversal speed in tiles/s
```

---

### Bridge

**File:** `src/surfaces/bridge.c` / `bridge.h`  
**Sprite:** `assets/sprites/surfaces/bridge.png` — 16×16 px brick tile  
**Behaviour:** Tiled crumble walkway. Bricks fall individually when the player walks over them, creating a time-limited path. After falling they respawn when the player moves away.

```toml
[bridges](LbridgesE.md)
x           = 1350.0
y           = 172.0
brick_count = 8   # number of 16×16 brick tiles
```

---

### Bouncepad

**File:** `src/surfaces/bouncepad.c` / `bouncepad.h`  
**Sprites:** `bouncepad_small.png`, `bouncepad_medium.png` (wood), `bouncepad_high.png`  
**Sheet:** 144×48 px, 3 columns × 1 row — Frame 0: extended, Frame 1: mid-compress, Frame 2: compressed (default idle state)  
**Behaviour:** Spring pad that launches the player upward on landing. Plays a 3-frame squash/release animation (2→1→0, 80 ms/frame) then resets to idle.

| Constant | Value | Description |
|----------|-------|-------------|
| `BOUNCEPAD_W/H` | 48 | Display size in logical px |
| `BOUNCEPAD_VY_SMALL` | −380.0 | Launch impulse for green pad |
| `BOUNCEPAD_VY_MEDIUM` | −536.25 | Launch impulse for wood pad |
| `BOUNCEPAD_VY_HIGH` | −700.0 | Launch impulse for red pad |
| `BOUNCEPAD_FRAME_MS` | 80 | ms per animation frame during release |
| `BOUNCEPAD_SRC_Y` | 14 | Art starts at row 14 (transparent above) |
| `BOUNCEPAD_SRC_H` | 18 | Art height = rows 14–31 |
| `BOUNCEPAD_ART_X` | 16 | Art starts at col 16 (transparent outside) |
| `BOUNCEPAD_ART_W` | 16 | Hitbox width = cols 16–31 |

```toml
[bouncepads_small](Lbouncepads_smallE.md)
x         = 734.0
launch_vy = -380.0
pad_type  = "GREEN"

[bouncepads_medium](Lbouncepads_mediumE.md)
x         = 310.0
launch_vy = -536.2
pad_type  = "WOOD"

[bouncepads_high](Lbouncepads_highE.md)
x         = 1420.0
launch_vy = -700.0
pad_type  = "RED"
```

---

### Rail

**File:** `src/surfaces/rail.c` / `rail.h`  
**Sprite:** `assets/sprites/surfaces/rail.png` — 64×64 px, 4×4 grid of 16×16 bitmask tiles  
**Behaviour:** A path of interconnected tiles that spike blocks and float platforms ride along. Each tile has a bitmask of connection directions (N/E/S/W) that drives the correct sprite selection. Objects riding a rail store a float `t ∈ [0, tile_count)` and call `rail_get_world_pos()` each frame.

| Constant | Value | Description |
|----------|-------|-------------|
| `RAIL_N/E/S/W` | 1/2/4/8 | Connection bitmask flags |
| `RAIL_TILE_W/H` | 16 | Tile size in the sprite sheet |
| `MAX_RAIL_TILES` | 128 | Max tiles per Rail instance |
| `MAX_RAILS` | 16 | Rail instances in `GameState` |

```toml
[rails](LrailsE.md)
layout  = "RECT"   # "RECT" = closed rectangle | "HORIZ" = open horizontal line
x       = 444      # top-left tile x
y       = 35       # top-left tile y
w       = 10       # width in tiles
h       = 6        # height in tiles
end_cap = 0        # 0 = open end (rider detaches), 1 = bouncing end
```

---

### Vine

**File:** `src/surfaces/vine.c` / `vine.h`  
**Sprite:** `assets/sprites/surfaces/vine.png` — 16×48 px per tile  
**Behaviour:** Climbable vertical surface. The player enters by touching the vine and pressing Up or Down. Press Left/Right to detach and jump away.

```toml
[vines](LvinesE.md)
x          = 88.0
y          = 172.0   # top tile y in logical pixels
tile_count = 2       # height in tiles (each tile = 16 px wide × 48 px tall)
```

---

### Ladder

**File:** `src/surfaces/ladder.c` / `ladder.h`  
**Sprite:** `assets/sprites/surfaces/ladder.png`  
**Behaviour:** Climbable ladder. The player enters by pressing Up at the base. Climbing is done with Up/Down. Jump or move left/right to exit.

```toml
[ladders](LladdersE.md)
x          = 1552.0
y          = 0.0       # top tile y
tile_count = 30        # height in tiles
```

---

### Rope

**File:** `src/surfaces/rope.c` / `rope.h`  
**Sprite:** `assets/sprites/surfaces/rope.png`  
**Behaviour:** Climbable rope. Same interaction model as the vine — touch and press Up to grab, Up/Down to climb, Left/Right to swing off.

```toml
[ropes](LropesE.md)
x          = 460.0
y          = 172.0
tile_count = 1
```
