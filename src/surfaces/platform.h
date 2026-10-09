/*
 * platform.h — Public interface for the platform module.
 *
 * Defines the Platform struct (a one-way elevated surface) and declares
 * the functions that manage a set of platforms: init and render.
 *
 * One-way means the player can jump through from below and land only
 * on the top surface — the classic "pass-through" platform behaviour.
 */
#pragma once

#include "../shared/graphics.h"

/*
 * MAX_PLATFORMS — upper bound on how many platforms the game can hold.
 * Stored as a fixed-size array inside GameState; no dynamic allocation needed.
 */
#define MAX_PLATFORMS 32

/*
 * Platform — a rectangular one-way surface built from tiled 48×48 blocks.
 *
 * `x` and `y` mark the top-left corner of the platform in logical (400×300)
 * coordinates.  `y` is specifically the TOP SURFACE — the Y value a player's
 * feet must cross to trigger a landing.
 *
 * `w` and `h` are set at init time and never change during a play session.
 *
 * `tex` is the 9-slice tileset texture for this platform.  If NULL, the
 * renderer falls back to the level's default floor tile texture.  It is
 * borrowed: GameState.world.platform_tiles owns one texture per tile path, shared
 * by every platform that names it, so a Platform never unloads `tex`.
 */
typedef struct {
    float       x;   /* left edge of the platform in logical pixels   */
    float       y;   /* top  edge (landing surface) in logical pixels */
    int         w;   /* total width  in logical pixels                */
    int         h;   /* total height in logical pixels                */
    Texture2D *tex; /* borrowed 9-slice tileset (NULL = use default) */
} Platform;

/*
 * platforms_render — Draw every platform using the supplied tile texture.
 *
 * Tiles the 48×48 grass texture vertically to fill each pillar's height.
 * cam_x is the camera left-edge offset (world px); subtract it from every
 * dst.x to convert world coordinates to screen coordinates.
 * Called every frame from game_render_frame, after the background, before the player.
 */
void platforms_render(const Platform *platforms, int count,
                      Texture2D *tex, int cam_x);
