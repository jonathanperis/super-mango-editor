# Assets

← [Home](home)

---

All visual assets live in the `assets/` directory. They are PNG files (loaded via `SDL2_image`) with one TrueType font.

> **Coordinate note:** All game objects use **logical space (400x300)**. SDL scales to the 800x600 OS window 2x. A 48x48 sprite appears as 96x96 physical pixels on screen.

---

## Currently Used Assets

| File | GameState Field / Used By | Description |
|------|---------------------------|-------------|
| `parallax_sky.png` | `parallax.c` (layer 0, speed 0.00) | Static sky gradient backdrop |
| `parallax_clouds_bg.png` | `parallax.c` (layer 1, speed 0.08) | Background cloud layer, slow scroll |
| `parallax_glacial_mountains.png` | `parallax.c` (layer 2, speed 0.15) | Distant mountains |
| `parallax_clouds_mg_3.png` | `parallax.c` (layer 3, speed 0.25) | Midground cloud layer 3 |
| `parallax_clouds_mg_2.png` | `parallax.c` (layer 4, speed 0.38) | Midground cloud layer 2 |
| `parallax_cloud_lonely.png` | `parallax.c` (layer 5, speed 0.44) | Single lonely cloud |
| `parallax_clouds_mg_1.png` | `parallax.c` (layer 6, speed 0.50) | Foreground cloud layer |
| `grass_tileset.png` | `gs->floor_tile` | 48x48 tile, 9-slice rendered across `FLOOR_Y` to form the floor |
| `platform.png` | `gs->platform_tex` | 48x48 tile, 9-slice rendered as one-way platform pillars |
| `player.png` | `player->texture` | 192x288 sprite sheet, 4 cols x 6 rows, 48x48 frames |
| `water.png` | `water->texture` | 384x64 sprite sheet, 8 frames of 48x64 with 16x31 art crop |
| `spider.png` | `gs->spider_tex` | Spider enemy sprite sheet (ground patrol) |
| `jumping_spider.png` | `gs->jumping_spider_tex` | Jumping spider enemy sprite sheet |
| `bird.png` | `gs->bird_tex` | Slow sine-wave bird enemy sprite sheet |
| `faster_bird.png` | `gs->faster_bird_tex` | Fast aggressive bird enemy sprite sheet |
| `fish.png` | `gs->fish_tex` | Jumping fish enemy sprite sheet |
| `faster_fish.png` | `gs->faster_fish_tex` | Faster fish enemy sprite sheet |
| `coin.png` | `gs->coin_tex` | 16x16 coin collectible sprite |
| `yellow_star.png` | `gs->yellow_star_tex` | Yellow star collectible sprite |
| `last_star.png` | `last_star.c` | Last star goal collectible sprite |
| `spike.png` | `gs->spike_tex` | Floor/ceiling spike hazard |
| `spike_block.png` | `gs->spike_block_tex` | Rail-riding rotating hazard sprite |
| `spike_platform.png` | `gs->spike_platform_tex` | Spiked platform hazard sprite |
| `circular_saw.png` | `gs->circular_saw_tex` | Rotating saw blade hazard |
| `axe_trap.png` | `gs->axe_trap_tex` | Swinging axe trap hazard |
| `blue_flame.png` | `gs->blue_flame_tex` | Blue flame hazard sprite |
| `float_platform.png` | `gs->float_platform_tex` | 48x16 sprite, 3-slice horizontal strip (left cap, centre fill, right cap) |
| `bridge.png` | `gs->bridge_tex` | 16x16 single-frame brick tile for crumble walkways |
| `bouncepad_small.png` | `gs->bouncepad_small_tex` | Small bouncepad sprite (low launch) |
| `bouncepad_medium.png` | `gs->bouncepad_medium_tex` | Medium bouncepad sprite (standard launch) |
| `bouncepad_high.png` | `gs->bouncepad_high_tex` | High bouncepad sprite (max launch) |
| `rail.png` | `gs->rail_tex` | 64x64 sprite sheet, 4x4 grid of 16x16 bitmask rail tiles |
| `vine.png` | `gs->vine_tex` | 16x48 single-frame plant sprite for climbable vines |
| `ladder.png` | `gs->ladder_tex` | Climbable ladder sprite |
| `rope.png` | `gs->rope_tex` | Climbable rope sprite |
| `fog_background_1.png` | `fog.c` (`fog->textures[0]`) | Fog overlay layer, semi-transparent sliding effect |
| `fog_background_2.png` | `fog.c` (`fog->textures[1]`) | Fog overlay layer, semi-transparent sliding effect |
| `hud_coins.png` | `hud.c` | Coin count UI icon used in the HUD |
| `start_menu_logo.png` | `start_menu.c` | Game logo displayed on the start menu screen |
| `round9x13.ttf` | `hud.c` (`hud->font`) | Bitmap font for score and lives text in the HUD |

---

## Player Sprite Sheet — `player.png`

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

The following assets are stored in `assets/unused/` and are not loaded by the game. They are available as reserves for future use.

| File | Description |
|------|-------------|
| `brick_oneway.png` | One-way brick platform tile |
| `brick_tileset.png` | Brick wall / platform tile |
| `castle_background_0.png` | Castle/dungeon interior background |
| `cloud_tileset.png` | Cloud platform tile |
| `clouds.png` | Decorative cloud layer |
| `clouds_mg_1_lightened.png` | Lightened midground cloud variant |
| `flame_1.png` | Flame hazard variant |
| `forest_background_0.png` | Forest scene background |
| `glacial_mountains_lightened.png` | Lightened mountain variant |
| `grass_rock_oneway.png` | One-way grass + rock platform |
| `grass_rock_tileset.png` | Grass + rock mixed tileset |
| `lava.png` | Lava hazard tile |
| `leaf_tileset.png` | Leaf / foliage platform tile |
| `sky_background_0.png` | Sky gradient background |
| `sky_lightened.png` | Lightened sky variant |
| `star_green.png` | Green star collectible |
| `star_red.png` | Red star collectible |
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
SDL_Texture *tex = IMG_LoadTexture(gs->renderer, "assets/coin.png");
if (!tex) {
    fprintf(stderr, "Failed to load coin.png: %s\n", IMG_GetError());
    exit(EXIT_FAILURE);
}

// At cleanup (reverse init order):
if (tex) { SDL_DestroyTexture(tex); tex = NULL; }
```
