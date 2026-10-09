/* Screen-local logical target. AppSession owns the process-wide raylib window. */
#include "game_window.h"
#include "../game.h"  /* GameState: this file reads its fields */
#include <stdio.h>

int game_window_init(GameState *gs)
{
    /* A render target is an off-screen color texture plus a framebuffer.
     * This screen owns it, while AppSession owns the actual OS window. */
    if (!IsWindowReady()) return -1;
    gs->screen.frame_target = LoadRenderTexture(GAME_W, GAME_H);
    if (!IsRenderTextureValid(gs->screen.frame_target)) {
        fprintf(stderr, "Could not create gameplay render target\n");
        return -1;
    }
    SetTextureFilter(gs->screen.frame_target.texture, TEXTURE_FILTER_POINT);
    return 0;
}

void game_window_cleanup(GameState *gs)
{
    /* Release only this screen's target. Closing the shared window here
     * would invalidate the next menu/game screen's textures and callbacks. */
    if (IsRenderTextureValid(gs->screen.frame_target))
        UnloadRenderTexture(gs->screen.frame_target);
    gs->screen.frame_target = (RenderTexture2D){0};
}
