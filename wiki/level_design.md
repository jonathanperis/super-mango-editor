# Level Design — TOML Reference

[← Home](index.md)

---

Super Mango levels are defined as [TOML](https://toml.io) files inside the `levels/` directory. The game engine loads them at startup via `level_loader.c`; the level editor reads and writes the same format. All positions are in **logical pixels** (400×300 coordinate space).

> **TOML rule:** All scalar key-value pairs must appear **before** any `[array tables](Larray tablesE.md)` in the file. The parser (`tomlc17`) will fail silently or error if arrays precede scalars.

---

## Quick Start

```sh
# Run a level file directly
make run-level LEVEL=levels/00_sandbox_01.toml

# Open a level in the visual editor
make run-editor
# Then File → Open inside the editor
```

---

## Top-Level Scalars

Every level file begins with these key-value pairs. All are required unless marked optional.

```toml
name        = "Creator's Playground"
description = """
Optional multi-line description of the level.
"""
generated_by  = "Author Name"           # optional credit string
screen_count  = 4                       # world width = screen_count × 400 px
player_start_x = 79.0                   # player spawn x in logical pixels
player_start_y = 124.5                  # player spawn y in logical pixels
music_path    = "assets/sounds/levels/water.wav"
music_volume  = 13                      # SDL_mixer volume 0–128
floor_tile_path = "assets/sprites/levels/grass_tileset.png"
initial_hearts  = 3                     # starting hit points
initial_lives   = 3                     # starting lives
score_per_life  = 1000                  # score at which a bonus life is awarded
coin_score      = 100                   # points per coin collected
floor_gaps      = [0, 192, 560, 928]    # world-space x positions of sea gaps
```

| Field | Type | Description |
|-------|------|-------------|
| `screen_count` | int | Number of 400px-wide screens. `4` → world is 1600px wide. |
| `player_start_x/y` | float | Spawn position in logical pixels. |
| `music_path` | string | Path to WAV/OGG, relative to repo root. |
| `music_volume` | int | SDL_mixer channel volume: 0 (silent) – 128 (full). |
| `floor_tile_path` | string | PNG used to tile the ground. Per-level theming. |
| `floor_gaps` | int array | Sea gap x-positions. Blue flames spawn at each gap automatically. |

---

## Rails

Rails define closed or open tracks that spike blocks and float platforms ride on.

```toml
[rails](LrailsE.md)
layout  = "RECT"    # "RECT" = rectangular loop, "HORIZ" = horizontal line
x       = 444       # top-left tile x in logical pixels
y       = 35        # top-left tile y in logical pixels
w       = 10        # width in tiles (for RECT)
h       = 6         # height in tiles (for RECT)
end_cap = 0         # 0 = open end (rider detaches), 1 = capped end (rider bounces)
```

Rail layouts:

| `layout` | Shape | Typical use |
|----------|-------|-------------|
| `RECT` | Closed rectangular loop | Continuous-circuit spike blocks / float platforms |
| `HORIZ` | Open horizontal line | Spike block that bounces left–right; `w` = length in tiles |

The `end_cap` flag only applies to open (`HORIZ`) rails. With `end_cap = 1` the rider bounces back; with `end_cap = 0` it falls off the far end as a projectile.

---

## Platforms

Ground-level pillar columns. The player can land on the top surface only.

```toml
[platforms](LplatformsE.md)
x           = 80.0   # left edge of the pillar in logical pixels
tile_height = 2      # pillar height in 48px tiles (1 = 48px, 2 = 96px, 3 = 144px)
tile_width  = 1      # pillar width in 48px tiles (usually 1)
```

Pillar top Y: `FLOOR_Y − (tile_height × TILE_SIZE)` = `252 − (tile_height × 48)`.

| `tile_height` | Top surface Y | Notes |
|---------------|---------------|-------|
| 1 | 204 | Short hop |
| 2 | 156 | Standard — medium bouncepads clear this |
| 3 | 108 | Tall — requires high bouncepad or triple-jump |

---

## Coins

```toml
[coins](LcoinsE.md)
x = 46.0    # centre-ish x in logical pixels (render width = 16px)
y = 236.0   # top edge y in logical pixels
```

Each coin is worth `coin_score` points (default 100). Every `score_per_life` points grants a bonus life.

---

## Stars

```toml
[star_yellows](Lstar_yellowsE.md)
x = 272.0
y = 108.0

[star_greens](Lstar_greensE.md)
x = 500.0
y = 80.0

[star_reds](Lstar_redsE.md)
x = 800.0
y = 100.0
```

Each star variant restores 1 heart on pickup. All are 16×16 px display size.

---

## Last Star

```toml
[last_star]
x = 1492.0
y = 100.0
```

Single-instance. Triggers the level-complete event when collected. Displayed at 24×24 px.

---

## Enemies

### Spiders

Ground patrol enemy. Walks back and forth between `patrol_x0` and `patrol_x1`.

```toml
[spiders](LspidersE.md)
x          = 600.0   # starting x in logical pixels
vx         = 50.0    # initial horizontal speed (px/s); sign sets direction
patrol_x0  = 592.0   # left patrol boundary
patrol_x1  = 750.0   # right patrol boundary
frame_index = 0      # starting animation frame (0–2)
```

### Jumping Spiders

Variant that leaps across sea gaps. Same fields as spider.

```toml
[jumping_spiders](Ljumping_spidersE.md)
x          = 130.0
vx         = 55.0
patrol_x0  = 46.0
patrol_x1  = 310.0
```

### Birds

Slow sine-wave sky patrol. `base_y` is the vertical centre of the wave.

```toml
[birds](LbirdsE.md)
x          = 100.0
base_y     = 60.0    # vertical centre of the sine wave in logical pixels
vx         = 45.0    # horizontal speed (px/s)
patrol_x0  = 0.0
patrol_x1  = 700.0
frame_index = 0
```

### Faster Birds

Same schema as `[birds](LbirdsE.md)`. Higher `vx` for more aggressive patrol.

```toml
[faster_birds](Lfaster_birdsE.md)
x          = 600.0
base_y     = 50.0
vx         = -80.0
patrol_x0  = 300.0
patrol_x1  = 1100.0
frame_index = 0
```

### Fish

Water-lane patrol with random upward jumps.

```toml
[fish](LfishE.md)
x          = 700.0
vx         = 70.0
patrol_x0  = 500.0
patrol_x1  = 950.0
```

### Faster Fish

Same schema as `[fish](LfishE.md)`. Higher `vx`.

```toml
[faster_fish](Lfaster_fishE.md)
x          = 1100.0
vx         = 120.0
patrol_x0  = 900.0
patrol_x1  = 1400.0
```

---

## Hazards

### Axe Traps

Swinging or spinning axe mounted at the top of a platform pillar.

```toml
[axe_traps](Laxe_trapsE.md)
pillar_x = 256.0    # x of the platform column the axe is mounted on
y        = 0.0      # pivot y (0 = top of pillar; engine computes exact Y from pillar)
mode     = "PENDULUM"  # "PENDULUM" = sinusoidal ±60° swing | "SPIN" = full 360°
```

| `mode` | Behaviour | Period |
|--------|-----------|--------|
| `PENDULUM` | Swings −60° to +60° and back | 2 seconds per cycle |
| `SPIN` | Continuous clockwise rotation | 180°/s → one full rotation per 2 s |

### Circular Saws

Fast horizontal patrol with constant spin. Does not use a rail.

```toml
[circular_saws](Lcircular_sawsE.md)
x          = 1350.0
y          = 0.0      # engine snaps to floor level
patrol_x0  = 1350.0
patrol_x1  = 1446.0
direction  = 1        # 1 = starts moving right, -1 = starts moving left
```

Patrol speed: 180 px/s. Spin speed: 720°/s. Pushes player on contact (220 px/s + −150 vy).

### Spike Rows

Static strip of 16×16 spike tiles placed on the ground floor.

```toml
[spike_rows](Lspike_rowsE.md)
x     = 780.0   # left edge of the strip in logical pixels
count = 4       # number of 16×16 tiles in the row
```

### Spike Platforms

Elevated spike hazard surface.

```toml
[spike_platforms](Lspike_platformsE.md)
x          = 370.0
y          = 200.0   # top edge in logical pixels
tile_count = 3       # number of tiles wide
```

### Spike Blocks

Rail-riding hazard. References a rail by index (0-based order of `[rails](LrailsE.md)` in the file).

```toml
[spike_blocks](Lspike_blocksE.md)
rail_index = 0      # which rail to ride (0 = first [rails](LrailsE.md) entry)
t_offset   = 0.0    # starting position on the rail (0.0 = first tile)
speed      = 1.5    # traversal speed in tiles per second
```

### Blue Flames

Erupts from a sea gap. The engine automatically places one per gap; use this entry to override or add extras.

```toml
[blue_flames](Lblue_flamesE.md)
x = 192.0   # world-space x of the sea gap centre
```

Eruption cycle: **waiting** (1.5 s) → **rising** (−550 px/s launch) → **flipping** (180° over 0.12 s at apex) → **falling** → repeat.

---

## Surfaces

### Float Platforms

Hovering surfaces with three behaviour modes.

```toml
[float_platforms](Lfloat_platformsE.md)
mode       = "STATIC"   # "STATIC" | "CRUMBLE" | "RAIL"
x          = 172.0
y          = 200.0
tile_count = 4          # platform width in 16px pieces
rail_index = 0          # only used for RAIL mode
t_offset   = 0.0        # rail starting position (RAIL mode)
speed      = 0.0        # rail traversal speed in tiles/s (RAIL mode)
```

| `mode` | Behaviour |
|--------|-----------|
| `STATIC` | Hovers at fixed position forever |
| `CRUMBLE` | Falls after player stands on it for 0.75 s |
| `RAIL` | Travels along the referenced rail path |

### Bridges

Tiled crumble walkway. Bricks fall when the player walks across.

```toml
[bridges](LbridgesE.md)
x           = 1350.0
y           = 172.0
brick_count = 8       # number of 16×16 brick tiles
```

### Bouncepads

Spring pads that launch the player vertically. Three size tiers.

```toml
[bouncepads_small](Lbouncepads_smallE.md)
x         = 734.0
launch_vy = -380.0    # upward impulse in px/s (negative = up)
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

| Array | `pad_type` | Default `launch_vy` | Clears |
|-------|------------|---------------------|--------|
| `bouncepads_small` | `GREEN` | −380.0 | 1-tile pillars |
| `bouncepads_medium` | `WOOD` | −536.25 | 2-tile pillars |
| `bouncepads_high` | `RED` | −700.0 | 3-tile pillars |

### Climbable Surfaces

Vines, ladders, and ropes are placed as vertical stacks of 16px tiles.

```toml
[vines](LvinesE.md)
x          = 88.0
y          = 172.0   # top tile y in logical pixels
tile_count = 2       # height in 16px tiles

[ladders](LladdersE.md)
x          = 1552.0
y          = 0.0
tile_count = 30

[ropes](LropesE.md)
x          = 460.0
y          = 172.0
tile_count = 1
```

The player can climb all three by pressing Up/Down while overlapping the surface. Vines and ropes require the player to jump into them; ladders are entered by pressing Up at the base.

---

## Background & Foreground Layers

```toml
[background_layers](Lbackground_layersE.md)
path  = "assets/sprites/backgrounds/sky_blue.png"
speed = 0.0    # parallax scroll factor: 0.0 = static, 1.0 = locks to camera

[background_layers](Lbackground_layersE.md)
path  = "assets/sprites/backgrounds/glacial_mountains.png"
speed = 0.2

[foreground_layers](Lforeground_layersE.md)
path  = "assets/sprites/foregrounds/fog_1.png"
speed = 0.6
```

Layers are drawn in array order (first = furthest back). Up to 8 background layers are supported. Speed `0.0` tiles the image but doesn't scroll; speed `1.0` would scroll at the same rate as the camera (appears fixed in world space). Most parallax layers use `0.1`–`0.5`.

Available background images in `assets/sprites/backgrounds/`:

| File | Suggested speed |
|------|----------------|
| `sky_blue.png` | 0.0 |
| `sky_fire.png` | 0.0 |
| `clouds_bg.png` | 0.1 |
| `glacial_mountains.png` | 0.2 |
| `volcanic_mountains.png` | 0.2 |
| `forest_leafs.png` | 0.3 |
| `clouds_mg_1/2/3.png` | 0.3–0.5 |
| `clouds_lonely.png` | 0.4 |
| `castle_pillars.png` | 0.5 |

---

## Minimum Valid Level File

```toml
name            = "My Level"
screen_count    = 2
player_start_x  = 40.0
player_start_y  = 200.0
music_path      = "assets/sounds/levels/water.wav"
music_volume    = 15
floor_tile_path = "assets/sprites/levels/grass_tileset.png"
initial_hearts  = 3
initial_lives   = 3
score_per_life  = 1000
coin_score      = 100
floor_gaps      = []

[background_layers](Lbackground_layersE.md)
path  = "assets/sprites/backgrounds/sky_blue.png"
speed = 0.0

[last_star]
x = 760.0
y = 200.0
```

See `levels/00_sandbox_01.toml` for a full-featured reference level using every entity type.
