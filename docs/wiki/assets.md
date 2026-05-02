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

Two visual elements are configurable per TOML level file, allowing different themes without changing code:

### Floor Tile

```toml
floor_tile_path = "assets/sprites/levels/grass_tileset.png"
```

Any 48×48 PNG tile can be substituted here. The engine 9-slice renders it across `FLOOR_Y` to form the ground.

### Parallax Background Layers

```toml
[background_layers]
path  = "assets/sprites/backgrounds/sky_blue.png"
speed = 0.0

[background_layers]
path  = "assets/sprites/backgrounds/glacial_mountains.png"
speed = 0.2
```

Layers are drawn in array order (first = furthest back). `speed` is the parallax scroll factor: `0.0` = static sky, `1.0` = locks to camera, `0.1`–`0.5` = typical mid-ground parallax. Up to 8 layers are supported. See [Level Design](#level-design) for the full list of available background images and suggested speeds.

---

## Currently Used Assets

### Backgrounds — `assets/sprites/backgrounds/`

| File | Used By | Description |
|------|---------|-------------|
| `sky_blue.png` | `parallax.c` (layer 0, speed 0.00) | Static sky gradient backdrop |
| `clouds_bg.png` | `parallax.c` (layer 1, speed 0.08) | Background cloud layer, slow scroll |
| `glacial_mountains.png` | `parallax.c` (layer 2, speed 0.15) | Distant mountains |
| `clouds_mg_3.png` | `parallax.c` (layer 3, speed 0.25) | Midground cloud layer 3 |
| `clouds_mg_2.png` | `parallax.c` (layer 4, speed 0.38) | Midground cloud layer 2 |
| `clouds_lonely.png` | `parallax.c` (layer 5, speed 0.44) | Single lonely cloud |
| `clouds_mg_1.png` | `parallax.c` (layer 6, speed 0.50) | Foreground cloud layer |

### Foregrounds — `assets/sprites/foregrounds/`

| File | Used By | Description |
|------|---------|-------------|
| `fog_1.png` | `fog.c` (`fog->textures[0]`) | Fog overlay layer, semi-transparent sliding effect |
| `fog_2.png` | `fog.c` (`fog->textures[1]`) | Fog overlay layer, semi-transparent sliding effect |
| `water.png` | `water->texture` | 384x64 sprite sheet, 8 frames of 48x64 with 16x31 art crop |
| `lava.png` | `lava->texture` | Lava hazard foreground |
| `clouds.png` | foreground clouds | Decorative cloud layer |

### Player — `assets/sprites/player/`

| File | Used By | Description |
|------|---------|-------------|
| `player.png` | `player->texture` | 192x288 sprite sheet, 4 cols x 6 rows, 48x48 frames |

### Entities — `assets/sprites/entities/`

| File | Used By | Description |
|------|---------|-------------|
| `spider.png` | `gs->spider_tex` | Spider enemy sprite sheet (ground patrol) |
| `jumping_spider.png` | `gs->jumping_spider_tex` | Jumping spider enemy sprite sheet |
| `bird.png` | `gs->bird_tex` | Slow sine-wave bird enemy sprite sheet |
| `faster_bird.png` | `gs->faster_bird_tex` | Fast aggressive bird enemy sprite sheet |
| `fish.png` | `gs->fish_tex` | Jumping fish enemy sprite sheet |
| `faster_fish.png` | `gs->faster_fish_tex` | Faster fish enemy sprite sheet |

### Collectibles — `assets/sprites/collectibles/`

| File | Used By | Description |
|------|---------|-------------|
| `coin.png` | `gs->coin_tex` | 16x16 coin collectible sprite |
| `star_yellow.png` | `gs->star_yellow_tex` | Yellow star collectible sprite |
| `star_green.png` | `gs->star_green_tex` | Green star collectible sprite |
| `star_red.png` | `gs->star_red_tex` | Red star collectible sprite |
| `last_star.png` | `last_star.c` | Last star goal collectible sprite |

### Hazards — `assets/sprites/hazards/`

| File | Used By | Description |
|------|---------|-------------|
| `spike.png` | `gs->spike_tex` | Floor/ceiling spike hazard |
| `spike_block.png` | `gs->spike_block_tex` | Rail-riding rotating hazard sprite |
| `spike_platform.png` | `gs->spike_platform_tex` | Spiked platform hazard sprite |
| `circular_saw.png` | `gs->circular_saw_tex` | Rotating saw blade hazard |
| `axe_trap.png` | `gs->axe_trap_tex` | Swinging axe trap hazard |
| `blue_flame.png` | `gs->blue_flame_tex` | Blue flame hazard sprite |
| `fire_flame.png` | `gs->fire_flame_tex` | Fire flame hazard sprite |

### Surfaces — `assets/sprites/surfaces/`

| File | Used By | Description |
|------|---------|-------------|
| `float_platform.png` | `gs->float_platform_tex` | 48x16 sprite, 3-slice horizontal strip (left cap, centre fill, right cap) |
| `bridge.png` | `gs->bridge_tex` | 16x16 single-frame brick tile for crumble walkways |
| `bouncepad_small.png` | `gs->bouncepad_small_tex` | Small bouncepad sprite (low launch) |
| `bouncepad_medium.png` | `gs->bouncepad_medium_tex` | Medium bouncepad sprite (standard launch) |
| `bouncepad_high.png` | `gs->bouncepad_high_tex` | High bouncepad sprite (max launch) |
| `rail.png` | `gs->rail_tex` | 64x64 sprite sheet, 4x4 grid of 16x16 bitmask rail tiles |
| `vine.png` | `gs->vine_tex` | 16x48 single-frame plant sprite for climbable vines |
| `ladder.png` | `gs->ladder_tex` | Climbable ladder sprite |
| `rope.png` | `gs->rope_tex` | Climbable rope sprite |

### Levels — `assets/sprites/levels/`

| File | Used By | Description |
|------|---------|-------------|
| `grass_tileset.png` | `gs->floor_tile` | 48x48 tile, 9-slice rendered across `FLOOR_Y` to form the floor |
| `grass_platform.png` | `gs->platform_tex` | 48x48 tile, 9-slice rendered as one-way platform pillars |

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
| `leaf_tileset.png` | Leaf / foliage platform tile |
| `stone_tileset.png` | Stone floor / wall tile |

---

## Sprite Sheet Analysis

To inspect any sprite sheet's exact dimensions and pixel layout:

```sh
python3 .claude/scripts/analyze_sprite.py assets/<sprite>.png
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
