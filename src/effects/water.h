/*
 * water.h — Public interface for the animated water strip module.
 *
 * Sprite facts (Water.png, 384×64):
 *   - 8 animation frames, each 48×64 px in the sheet.
 *   - Actual art: x=16..31, y=17..47 per frame (16×31 px visible area).
 *   - 16 px transparent padding on both left AND right of every frame.
 *
 * Rendering strategy:
 *   - Source crop per tile: { frame*48+16, 17, 16, 31 }
 *   - Tiles placed every 16 px, cycling through frames 0-7 in order.
 *   - Pattern period = 8 x 16 = 128 px; scroll wraps at 128 px.
 *   - Scroll is rightward — continuous wave motion, no transparent gaps.
 */
#pragma once

#include <SDL.h>

#define WATER_FRAMES        8    /* frames in the sheet                    */
#define WATER_FRAME_W      48    /* full slot width in the sheet (px)      */
#define WATER_ART_DX       16    /* left offset to art within each slot    */
#define WATER_ART_W        16    /* width of actual art pixels per frame   */
#define WATER_ART_Y        17    /* first visible row                      */
#define WATER_ART_H        31    /* height of visible art                  */
#define WATER_PERIOD      (WATER_FRAMES * WATER_ART_W)  /* 128 px repeat  */
#define WATER_SCROLL_SPEED 40.0f /* rightward px/s                         */

typedef struct {
    SDL_Texture *texture;   /* GPU handle for Water.png                   */
    float        scroll_x;  /* rightward offset, wraps at WATER_PERIOD    */
} Water;

void water_init(Water *w, SDL_Renderer *renderer);
void water_reload_texture(Water *w, SDL_Renderer *renderer, const char *path);
void water_update(Water *w, float dt);
void water_render(const Water *w, SDL_Renderer *renderer);
void water_cleanup(Water *w);

/* Draw the animated water strip across the bottom of the canvas. */
void water_render(const Water *w, SDL_Renderer *renderer);

/* Release the GPU texture. */
void water_cleanup(Water *w);
