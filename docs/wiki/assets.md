# Assets

<a id="home"></a>

---

All visual assets live in the `assets/` directory, organized into categorized subdirectories. Sprites are PNG files (loaded via `SDL2_image`), sounds are WAV files in `assets/sounds/`, and fonts are in `assets/fonts/`.

```
assets/
├── fonts/                  ← TrueType fonts
├── sounds/                 ← Audio files (see Sounds page)
│   ├── collectibles/
│   ├── entities/
│   ├── hazards/
│   ├── levels/
│   ├── player/
│   ├── screens/
│   ├── surfaces/
│   └── unused/
└── sprites/                ← PNG sprite sheets and textures
    ├── backgrounds/        ← Parallax background layers
    ├── collectibles/       ← Coins, stars
    ├── entities/           ← Enemy sprites
    ├── foregrounds/        ← Fog, water, lava, clouds
    ├── hazards/            ← Spike, saw, flame sprites
    ├── levels/             ← Level tilesets and themes
    ├── player/             ← Player sprite sheet
    ├── screens/            ← HUD, menu assets
    ├── surfaces/           ← Platforms, bridges, vines, etc.
    └── unused/             ← Reserve assets for future use
```

> **Coordinate note:** All game objects use **logical space (400x300)**. SDL scales to the 800x600 OS window 2x. A 48x48 sprite appears as 96x96 physical pixels on screen.

---

## Per-Level Asset Configuration

Several visual elements are configurable per TOML level file, allowing different themes without changing code:

### Floor Tile

```toml
floor_tile_path = "assets/sprites/levels/grass_tileset.png"
```

Any 48×48 PNG tile can be substituted here. The engine 9-slice renders it across `FLOOR_Y` to form the ground.

### Parallax Background Layers

```toml
[[background_layers]]
path  = "assets/sprites/backgrounds/sky_blue.png"
speed = 0.0

[[background_layers]]
path  = "assets/sprites/backgrounds/glacial_mountains.png"
speed = 0.2
```

Layers are TOML array-of-tables and are drawn in array order (first = furthest back). `speed` is the parallax scroll factor: `0.0` = static sky, `1.0` = locks to camera, `0.1`–`0.5` = typical mid-ground parallax. Up to 8 layers are supported. See [Level Design](../level-design/) for the full list of available background images and suggested speeds.

### Foreground Strip and Fog Layers

`foreground_layers` currently select the level foreground strip texture used by the water/lava renderer; the active runtime takes the last valid configured path. Atmospheric overlays are configured separately with `fog_layers` and loaded into the fog system.

```toml
[[foreground_layers]]
path  = "assets/sprites/foregrounds/water.png"
speed = 0.0

[[fog_layers]]
path = "assets/sprites/foregrounds/fog_1.png"

[[fog_layers]]
path = "assets/sprites/foregrounds/fog_2.png"
```

---

## Currently Used Assets

### Backgrounds — `assets/sprites/backgrounds/`

| File | Used By | Description |
|------|---------|-------------|
| `sky_blue.png` | `levels/00_sandbox_01.toml` | Static sky gradient backdrop |
| `clouds_bg.png` | `levels/00_sandbox_01.toml` | Background cloud layer |
| `glacial_mountains.png` | `levels/00_sandbox_01.toml` | Distant mountains |
| `clouds_mg_3.png` | `levels/00_sandbox_01.toml` | Midground cloud layer 3 |
| `clouds_mg_2.png` | `levels/00_sandbox_01.toml` | Midground cloud layer 2 |
| `clouds_lonely.png` | `levels/00_sandbox_01.toml` | Single cloud layer |
| `clouds_mg_1.png` | `levels/00_sandbox_01.toml` | Foreground cloud layer |
| `sky_fire.png` | `levels/01_lugio_01.toml`, `02_lugio_02.toml` | Volcanic sky backdrop |
| `sky_fire_lightened.png` | `levels/01_lugio_01.toml`, `02_lugio_02.toml` | Lightened volcanic sky variant |
| `volcanic_mountains.png` | `levels/01_lugio_01.toml`, `02_lugio_02.toml` | Volcanic mountain layer |
| `volcanic_mountains_lightened.png` | `levels/01_lugio_01.toml`, `02_lugio_02.toml` | Lightened volcanic mountains |
| `smoke_bg.png`, `smoke_mg_1.png`, `smoke_mg_2.png`, `smoke_mg_3.png`, `smoke_lonely.png` | Volcanic levels | Smoke parallax layers |

### Foregrounds — `assets/sprites/foregrounds/`

