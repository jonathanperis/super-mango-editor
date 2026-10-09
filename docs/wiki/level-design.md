# Level Design — TOML Reference

<a id="home"></a>

On this page: [Quick start](#quick-start) · [Top-level scalars](#top-level-scalars) ·
[Checkpoints](#authored-checkpoints) · [Rails](#rails) · [Platforms](#platforms) ·
[Coins](#coins) · [Stars](#stars) · [Last star](#last-star) ·
[Campaign manifest](#campaign-manifest-v1) · [Enemies](#enemies) ·
[Hazards](#hazards) · [Surfaces](#surfaces) ·
[Background and foreground layers](#background--foreground-layers) ·
[Minimum valid level](#minimum-valid-level-file)

---

Super Mango levels are defined as [TOML](https://toml.io) files inside the `levels/` directory. The shared serializer parses them; `level_loader.c` creates runtime objects from the validated data. The editor reads and writes the same format. Positions use **logical world pixels**: the viewport is 400×300, while world width is `screen_count × 400`.

> **TOML scope:** Put root fields before the first table header. A key after `[physics]` or `[[coins]]` belongs to that table until another header begins; it does not return to the root. Use `[[...]]` for repeated placements and `[last_star]` / `[physics]` for singleton tables. Strict v1 validation rejects misplaced or unknown fields.

---

## Quick Start

```sh
# Run a focused collision lab directly
make run-level LEVEL=levels/labs/01_collision.toml

# Open a level in the visual editor
make run-editor
# Then press Ctrl+O (or the Open button) inside the editor
```

---

## Top-Level Scalars

New checked-in levels must declare `format_version = 1`. Other root fields have loader defaults, but should be authored explicitly when they matter to the level. Files without a version use the legacy reader; new examples use strict v1.

```toml
format_version = 1                       # required level-schema version
name        = "Creator's Playground"
description = """
Optional multi-line description of the level.
"""
generated_by  = "Author Name"           # optional credit string
screen_count  = 4                       # world width = screen_count × 400 px
player_start_x = 79.0                   # player spawn x in logical pixels
player_start_y = 124.5                  # player spawn y in logical pixels
music_path    = "assets/sounds/levels/water.wav"
music_volume  = 13                      # authored volume units 0–128
floor_tile_path = "assets/sprites/levels/grass_tileset.png"
initial_hearts  = 3                     # starting hit points
initial_lives   = 3                     # starting lives
score_per_life  = 1000                  # score at which a bonus life is awarded
coin_score      = 100                   # points per coin collected
floor_gaps      = [0, 192, 560, 928]    # world-space x positions of sea gaps
```

| Field | Type | Description |
|-------|------|-------------|
| `format_version` | int | Required for checked-in levels and new v1 documents: `1`. |
| `name`, `description`, `generated_by` | string | Valid UTF-8, at most 63, 4095 and 127 bytes. The editor warns when `name` is empty. |
| `screen_count` | int | Number of 400px-wide screens, `0`–`99`; `0` means the default of 4. `4` → world is 1600px wide. The editor requires at least 1. |
| `player_start_x/y` | float | Spawn x and foot/landing y in logical pixels; y is not the sprite's top edge. Both `0` means the engine default (x 80, y 172); otherwise the point must be inside the world. |
| `music_path` | string | A WAV path matching `assets/sounds/*.wav`, relative to repo root, at most 63 bytes. Empty means no music. |
| `music_volume` | int | Authored music volume: 0 (silent) – 128 (full), scaled by the player's music setting at playback. |
| `floor_tile_path` | string | A PNG matching `assets/sprites/levels/*.png` used to tile the ground, at most 63 bytes. Per-level theming. |
| `initial_hearts` | int | Starting hit points, `0`–`3`; `0` means the default (`MAX_HEARTS`, 3). |
| `initial_lives` | int | Starting lives, `0`–`999`; `0` means the default (3). |
| `score_per_life` | int | Score threshold spacing for bonus lives, `0`–`999999`; `0` means the default (1000). |
| `coin_score` | int | Points awarded for each collected coin, `0`–`999999`; `0` means the default (100). |
| `floor_gaps` | int array | Up to 16 sea-gap x-positions; each gap is 32 px wide, must fit inside the world, and must start on a multiple of 16 px (the floor is drawn in 16 px pieces, so an off-grid gap would show grass over part of the hole). The player falls in when their centre is over the opening; once their feet are below the floor top, the gap's sides hold them in, so steering sideways cannot climb back onto the grass. Blue/fire flames are placed manually; each flame `x` must match one of these openings. |

Asset paths must be repo-relative with forward slashes, without `..` segments or control characters. Every string must be valid UTF-8; the parser rejects other bytes as `<field> is not valid UTF-8`. When the editor saves, it escapes control characters (including DEL, `\u007f`) so the file loads again.

## Authored Checkpoints

`[[checkpoints]]` is optional level data for explicit respawn points. Each record requires finite numeric `x` and `y` values:

```toml
[[checkpoints]]
x = 304.0
y = 205.0

[[checkpoints]]
x = 448.0
y = 205.0
```

| Rule | Requirement |
|------|-------------|
| Capacity | At most `MAX_CHECKPOINTS` (`99`) records. |
| `x` | Finite, unique, strictly after the effective player-start x, and within `0..(screen_count × GAME_W − TILE_SIZE)`. The respawn column `x..x + TILE_SIZE` must not overlap any floor gap. |
| `y` | Finite and within `0..GAME_H`. |
| Placement order | Kept as authored. It controls the editor/HUD checkpoint number; records do not need to be sorted by x. |

At runtime, the level definition remains immutable. After player movement and before lethal collision handling, the runtime resolves the furthest checkpoint whose `x` is at or behind the player. Its exact `x` and `y` become the respawn position; progress never moves backward. A brief bottom-left `CHECKPOINT CP n` notice (1.2 s of play; time spent paused does not count) confirms activation; a death respawn shows `RESPAWN CP n` for 0.9 s.

When a level has one or more authored records, they are the only checkpoint system: automatic screen-boundary checkpoints are disabled, including before the first authored record is crossed. When `[[checkpoints]]` is omitted or empty, the legacy automatic screen-boundary checkpoints apply: entering a new screen saves the screen edge, or the nearest safe column to its left when the edge is over a floor gap or a static hazard (spike row, spike platform, flame). Moving dangers (saws, axes, spike blocks, enemies) are not considered; author `[[checkpoints]]` where a respawn must avoid them. If the whole stretch since the last checkpoint is unsafe, the previous checkpoint is kept. Loading a level, retrying after game over, replaying, or successfully advancing to the next phase starts from that level's effective player start again.

The standalone `levels/labs/03_checkpoints.toml` example places checkpoints before and after one gap. It is independent of the campaign catalog.

### Optional `[physics]` Overrides

Levels can override player movement and camera feel with a `[physics]` table. Every field is optional; omitted fields, or values below zero, keep the engine default (movement constants in `src/player/player_lifecycle.c`, camera constants in `src/game_constants.h`). Values must be finite and at most `MAX_LEVEL_MOTION` (10000). The example below shows overrides; the defaults are in the table.

```toml
[physics]
walk_max_speed          = 160.0
run_max_speed           = 250.0
walk_ground_accel       = 1200.0
run_ground_accel        = 1600.0
ground_friction         = 1800.0
ground_counter_accel    = 2400.0
air_accel_walk          = 600.0
air_accel_run           = 450.0
air_friction            = 80.0
cam_lookahead_vx_factor = 0.20
cam_lookahead_max       = 50.0
```

| Field | Default | Description |
|-------|---------|-------------|
| `walk_max_speed` | 100 | Maximum horizontal walk speed in px/s. |
| `run_max_speed` | 250 | Maximum horizontal run speed in px/s. |
| `walk_ground_accel` | 750 | Ground acceleration when walking (px/s²). |
| `run_ground_accel` | 600 | Ground acceleration when running (px/s²). |
| `ground_friction` | 550 | Deceleration when no horizontal input is held on the ground (px/s²). |
| `ground_counter_accel` | 100 | Extra deceleration/turn force when reversing direction on the ground (px/s²). |
| `air_accel_walk` | 350 | Horizontal air-control acceleration for walk arcs (px/s²). |
| `air_accel_run` | 180 | Horizontal air-control acceleration for run arcs (px/s²). |
| `air_friction` | 80 | Air deceleration when no horizontal input is held (px/s²). |
| `cam_lookahead_vx_factor` | 0.20 | Camera lookahead in px per px/s of player horizontal velocity. |
| `cam_lookahead_max` | 50 | Maximum camera lookahead distance in logical pixels. |

All fields are floats. In debug mode the inspector (F6 to choose a field, minus/equal to change it by 25, F7 to restore) edits the first nine live; see the [Mechanics Museum](../mechanics-museum/).

Use physics overrides sparingly: they are level-wide tuning knobs, not per-entity behaviour. After changing them, run the level directly and include `make scripted-smoke` in validation so deterministic replay input still behaves.

---

## Rails

Rails define closed or open tracks that spike blocks and float platforms ride on.

```toml
[[rails]]
layout  = "RECT"    # "RECT" = rectangular loop, "HORIZ" = horizontal line
x       = 444       # top-left tile x in logical pixels
y       = 35        # top-left tile y in logical pixels
w       = 10        # width in tiles (for RECT)
h       = 6         # height in tiles (for RECT)
end_cap = 0         # HORIZ only: 0 = open end (spike block detaches), 1 = capped end (bounces)
```

Rail layouts:

| `layout` | Shape | Typical use |
|----------|-------|-------------|
| `RECT` | Closed rectangular loop | Continuous-circuit spike blocks / float platforms |
| `HORIZ` | Open horizontal line | Spike block that bounces left–right; `w` = length in tiles |

`w` and `h` are 2–128 tiles (16 px each), a `RECT` loop may have at most 128 tiles in total (`2w + 2(h − 2)`), and the whole rail must lie inside the world. The `end_cap` flag (`0` or `1`) only applies to open (`HORIZ`) rails. With `end_cap = 1` a spike block bounces back; with `end_cap = 0` it waits at the start until the camera reaches it, then falls off the far end as a projectile. Float platforms on an open rail always bounce at both ends.

Riders name a rail by its position in the `[[rails]]` list (`rail_index`, 0-based), and their `t_offset` must lie on that rail. Reordering rails in a text editor therefore changes which rail each rider uses; the editor renumbers references for you.

---

## Platforms

Ground-level pillar columns. The player can land on the top surface only.

```toml
[[platforms]]
x           = 80.0   # left edge of the pillar in logical pixels
tile_height = 2      # pillar height in 48px tiles (1–5)
tile_width  = 1      # pillar width in 48px tiles (0 or omitted = 1)
```

An optional `tile_path` (`assets/sprites/levels/*.png`) gives one pillar its own tileset; otherwise it uses `assets/sprites/levels/grass_platform.png`.

Each pillar sinks 16 px into the floor so its grass edge meets the ground, so its top surface is `FLOOR_Y − (tile_height × TILE_SIZE) + 16` = `268 − (tile_height × 48)`.

| `tile_height` | Top surface Y | Notes |
|---------------|---------------|-------|
| 1 | 220 | Short hop |
| 2 | 172 | Standard step-up |
| 3 | 124 | Tall — use a bouncepad, climbable, or intermediate ledge |
| 4 | 76 | Very tall |
| 5 | 28 | Maximum height |

---

## Coins

```toml
[[coins]]
x = 46.0    # left edge in logical pixels (render width = 16px)
y = 236.0   # top edge y in logical pixels
```

Each coin is worth `coin_score` points (default 100). Every `score_per_life` points grants a bonus life. Up to `MAX_COINS` (64) per level, each inside the world. Collected coins stay collected when the player loses a life; they return only for a fresh attempt (Retry, Replay or loading the level), so dying cannot farm score.

---

## Stars

```toml
[[star_yellows]]
x = 272.0
y = 108.0

[[star_greens]]
x = 500.0
y = 80.0

[[star_reds]]
x = 800.0
y = 100.0
```

Each star variant restores 1 heart on pickup (up to the maximum of 3) and awards no score. All are 16×16 px display size, up to 16 of each colour. The three colours share one runtime module, `src/collectibles/health_star.c`. Stars respawn after a life loss.

---

## Last Star

```toml
[last_star]
x = 1492.0
y = 100.0
next_phase = "levels/01_lugio_01.toml"  # optional level loaded after completion
```

Single-instance. Triggers the level-complete event when collected. Displayed at 24×24 px. `next_phase` is serialized inside `[last_star]` because phase progression is tied to collecting the end-of-level star.

### Level references

`next_phase`, campaign manifest entries and player-profile result keys all name a level with one rule (`src/levels/level_ref.c`, mirrored by `tools/validate_levels.py`):

- The path is a direct child of `levels/` ending in `.toml`: `levels/<name>.toml`. Subdirectories such as `levels/labs/` are rejected, so every chained phase can also be listed in a campaign and record profile results. Labs are standalone examples opened with `--level`.
- `<name>` must not contain `/`, `\`, control characters (including DEL) or the Windows-reserved characters `< > : " | ? *`.
- The stem before the first dot must be nonempty and must not be a Windows device name, in any letter case (trailing spaces ignored): `CON`, `PRN`, `AUX`, `NUL`, `COM0`–`COM9`, `LPT0`–`LPT9`, or `COM`/`LPT` followed by `¹`, `²` or `³`. `levels/con.toml` and `levels/nul.x.toml` are both rejected; `levels/console.toml` is fine.
- The whole path is valid UTF-8 and at most 255 bytes: `next_phase`, campaign entries and profile keys are each stored in a 256-byte C buffer.

Collecting the last star snapshots elapsed time and coin totals, then shows the level-completion summary. With `next_phase`, its actions are **Next Level**, **Replay**, **Level Select**, and **Exit**; without one, the actions are **Replay**, **Level Select**, and **Exit**. Use Up/Down or D-pad to focus an action, Enter/Space/Start (or A) to confirm it, and Esc/Back (or B) to exit without advancing. See [Controls & Input](../controls/) for native level-select and browser-replay behavior.

---

## Campaign Manifest (v1)

`levels/campaigns/main.toml` is the required native-menu catalog, not a playable level. It has exactly two top-level fields. The v1 order starts with Creator's Playground:

```toml
format_version = 1
levels = [
    "levels/00_sandbox_01.toml",
    "levels/01_lugio_01.toml",
    "levels/02_lugio_02.toml",
]
```

| Rule | Requirement |
|------|-------------|
| Version | `format_version` is integer `1`. |
| Membership | `levels` is a nonempty, duplicate-free ordered array. |
| Paths | Each item follows the [level reference](#level-references) rule: a direct child path in the form `levels/<filename>.toml` with forward slashes only. Nested paths, absolute paths, `:`, `\\` and Windows device names are rejected. |
| Listed files | Every entry must resolve and load as a TOML level. The start menu uses that level's `name`, falling back to its filename stem. |
| Progression | Each non-final listed level must set `[last_star].next_phase` to the next manifest entry. The final listed level must omit `next_phase`. |

The manifest order drives the native selector and generated [Level Catalog](../level-catalog/): Creator's Playground, then Volcanic Depths 1 and 2. `make validate-levels` checks the manifest, campaign levels and `levels/labs/*.toml`. The editor's [Campaign view](../level-editor/#campaign-view) (`Ctrl+M`) edits the manifest with the same rules: reorder, add, remove and rename levels, and relink every `next_phase` to the order in one step.

At runtime, breaking the Version, Membership or Paths rules rejects the whole manifest. A listed file that is missing or invalid, or that breaks Progression, only disables its own entry: the menu lists it greyed out as "Unavailable: <reason>" (`level file not found`, `level file is invalid`, `next_phase is out of campaign order`, `final level has a next_phase`) and keeps every other level playable. A manifest with no playable entry is rejected. `--level <path>` bypasses the selector and can open a valid TOML file outside the campaign. The mechanics museum remains a separate collection of standalone examples.

---

## Enemies

Every patrolling enemy needs `patrol_x0 ≤ x ≤ patrol_x1`, inside the world, a range at least as wide as its sprite (`patrol_x1 - patrol_x0` ≥ 64 px for spiders and jumping spiders, 48 px for birds and fish), and a `vx` that is not 0 and whose size is at most `MAX_PATROL_SPEED` (960 px/s, half a floor gap per 1/60 s step, so a spider can never step over a gap between two gap checks). `vx` is the patrol speed for the whole level: its sign picks the first direction, and every turn flips the direction and keeps the speed. The usual speeds are spider 50, jumping spider 55, bird 45, faster bird 80, fish 70 and faster fish 120 px/s. Each type holds up to 16 placements.

### Spiders

Ground patrol enemy. Walks back and forth between `patrol_x0` and `patrol_x1`, turning at floor gaps.

```toml
[[spiders]]
x          = 600.0   # starting x in logical pixels
vx         = 50.0    # patrol speed (px/s); sign sets the first direction
patrol_x0  = 592.0   # left patrol boundary
patrol_x1  = 750.0   # right patrol boundary
frame_index = 0      # starting animation frame (0–2)
```

### Jumping Spiders

Variant that leaps across sea gaps. Uses the spider's position, velocity and patrol fields, but has no authored `frame_index`.

```toml
[[jumping_spiders]]
x          = 130.0
vx         = 55.0
patrol_x0  = 46.0
patrol_x1  = 310.0
```

### Birds

Slow sine-wave sky patrol. `base_y` is the vertical centre of the wave.

```toml
[[birds]]
x          = 100.0
base_y     = 60.0    # vertical centre of the sine wave in logical pixels
vx         = 45.0    # horizontal speed (px/s)
patrol_x0  = 0.0
patrol_x1  = 700.0
frame_index = 0
```

### Faster Birds

Same schema as `[[birds]]`. Higher `vx` for faster patrol.

```toml
[[faster_birds]]
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
[[fish]]
x          = 700.0
vx         = 70.0
patrol_x0  = 500.0
patrol_x1  = 950.0
```

### Faster Fish

Same schema as `[[fish]]`. Higher `vx`.

```toml
[[faster_fish]]
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
[[axe_traps]]
pillar_x = 256.0    # left x of the 48 px column; the pivot is at its centre
y        = 0.0      # pivot y; 0 = default (y 124, the top of a 3-tile pillar)
mode     = "PENDULUM"  # "PENDULUM" = sinusoidal ±60° swing | "SPIN" = full 360°
```

The default height is fixed; it is not measured from a pillar at `pillar_x`, so set `y` when the axe hangs from a shorter or taller pillar. A non-zero `y` must be within `0..300`.

| `mode` | Behaviour | Period |
|--------|-----------|--------|
| `PENDULUM` | Swings −60° to +60° and back | 2 seconds per cycle |
| `SPIN` | Continuous clockwise rotation | 180°/s → one full rotation per 2 s |

### Circular Saws

Fast horizontal patrol with constant spin. Does not use a rail.

```toml
[[circular_saws]]
x          = 1350.0
y          = 0.0      # 0 = default (y 140: rolling on top of a 2-tile pillar)
patrol_x0  = 1350.0
patrol_x1  = 1446.0
direction  = 1        # 1 = starts moving right, -1 = starts moving left
```

`direction` must be `1` or `-1`, and `patrol_x0 ≤ x ≤ patrol_x1`. A non-zero `y` is the saw's top edge and must be within `0..300`.

Patrol speed: 180 px/s. Spin speed: 720°/s. Pushes player on contact (220 px/s + −150 vy).

### Spike Rows

Static strip of 16×16 spike tiles placed on the ground floor.

```toml
[[spike_rows]]
x     = 780.0   # left edge of the strip in logical pixels
count = 4       # number of 16×16 tiles in the row (1–16)
```

### Spike Platforms

Elevated spike hazard surface.

```toml
[[spike_platforms]]
x          = 370.0
y          = 200.0   # top edge in logical pixels
tile_count = 3       # number of 16 px tiles wide (1–16)
```

### Spike Blocks

Rail-riding hazard. References a rail by index (0-based order of `[[rails]]` in the file).

```toml
[[spike_blocks]]
rail_index = 0      # which rail to ride (0 = first [[rails]] entry)
t_offset   = 0.0    # starting position on the rail (0.0 = first tile)
speed      = 1.5    # traversal speed in tiles per second
```

`speed` must be above 0 and at most `MAX_RAIL_SPEED` (30 tiles/s).

### Blue Flames

Erupts from a manually placed floor-gap position. `x` is the gap's left edge and must match a `floor_gaps` entry; the flame is centred in the 32 px opening. `x` must leave room for the whole gap inside the world; `x = 0` is the gap at the left edge of the world. Blue and fire flame placements have separate capacities: `MAX_BLUE_FLAMES` and `MAX_FIRE_FLAMES` (16 each).

```toml
[[blue_flames]]
x = 192.0   # left edge of the sea gap (same value as its floor_gaps entry)
```

```toml
[[fire_flames]]
x = 560.0   # same mechanics, fire-colored sprite (also a floor_gaps entry)
```

Eruption cycle: **waiting** (1.5 s) → **rising** (−550 px/s launch) → **flipping** (180° over 0.12 s at apex) → **falling** → repeat.

---

## Surfaces

### Float Platforms

Hovering surfaces with three behaviour modes.

```toml
[[float_platforms]]
mode       = "STATIC"   # "STATIC" | "CRUMBLE" | "RAIL"
x          = 172.0
y          = 200.0
tile_count = 4          # platform width in 16px pieces (1–16)
rail_index = 0          # only used for RAIL mode
t_offset   = 0.0        # rail starting position (RAIL mode)
speed      = 0.0        # rail traversal speed in tiles/s (RAIL mode)
```

| `mode` | Behaviour |
|--------|-----------|
| `STATIC` | Hovers at fixed position forever |
| `CRUMBLE` | Falls after the player stands on it for 0.75 s without stepping off (stepping off resets the timer) |
| `RAIL` | Travels along the referenced rail path, bouncing at the ends of an open rail; the rail sets its position, so `x`/`y` are not used |

`STATIC` and `CRUMBLE` platforms must fit inside the world. A `RAIL` platform's `speed` must be above 0 and at most `MAX_RAIL_SPEED` (30 tiles/s). The platform carries the player standing on it, up and down as well as sideways.

### Bridges

Tiled crumble walkway. Each brick the player stands on falls 0.2 s later; fallen bricks return only when the level resets after a life loss.

```toml
[[bridges]]
x           = 1350.0
y           = 172.0
brick_count = 8       # number of 16×16 brick tiles (1–16)
```

### Bouncepads

Spring pads that launch the player vertically. Three size tiers.

```toml
[[bouncepads_small]]
x         = 734.0
launch_vy = -380.0    # upward impulse in px/s (negative = up)
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

| Array | `pad_type` | Usual `launch_vy` | Rise (approx.) |
|-------|------------|---------------------|----------------|
| `bouncepads_small` | `GREEN` | −380.0 | 90 px |
| `bouncepads_medium` | `WOOD` | −536.25 | 180 px |
| `bouncepads_high` | `RED` | −700.0 | 306 px (full screen height) |

The usual values are the `BOUNCEPAD_VY_*` constants; the loader uses the authored `launch_vy` as written, so always set it. The rise is `launch_vy² / (2 × 800)`, with gravity 800 px/s². `pad_type` must be `GREEN`, `WOOD` or `RED`, and `launch_vy` must be between `-MAX_LEVEL_MOTION` and `JUMP_VY` (−325): the player cannot jump while standing on a pad, so every pad has to launch at least as hard as a normal jump.

### Climbable Surfaces

Climbables are 16px wide, with overlapping cropped segments. Total height is `H + (tile_count - 1) × STEP`: vines use `H=32, STEP=19`, ladders `H=22, STEP=8`, and ropes `H=36, STEP=23`. The complete stack must fit within the 300px world height.

```toml
[[vines]]
x          = 88.0
y          = 172.0   # top tile y in logical pixels
tile_count = 2       # 51px total cropped height
vine_type  = 0       # optional art variant: 0 = Green, 1 = Brown

[[ladders]]
x          = 1552.0
y          = 0.0
tile_count = 30

[[ropes]]
x          = 460.0
y          = 172.0
tile_count = 1
```

Press Up while overlapping any climbable to grab it, without holding Jump. Up/Down climbs, Left/Right drifts, and Jump dismounts. `vine_type` is preserved by the serializer and exposed by the editor as a visual-variant dropdown: `0` = Green, `1` = Brown.

---

## Background & Foreground Layers

```toml
[[background_layers]]
path  = "assets/sprites/backgrounds/sky_blue.png"
speed = 0.0    # parallax scroll factor: 0.0 = static, 1.0 = locks to camera

[[background_layers]]
path  = "assets/sprites/backgrounds/glacial_mountains.png"
speed = 0.2

[[foreground_layers]]
path  = "assets/sprites/foregrounds/water.png"
speed = 0.0

[[fog_layers]]
path = "assets/sprites/foregrounds/fog_1.png"

[[fog_layers]]
path = "assets/sprites/foregrounds/fog_2.png"
```

Background layers are drawn in array order (first = furthest back). Up to 8 background layers, 8 foreground layers and 4 fog layers are supported; every path must match `assets/sprites/*.png` and fit in 63 bytes. Speed `0.0` tiles the image but does not scroll; speed `1.0` would scroll at the same rate as the camera (appears fixed in world space). Most parallax layers use `0.1`–`0.5`. `foreground_layers` select the water/lava foreground strip texture, while `fog_layers` configure semi-transparent atmospheric overlays loaded by the fog system.

Common background images in `assets/sprites/backgrounds/` (the folder also has `smoke_*` layers and `*_lightened` variants):

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
format_version  = 1
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

[[background_layers]]
path  = "assets/sprites/backgrounds/sky_blue.png"
speed = 0.0

[last_star]
x = 760.0
y = 200.0
```

See `levels/labs/03_checkpoints.toml` for a compact checkpoint example and `levels/00_sandbox_01.toml` for a combined showcase. Use the generated catalog for its actual entity inventory.
