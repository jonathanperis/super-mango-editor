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

#include <SDL.h>   /* SDL_Renderer, SDL_Texture, SDL_Rect */

/*
 * MAX_PLATFORMS — upper bound on how many platforms the game can hold.
 * Stored as a fixed-size array inside GameState; no dynamic allocation needed.
 */
#define MAX_PLATFORMS 8

/*
 * Platform — a rectangular one-way surface built from tiled 48×48 grass blocks.
 *
 * `x` and `y` mark the top-left corner of the platform in logical (400×300)
 * coordinates.  `y` is specifically the TOP SURFACE — the Y value a player's
 * feet must cross to trigger a landing.
 *
 * `w` and `h` are set at init time and never change during a play session.
 */
typedef struct {
    float x;   /* left edge of the platform in logical pixels   */
    float y;   /* top  edge (landing surface) in logical pixels */
    int   w;   /* total width  in logical pixels                */
    int   h;   /* total height in logical pixels                */
} Platform;

/*
 * platforms_init — Populate the platform array with the two pillar definitions.
 *
 * Writes into `platforms[]` and sets *count to the number of platforms placed.
 * Called once from game_init before the game loop starts.
 */
void platforms_init(Platform *platforms, int *count);

/*
 * platforms_render — Draw every platform using the supplied tile texture.
 *
 * Tiles the 48×48 grass texture vertically to fill each pillar's height.
 * cam_x is the camera left-edge offset (world px); subtract it from every
 * dst.x to convert world coordinates to screen coordinates.
 * Called every frame from game_loop, after the background, before the player.
 */
void platforms_render(const Platform *platforms, int count,
                      SDL_Renderer *renderer, SDL_Texture *tex, int cam_x);