| File | Used By | Description |
|------|---------|-------------|
| `fog_1.png` | `fog_layers` / `fog.c` (`fog->textures[0]`) | Fog overlay layer, semi-transparent sliding effect |
| `fog_2.png` | `fog_layers` / `fog.c` (`fog->textures[1]`) | Fog overlay layer, semi-transparent sliding effect |
| `water.png` | `foreground_layers` / `water->texture` | 384x64 sprite sheet, 8 frames of 48x64 with 16x31 art crop |
| `lava.png` | `foreground_layers` / `water->texture` | Lava-themed foreground strip |
| `clouds.png` | foreground clouds | Decorative cloud layer |
| `fog_fire_1.png`, `fog_fire_2.png`, `smoke.png` | Volcanic levels | Fire fog / smoke overlay layers |

### Player — `assets/sprites/player/`

| File | Used By | Description |
|------|---------|-------------|
| `player.png` | `player->texture` | 192x288 sprite sheet, 4 cols x 6 rows, 48x48 frames |

### Entities — `assets/sprites/entities/`

| File | Used By | Description |
|------|---------|-------------|
| `spider.png` | `gs->textures.spider` | Spider enemy sprite sheet (ground patrol) |
| `jumping_spider.png` | `gs->textures.jumping_spider` | Jumping spider enemy sprite sheet |
| `bird.png` | `gs->textures.bird` | Slow sine-wave bird enemy sprite sheet |
| `faster_bird.png` | `gs->textures.faster_bird` | Fast aggressive bird enemy sprite sheet |
| `fish.png` | `gs->textures.fish` | Jumping fish enemy sprite sheet |
| `faster_fish.png` | `gs->textures.faster_fish` | Faster fish enemy sprite sheet |

### Collectibles — `assets/sprites/collectibles/`

| File | Used By | Description |
|------|---------|-------------|
| `coin.png` | `gs->textures.coin` | 16x16 coin collectible sprite |
| `star_yellow.png` | `gs->textures.star_yellow` | Yellow star collectible sprite |
| `star_green.png` | `gs->textures.star_green` | Green star collectible sprite |
| `star_red.png` | `gs->textures.star_red` | Red star collectible sprite |
| `last_star.png` | `gs->textures.last_star` | Last star goal collectible sprite |

### Hazards — `assets/sprites/hazards/`

| File | Used By | Description |
|------|---------|-------------|
| `spike.png` | `gs->textures.spike` | Floor/ceiling spike hazard |
| `spike_block.png` | `gs->textures.spike_block` | Rail-riding rotating hazard sprite |
| `spike_platform.png` | `gs->textures.spike_platform` | Spiked platform hazard sprite |
| `circular_saw.png` | `gs->textures.circular_saw` | Rotating saw blade hazard |
| `axe_trap.png` | `gs->textures.axe_trap` | Swinging axe trap hazard |
| `blue_flame.png` | `gs->textures.blue_flame` | Blue flame hazard sprite |
| `fire_flame.png` | `gs->textures.fire_flame` | Fire flame hazard sprite |

### Surfaces — `assets/sprites/surfaces/`

| File | Used By | Description |
|------|---------|-------------|
| `float_platform.png` | `gs->textures.float_platform` | 48x16 sprite, 3-slice horizontal strip (left cap, centre fill, right cap) |
| `bridge.png` | `gs->textures.bridge` | 16x16 single-frame brick tile for crumble walkways |
| `bouncepad_small.png` | `gs->textures.bouncepad_small` | Small bouncepad sprite (low launch) |
| `bouncepad_medium.png` | `gs->textures.bouncepad_medium` | Medium bouncepad sprite (standard launch) |
| `bouncepad_high.png` | `gs->textures.bouncepad_high` | High bouncepad sprite (max launch) |
| `rail.png` | `gs->textures.rail` | 64x64 sprite sheet, 4x4 grid of 16x16 bitmask rail tiles |
| `vine_green.png` | `gs->textures.vine_green` | 16x48 green climbable vine sprite |
| `vine_brown.png` | `gs->textures.vine_brown` | 16x48 brown climbable vine sprite |
| `ladder.png` | `gs->textures.ladder` | Climbable ladder sprite |
| `rope.png` | `gs->textures.rope` | Climbable rope sprite |

### Levels — `assets/sprites/levels/`

