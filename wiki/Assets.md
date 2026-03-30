# Assets

← [Home](Home)

---

All visual assets live in the `assets/` directory. They are PNG files (loaded via `SDL2_image`) with one TrueType font.

> **Coordinate note:** All game objects use **logical space (400×300)**. SDL scales to the 800×600 OS window 2×. A 48×48 sprite appears as 96×96 physical pixels on screen.

---

## Currently Used Assets

| File | Used By | Description |
|------|---------|-------------|
| `Parallax/sky.png` | `parallax.c` (layer 0, speed 0.00) | Static sky gradient backdrop |
| `Parallax/clouds_bg.png` | `parallax.c` (layer 1, speed 0.08) | Background cloud layer, slow scroll |
| `Parallax/glacial_mountains.png` | `parallax.c` (layer 2, speed 0.15) | Distant mountains |
| `Parallax/clouds_mg_3.png` | `parallax.c` (layer 3, speed 0.25) | Midground cloud layer 3 |
| `Parallax/clouds_mg_2.png` | `parallax.c` (layer 4, speed 0.38) | Midground cloud layer 2 |
| `Parallax/cloud_lonely.png` | `parallax.c` (layer 5, speed 0.44) | Single lonely cloud |
| `Parallax/clouds_mg_1.png` | `parallax.c` (layer 6, speed 0.50) | Foreground cloud layer |
| `Grass_Tileset.png` | `game.c` (`gs->floor_tile`) | 48×48 tile, 9-slice rendered across `FLOOR_Y` to form the floor |
| `Grass_Oneway.png` | `game.c` (`gs->platform_tex`) | 48×48 tile, 9-slice rendered as one-way platform pillars |
| `Player.png` | `player.c` (`player->texture`) | 192×288 sprite sheet, 4 cols × 6 rows, 48×48 frames |
| `Water.png` | `water.c` (`water->texture`) | 384×64 sprite sheet, 8 frames of 48×64 with 16×31 art crop |
| `Bouncepad_Wood.png` | `game.c` (`gs->bouncepad_tex`) | 144×48 sprite sheet, 3 frames of 48×48 (spring pad animation) |
| `Rails.png` | `game.c` (`gs->rail_tex`) | 64×64 sprite sheet, 4×4 grid of 16×16 bitmask rail tiles |
| `Spike_Block.png` | `game.c` (`gs->spike_block_tex`) | 16×16 single-frame rotating hazard sprite |
| `Spider_1.png` | `game.c` (`gs->spider_tex`) | 192×48 sprite sheet, 3 frames of 64×48 with 10 px art band |
| `Coin.png` | `coin.c` (`gs->coin_tex`) | 16×16 coin collectible sprite |
| `Platform.png` | `game.c` (`gs->float_platform_tex`) | 48×16 sprite, 3-slice horizontal strip (left cap, centre fill, right cap) |
| `Bridge.png` | `game.c` (`gs->bridge_tex`) | 16×16 single-frame brick tile for crumble walkways |
| `Spider_2.png` | `game.c` (`gs->jumping_spider_tex`) | 192×48 sprite sheet, 3 frames of 64×48 (jumping spider enemy) |
| `Bird_2.png` | `game.c` (`gs->bird_tex`) | 144×48 sprite sheet, 3 frames of 48×48 (slow sine-wave bird) |
| `Bird_1.png` | `game.c` (`gs->faster_bird_tex`) | 144×48 sprite sheet, 3 frames of 48×48 (fast aggressive bird) |
| `Vine.png` | `vine.c` (`gs->vine_tex`) | 16×48 single-frame plant sprite for climbable vines |
| `Fish_2.png` | `fish.c` (`gs->fish_tex`) | 96×48 sprite sheet, 2 frames of 48×48 (swim animation) |
| `Sky_Background_1.png` | `fog.c` (`fog->textures[0]`) | Fog overlay layer, semi-transparent sliding effect |
| `Sky_Background_2.png` | `fog.c` (`fog->textures[1]`) | Fog overlay layer, semi-transparent sliding effect |
| `Stars_Ui.png` | `hud.c` (`hud->star_tex`) | Heart/health indicator icon used in the HUD |
| `Round9x13.ttf` | `hud.c` (`hud->font`) | Bitmap font for score and lives text in the HUD |

---

## Player Sprite Sheet — `Player.png`

**Sheet dimensions:** 192 × 288 px  
**Grid:** 4 columns × 6 rows  
**Frame size:** 48 × 48 px

### Animation Row Map

| Row | AnimState | Frame Count | Frame Duration | Notes |
|-----|-----------|-------------|----------------|-------|
| 0 | `ANIM_IDLE` | 4 | 150 ms/frame | Subtle breathing cycle |
| 1 | `ANIM_WALK` | 4 | 100 ms/frame | Looping run cycle |
| 2 | `ANIM_JUMP` | 2 | 150 ms/frame | Rising phase poses |
| 3 | `ANIM_FALL` | 1 | 200 ms/frame | Descent pose |
| 4 | `ANIM_CLIMB` | 2 | 100 ms/frame | Vine climbing cycle |
| 5 | *(unused)* | — | — | Available for future states |

### Frame Source Rect Formula

