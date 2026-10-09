# Collectibles & Surfaces

<a id="home"></a>

---

Collectibles are items the player can pick up. Surfaces are interactive terrain the player can stand on, jump from, or climb. All positions are in **logical pixels** (400×300 space).

---

## Collectibles

### Coin

**File:** `src/collectibles/coin.c` / `coin.h`  
**Sprite:** `assets/sprites/collectibles/coin.png` — 16×16 px display size  
**Pickup:** AABB overlap with player. Plays `gs->audio.coin` on collection.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_COINS` | 64 | Coin slots in `GameState` |
| `COIN_DISPLAY_W/H` | 16 | Render size in logical px |
| `COIN_SCORE` | 100 | Points awarded per coin |
| `SCORE_PER_LIFE` | 1000 | Score threshold for a bonus life |

```toml
[[coins]]
x = 46.0
y = 236.0   # top edge in logical pixels
```

With defaults, every 1000 points (10 coins) earns a bonus life. Both `coin_score` and `score_per_life` are configurable in the level file.

**Collected coins stay gone for the whole attempt.** Losing a life respawns enemies, hazards, surfaces and stars, but not coins: score and the bonus-life threshold survive a death, so returning coins would let a player farm points and lives by dying on purpose. Coins come back only for a fresh attempt — Retry after game over, Replay, or loading a level — and the completion summary counts every coin collected during the attempt. Health stars and the last star *do* respawn after a life loss: they award no score, so each life simply meets the healing the level was designed with.

---

### Star Yellow

**File:** `src/collectibles/health_star.c` / `health_star.h` (shared by all three star colours)  
**Sprite:** `assets/sprites/collectibles/star_yellow.png` — 16×16 px display size  
**Pickup:** AABB overlap. Restores 1 heart (up to `MAX_HEARTS`, 3). No score awarded.

The three colours are one collectible: a single `HealthStar` struct, `health_stars_render()` and `health_star_get_hitbox()`, and one `collect_health_stars()` loop in `src/collision/game_collision.c`. The level file and `GameState` still keep a separate array and texture per colour.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_STAR_YELLOWS` | 16 | Slots in `GameState` |
| `HEALTH_STAR_DISPLAY_W/H` | 16 | Render size of every star colour in logical px |

```toml
[[star_yellows]]
x = 272.0
y = 108.0
```

---

### Star Green

**File:** `src/collectibles/health_star.c` / `health_star.h` (same `HealthStar` module as yellow; `MAX_STAR_GREENS` 16)  
**Sprite:** `assets/sprites/collectibles/star_green.png` — 16×16 px  
**Pickup:** Same as star yellow — restores 1 heart.

```toml
[[star_greens]]
x = 500.0
y = 80.0
```

---

### Star Red

**File:** `src/collectibles/health_star.c` / `health_star.h` (same `HealthStar` module as yellow; `MAX_STAR_REDS` 16)  
**Sprite:** `assets/sprites/collectibles/star_red.png` — 16×16 px  
**Pickup:** Same as star yellow — restores 1 heart.

```toml
[[star_reds]]
x = 800.0
y = 100.0
```

---

### Last Star

**File:** `src/collectibles/last_star.c` / `last_star.h`  
**Sprite:** `assets/sprites/collectibles/last_star.png`  
**Display size:** 24×24 px  
**Pickup:** Collecting it sets `collected = 1`, snapshots the level-completion summary, and shows the completion overlay. With `next_phase`, terminal actions are Next Level, Replay, Level Select, and Exit; otherwise they are Replay, Level Select, and Exit. Up/Down or D-pad changes focus, Enter/Space/Start (or A) confirms, and Esc/Back (or B) exits. Only one instance per level, defined with `[last_star]`.

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
**Sprite:** `assets/sprites/levels/grass_platform.png` by default, or the pillar's own `tile_path` — 48×48 tile, 9-slice rendered  
**Behaviour:** Static ground pillar. The player can land on the top surface. Pillars are positioned on the floor and extend upward, sunk 16 px into the ground. Rendered before the floor so the pillar base sinks into the ground naturally.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_PLATFORMS` | 32 | Pillar slots per level |

```toml
[[platforms]]
x           = 80.0   # left edge in logical pixels
tile_height = 2      # height in 48px tiles (1–5)
tile_width  = 1      # width in 48px tiles (0 or omitted = 1)
```

Top surface Y for a pillar: `FLOOR_Y − (tile_height × TILE_SIZE) + 16` = `268 − (h × 48)`.

| `tile_height` | Top Y | Typical use |
|---------------|-------|-------------|
| 1 | 220 | Step / obstacle |
| 2 | 172 | Standard platform |
| 3 | 124 | Tall — use a bouncepad, climbable or intermediate ledge |
| 4 | 76 | Very tall |
| 5 | 28 | Maximum height |

---

### Float Platform

**File:** `src/surfaces/float_platform.c` / `float_platform.h`  
**Sprite:** `assets/sprites/surfaces/float_platform.png` — 48×16 px, 3-slice (left cap | centre fill | right cap)  
**Behaviour:** Hovering one-way surface. Player lands on the top face only (one-way collision — can jump through from below). Three modes:

| Mode | Behaviour |
|------|-----------|
| `STATIC` | Fixed position, never moves |
| `CRUMBLE` | Begins falling after the player stands on it for 0.75 s without stepping off (stepping off resets the timer); it reappears when the level resets after a life loss |
| `RAIL` | Travels along a rail path at constant speed, carrying the player sideways and up or down with it; on an open rail it bounces at both ends and never detaches |

Landing on a float platform is tested against the platform's own movement: the player lands when their feet were at or above where its top was before its last move, and are now at or below where it is. The player moves before the platforms in each step, so this is what lets a platform that rises into falling feet catch them instead of slipping under them. A rider whose platform moved down was carried down with it; a small stay-on tolerance keeps them standing.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_FLOAT_PLATFORMS` | 16 | Slots in `GameState` |
| `FLOAT_PLATFORM_PIECE_W` | 16 | Width of each 3-slice piece |
| `FLOAT_PLATFORM_H` | 16 | Platform height in px |
| `CRUMBLE_STAND_LIMIT` | 0.75 s | Time before crumble fall starts |
| `CRUMBLE_FALL_GRAVITY` | 250 px/s² | Downward acceleration during fall |