| File | Used By | Description |
|------|---------|-------------|
| `grass_tileset.png` | `gs->textures.floor_tile` | 48x48 tile, 9-slice rendered across `FLOOR_Y` to form the floor |
| `grass_platform.png` | `gs->textures.platform` | 48x48 tile, 9-slice rendered as one-way platform pillars |
| `stone_tileset.png` | Volcanic levels | 48x48 stone floor tileset |
| `stone_platform.png` | Volcanic levels | Stone platform pillar tile |
| `grass_rock_tileset.png`, `grass_rock_platform.png` | Reserve / level theming | Grass-rock floor/platform variants |
| `brick_tileset.png`, `brick_platform.png` | Reserve / level theming | Brick floor/platform variants |
| `leaf_tileset.png`, `leaf_platform.png` | Reserve / level theming | Forest leaf floor/platform variants |
| `cloud_tileset.png` | Reserve / level theming | Cloud tileset |

### Screens — `assets/sprites/screens/`

| File | Used By | Description |
|------|---------|-------------|
| `hud_coins.png` | `hud.c` | Coin count UI icon used in the HUD |
| `start_menu_logo.png` | `start_menu.c` | Game logo displayed on the start menu screen |

### Fonts — `assets/fonts/`

| File | Used By | Description |
|------|---------|-------------|
| `round9x13.ttf` | `hud.c` (`hud->font`) | Bitmap font for score and lives text in the HUD |

---

## Player Sprite Sheet -- `player.png`

**Sheet dimensions:** 192 x 288 px
**Grid:** 4 columns x 6 rows
**Frame size:** 48 x 48 px

### Animation Row Map

| Row | AnimState | Frame Count | Frame Duration | Notes |
|-----|-----------|-------------|----------------|-------|
| 0 | `ANIM_IDLE` | 4 | 150 ms/frame | Subtle breathing cycle |
| 1 | `ANIM_WALK` | 4 | 100 ms/frame | Looping run cycle |
| 2 | `ANIM_JUMP` | 2 | 150 ms/frame | Rising phase poses |
| 3 | `ANIM_FALL` | 1 | 200 ms/frame | Descent pose |
| 4 | `ANIM_CLIMB` | 2 | 100 ms/frame | Vine climbing cycle |
| 5 | *(unused)* | -- | -- | Available for future states |

### Frame Source Rect Formula

```c
frame.x = anim_frame_index * FRAME_W;   // column x 48
frame.y = ANIM_ROW[anim_state] * FRAME_H; // row x 48
```

### Horizontal Flipping

When `player->facing_left == 1`, the sprite is drawn with `SDL_FLIP_HORIZONTAL` via `SDL_RenderCopyEx`. This means the same right-facing animation frames are used for both directions -- no duplicate assets needed.

---

## Unused Assets

The following asset is stored in `assets/sprites/unused/` and is not loaded by the game. It is available as a reserve for future use.

| File | Description |
|------|-------------|
| `sky_background_0.png` | Sky gradient background |

Additional background variants are stored alongside used backgrounds in `assets/sprites/backgrounds/`:

| File | Description |
|------|-------------|
| `castle_pillars.png` | Castle/dungeon pillar background |
| `clouds_mg_1_lightened.png` | Lightened midground cloud variant |
| `forest_leafs.png` | Forest leaf layer |
| `glacial_mountains_lightened.png` | Lightened mountain variant |
| `sky_blue_lightened.png` | Lightened sky variant |

Additional tilesets are stored in `assets/sprites/levels/`:

| File | Description |
|------|-------------|
| `brick_platform.png` | Brick platform tile |
| `brick_tileset.png` | Brick wall / platform tile |
| `cloud_tileset.png` | Cloud platform tile |
| `grass_rock_platform.png` | Grass + rock platform tile |
| `grass_rock_tileset.png` | Grass + rock mixed tileset |
| `leaf_platform.png` | Leaf / foliage platform tile |
| `leaf_tileset.png` | Leaf / foliage platform tile |
| `stone_platform.png` | Stone platform tile |
| `stone_tileset.png` | Stone floor / wall tile |

---

## Sprite Sheet Analysis

To inspect any sprite sheet's exact dimensions and pixel layout:

```sh
python3 .agents/scripts/analyze_sprite.py assets/sprites/<category>/<sprite>.png
```

### Frame Math Reference

```
Sheet width  = cols x frame_w
Sheet height = rows x frame_h

source_x = (frame_index % cols) * frame_w
source_y = (frame_index / cols) * frame_h
```

---

## Loading an Asset

```c
// In game_init or an entity's init function:
SDL_Texture *tex = IMG_LoadTexture(gs->renderer, "assets/sprites/collectibles/coin.png");
if (!tex) {
    fprintf(stderr, "Failed to load coin.png: %s\n", IMG_GetError());
    exit(EXIT_FAILURE);
}

// At cleanup (reverse init order):
if (tex) { SDL_DestroyTexture(tex); tex = NULL; }
```
