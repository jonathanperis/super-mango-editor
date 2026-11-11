/* Screen-local logical target. AppSession owns the process-wide raylib window. */
#include "game_window.h"
#include "../game.h"  /* GameState: this file reads its fields */
#include "../shared/graphics.h"  /* display_viewport */
#include <stdio.h>

/*
 * The canvas is GAME_W x GAME_H logical pixels, and everything is still
 * placed in those units. But it is stored at the density the window shows
 * it: a 400x300 canvas shown 800x600 on a Retina display is 1600x1200 real
 * pixels, so the canvas is made that size (scale 4) and drawn through a
 * camera that magnifies by 4.
 *
 * Pixel art looks the same either way: a texel becomes a 4x4 block whether
 * the magnifying happens here or when the finished canvas is stretched.
 * What changes is everything that is not on the pixel grid: the slanted
 * walls of the 2.5D view (shared/depth.h), the soft light and haze. Those
 * get four times the detail in each direction instead of being drawn at
 * 400x300 and blown up.
 */
#define FRAME_SCALE_MAX 6   /* 2400x1800: enough for a 4K monitor */

/* How many real pixels one logical pixel covers in the window right now. */
static int wanted_scale(void)
{
#ifdef MANGO_RAYLIB_MEMORY
    /* The test backend draws in software; keep its canvas small. */
    return 1;
#else
    Rectangle view = display_viewport(GetScreenWidth(), GetScreenHeight(), GAME_W, GAME_H);
    float density = GetWindowScaleDPI().x;
    if (!(density >= 1.0f)) density = 1.0f;
    /* Round up, so a window between two whole scales is drawn sharper
     * than shown, never coarser. */
    int scale = (int)(view.width * density / (float)GAME_W + 0.999f);
    if (scale < 1) scale = 1;
    if (scale > FRAME_SCALE_MAX) scale = FRAME_SCALE_MAX;
    return scale;
#endif
}

static RenderTexture2D make_target(int scale)
{
    RenderTexture2D target = LoadRenderTexture(GAME_W * scale, GAME_H * scale);
    /* Shown one-to-one this filter changes nothing; in a window between
     * two whole scales it shrinks the canvas smoothly. */
    if (IsRenderTextureValid(target)) SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    return target;
}

int game_window_init(GameState *gs)
{
    /* A render target is an off-screen color texture plus a framebuffer.
     * This screen owns it, while AppSession owns the actual OS window. */
    if (!IsWindowReady()) return -1;
    int scale = wanted_scale();
    gs->screen.frame_target = make_target(scale);
    if (!IsRenderTextureValid(gs->screen.frame_target)) {
        fprintf(stderr, "Could not create gameplay render target\n");
        return -1;
    }
    gs->screen.frame_scale = scale;
    return 0;
}

int game_window_fit(GameState *gs)
{
    int scale = wanted_scale();
    if (scale == gs->screen.frame_scale && IsRenderTextureValid(gs->screen.frame_target)) return 1;
    /* The window was resized, went fullscreen or moved to another display. */
    RenderTexture2D target = make_target(scale);
    if (!IsRenderTextureValid(target)) return IsRenderTextureValid(gs->screen.frame_target);
    if (IsRenderTextureValid(gs->screen.frame_target)) UnloadRenderTexture(gs->screen.frame_target);
    gs->screen.frame_target = target;
    gs->screen.frame_scale = scale;
    return 1;
}

void game_window_cleanup(GameState *gs)
{
    /* Release only this screen's target. Closing the shared window here
     * would invalidate the next menu/game screen's textures and callbacks. */
    if (IsRenderTextureValid(gs->screen.frame_target))
        UnloadRenderTexture(gs->screen.frame_target);
    gs->screen.frame_target = (RenderTexture2D){0};
    gs->screen.frame_scale = 0;
}