```toml
[[float_platforms]]
mode       = "STATIC"   # "STATIC" | "CRUMBLE" | "RAIL"
x          = 172.0
y          = 200.0
tile_count = 4          # width in 16px pieces (1–16)
rail_index = 0          # RAIL mode only: index into [[rails]]
t_offset   = 0.0        # RAIL mode only: starting position on rail
speed      = 0.0        # RAIL mode only: traversal speed in tiles/s
```

---

### Bridge

**File:** `src/surfaces/bridge.c` / `bridge.h`  
**Sprite:** `assets/sprites/surfaces/bridge.png` — 16×16 px brick tile  
**Behaviour:** Tiled crumble walkway. Each brick under the player's centre starts its own timer on first contact and falls `BRIDGE_FALL_DELAY` later, so only the bricks the player actually steps on drop, creating a time-limited path. A brick counts as stepped on only when the player's landing test lands them on that bridge, so standing on another surface beside or just below it never starts the timer. Fallen bricks stay gone until the level resets after a life loss (or a fresh attempt).

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_BRIDGES` | 16 | Bridge slots in `GameState` |
| `MAX_BRIDGE_BRICKS` | 16 | Maximum bricks in one bridge |
| `BRIDGE_FALL_DELAY` | 0.2 s | Delay between touching a brick and its fall |

Bricks do not cascade on their own: each brick falls only after the player steps on it.

```toml
[[bridges]]
x           = 1350.0
y           = 172.0
brick_count = 8   # number of 16×16 brick tiles (1–16)
```

---

### Bouncepad

**File:** `src/surfaces/bouncepad.c` / `bouncepad.h`  
**Sprites:** `bouncepad_small.png`, `bouncepad_medium.png` (wood), `bouncepad_high.png`  
**Sheet:** 144×48 px, 3 columns × 1 row — Frame 0: extended, Frame 1: mid-compress, Frame 2: compressed (default idle state)  
**Behaviour:** Spring pad that launches the player upward on landing. Plays a 3-frame squash/release animation (2→1→0, 80 ms/frame) then resets to idle. The small, medium and high pads stay in their three `GameState` arrays; `player_update` receives them as `BouncepadList` views (pointer + count) and reports the landed pad as a flat index across the lists, so nothing is copied per frame.

| Constant | Value | Description |
|----------|-------|-------------|
| `BOUNCEPAD_W/H` | 48 | Display size in logical px |
| `BOUNCEPAD_VY_SMALL` | −380.0 | Usual launch impulse for green pad (the loader uses each pad's authored `launch_vy` as written; it must be `JUMP_VY` (−325) or stronger) |
| `BOUNCEPAD_VY_MEDIUM` | −536.25 | Launch impulse for wood pad |
| `BOUNCEPAD_VY_HIGH` | −700.0 | Launch impulse for red pad |
| `BOUNCEPAD_FRAME_MS` | 80 | ms per animation frame during release |
| `BOUNCEPAD_SRC_Y` | 14 | Art starts at row 14 (transparent above) |
| `BOUNCEPAD_SRC_H` | 18 | Art height = rows 14–31 |
| `BOUNCEPAD_ART_X` | 16 | Art starts at col 16 (transparent outside) |
| `BOUNCEPAD_ART_W` | 16 | Hitbox width = cols 16–31 |
| `MAX_BOUNCEPADS_SMALL` | 16 | Small bouncepad slots |
| `MAX_BOUNCEPADS_MEDIUM` | 16 | Medium bouncepad slots |
| `MAX_BOUNCEPADS_HIGH` | 16 | High bouncepad slots |

```toml
[[bouncepads_small]]
x         = 734.0
launch_vy = -380.0
pad_type  = "GREEN"