```c
frame.x = anim_frame_index * FRAME_W;   // column × 48
frame.y = ANIM_ROW[anim_state] * FRAME_H; // row × 48
```

### Horizontal Flipping

When `player->facing_left == 1`, the sprite is drawn with `SDL_FLIP_HORIZONTAL` via `SDL_RenderCopyEx`. This means the same right-facing animation frames are used for both directions — no duplicate assets needed.

---

## Tileset Assets (Available, Not Yet In Use)

These are included in `assets/` and ready for future use in platformer levels.

### Background Layers

| File | Description |
|------|-------------|
| `Forest_Background_0.png` | Forest scene (not currently in use) |
| `Sky_Background_0.png` | Sky gradient, layer 0 |
| `Sky_Background_1.png` | ✅ In use — fog overlay texture |
| `Sky_Background_2.png` | ✅ In use — fog overlay texture |
| `Castle_Background_0.png` | Castle/dungeon interior background |
| `Clouds.png` | Decorative cloud layer |

### Ground / Platform Tilesets

| File | Description |
|------|-------------|
| `Grass_Tileset.png` | ✅ In use — 9-slice floor tile (48×48) |
| `Grass_Oneway.png` | ✅ In use — one-way platform pillar tile |
| `Grass_Rock_Tileset.png` | Grass + rock mixed tileset |
| `Grass_Rock_Oneway.png` | One-way rock platform |
| `Brick_Tileset.png` | Brick wall / platform tile |
| `Brick_Oneway.png` | One-way brick platform |
| `Stone_Tileset.png` | Stone floor / wall tile |
| `Cloud_Tileset.png` | Cloud platform tile |
| `Leaf_Tileset.png` | Leaf / foliage platform tile |
| `Platform.png` | ✅ In use — `float_platform.c` 3-slice hovering surfaces |
| `Spike_Platform.png` | Spiked platform (hazard) |
| `Bridge.png` | ✅ In use — `bridge.c` tiled crumble walkway |
| `Ladder.png` | Climbable ladder |
| `Rails.png` | ✅ In use — `rail.c` bitmask tile track system |
| `Rope.png` | Rope (climbable) |
| `Vine.png` | ✅ In use — `vine.c` climbable plant decoration |
| `Water.png` | ✅ In use — animated water surface strip |
| `Lava.png` | Lava hazard tile |

### Collectibles

| File | Description |
|------|-------------|
| `Coin.png` | ✅ In use — `coin.c` collectible sprite (16×16) |
| `Coins_Ui.png` | Coin count UI icon |
| `Star_Green.png` | Green star collectible |
| `Star_Red.png` | Red star collectible |
| `Star_Yellow.png` | Yellow star collectible |
| `Stars_Ui.png` | ✅ In use — `hud.c` heart/health indicator icon |

### Enemies

| File | Description |
|------|-------------|
| `Bird_1.png` | ✅ In use — `faster_bird.c` fast aggressive sine-wave sky patrol |
| `Bird_2.png` | ✅ In use — `bird.c` slow sine-wave sky patrol |
| `Fish_1.png` | Fish enemy, variant 1 |
| `Fish_2.png` | ✅ In use — `fish.c` jumping water enemy |
| `Spider_1.png` | ✅ In use — ground patrol spider enemy |
| `Spider_2.png` | ✅ In use — `jumping_spider.c` jumping patrol enemy |

### Hazards / Traps

| File | Description |
|------|-------------|
| `Axe_Trap.png` | Swinging axe hazard |
| `Circular_Saw.png` | Rotating saw blade hazard |
| `Spike.png` | Floor/ceiling spike |
| `Spike_Block.png` | ✅ In use — `spike_block.c` rail-riding rotating hazard |
| `Flame_1.png` | Flame hazard, variant 1 |
| `Flame_2.png` | Flame hazard, variant 2 |

### Bounce / Movement Pads

| File | Description |
|------|-------------|
| `Bouncepad_Green.png` | Green bounce pad |
| `Bouncepad_Red.png` | Red bounce pad |
| `Bouncepad_Wood.png` | ✅ In use — `bouncepad.c` spring launch pad |

### UI / Branding

| File | Description |
|------|-------------|
| `Logo.png` | Game logo graphic |
| `Coins_Ui.png` | Coin counter icon |
| `Stars_Ui.png` | ✅ In use — `hud.c` heart/health indicator icon |
| `Round9x13.ttf` | ✅ In use — `hud.c` bitmap font for score/HUD text |

---

## Sprite Sheet Analysis

To inspect any sprite sheet's exact dimensions and pixel layout:

```sh
python3 .github/skills/pixelart-game-designer/scripts/analyze_sprite.py assets/<sprite>.png
```

### Frame Math Reference

```
Sheet width  = cols × frame_w
Sheet height = rows × frame_h

source_x = (frame_index % cols) * frame_w
source_y = (frame_index / cols) * frame_h
```

---

## Loading an Asset

```c
// In game_init or an entity's init function:
SDL_Texture *tex = IMG_LoadTexture(gs->renderer, "assets/Coin.png");
if (!tex) {
    fprintf(stderr, "Failed to load Coin.png: %s\n", IMG_GetError());
    exit(EXIT_FAILURE);
}

// At cleanup (reverse init order):
if (tex) { SDL_DestroyTexture(tex); tex = NULL; }
```
