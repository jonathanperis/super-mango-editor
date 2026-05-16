#include "game_overlay.h"

GameOverlayState game_overlay_state(const GameState *gs)
{
    if (!gs) return GAME_OVERLAY_NONE;
    if (gs->completion.complete) return GAME_OVERLAY_LEVEL_COMPLETE;
    if (gs->paused) return GAME_OVERLAY_PAUSED;
    return GAME_OVERLAY_NONE;
}

int game_overlay_blocks_update(const GameState *gs)
{
    return game_overlay_state(gs) != GAME_OVERLAY_NONE;
}