[[bouncepads_medium]]
x         = 310.0
launch_vy = -536.2
pad_type  = "WOOD"

[[bouncepads_high]]
x         = 1420.0
launch_vy = -700.0
pad_type  = "RED"
```

---

### Rail

**File:** `src/surfaces/rail.c` / `rail.h`  
**Sprite:** `assets/sprites/surfaces/rail.png` — 64×64 px, 4×4 grid of 16×16 bitmask tiles  
**Behaviour:** A path of interconnected tiles that spike blocks and float platforms ride along. Each tile has a bitmask of connection directions (N/E/S/W) that drives the correct sprite selection. Objects riding a rail store a float `t ∈ [0, tile_count)` and call `rail_get_world_pos()` each step. `w` and `h` are 2–128 tiles and a `RECT` loop has at most 128 tiles. At an open (`HORIZ`) end without a cap, a spike block detaches and falls; float platforms always bounce. A rider's `speed` must be above 0 and at most `MAX_RAIL_SPEED` (30 tiles/s).

| Constant | Value | Description |
|----------|-------|-------------|
| `RAIL_N/E/S/W` | 1/2/4/8 | Connection bitmask flags |
| `RAIL_TILE_W/H` | 16 | Tile size in the sprite sheet |
| `MAX_RAIL_TILES` | 128 | Max tiles per Rail instance |
| `MAX_RAILS` | 16 | Rail instances in `GameState` |
| `MAX_RAIL_SPEED` | 30 | Fastest rider speed a level may set, in tiles/s |

```toml
[[rails]]
layout  = "RECT"   # "RECT" = closed rectangle | "HORIZ" = open horizontal line
x       = 444      # top-left tile x
y       = 35       # top-left tile y
w       = 10       # width in tiles
h       = 6        # height in tiles
end_cap = 0        # HORIZ only: 0 = open end (spike block detaches), 1 = bouncing end
```

---

### Vine

**File:** `src/surfaces/vine.c` / `vine.h`  
**Sprites:** `assets/sprites/surfaces/vine_green.png`, `assets/sprites/surfaces/vine_brown.png` — 16×48 px per tile  
**Behaviour:** Press Up while overlapping to grab (without holding Jump), then Up/Down to climb and Left/Right to drift. Jump dismounts; leaving the grab area also detaches.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_VINES` | 24 | Vine slots in `GameState` |
| `VINE_W/H` | 16×32 | Cropped climbable art size |
| `VINE_STEP` | 19 | Vertical spacing between tile starts |

```toml
[[vines]]
x          = 88.0
y          = 172.0   # top tile y in logical pixels
tile_count = 2       # cropped height = 32 + (tile_count - 1) * 19 = 51 px
vine_type  = 0       # optional art variant: 0 = green, 1 = brown
```

For every climbable, `tile_count` is at least 1 and the whole stack must fit inside the world.

---

### Ladder

**File:** `src/surfaces/ladder.c` / `ladder.h`  
**Sprite:** `assets/sprites/surfaces/ladder.png`  
**Behaviour:** Uses the same Up-to-grab, Up/Down climb, horizontal drift and Jump-to-dismount controls as vines.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_LADDERS` | 16 | Ladder slots in `GameState` |
| `LADDER_W/H` | 16×22 | Cropped climbable art size |
| `LADDER_STEP` | 8 | Vertical spacing between tile starts |

```toml
[[ladders]]
x          = 1552.0
y          = 0.0       # top tile y
tile_count = 30        # height in tiles
```

---

### Rope

**File:** `src/surfaces/rope.c` / `rope.h`  
**Sprite:** `assets/sprites/surfaces/rope.png`  
**Behaviour:** Same interaction model as the vine: Up to grab, Up/Down to climb, Left/Right to drift, and Jump to dismount. There is no rope-swing simulation.

| Constant | Value | Description |
|----------|-------|-------------|
| `MAX_ROPES` | 16 | Rope slots in `GameState` |
| `ROPE_W/H` | 16×36 | Cropped climbable art size |
| `ROPE_SRC_X/Y/W/H` | 0 / 6 / 16 / 36 | Source crop from the 16×48 sprite |
| `ROPE_STEP` | 23 | Vertical spacing for stacked rope tiles |

```toml
[[ropes]]
x          = 460.0
y          = 172.0
tile_count = 1
```
