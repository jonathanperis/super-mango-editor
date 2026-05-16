#include "game_overlay.h"

static void sync_pause_flag(GameState *gs)
{
    if (!gs) return;
    gs->paused = gs->pause_reasons != 0;
}

GameOverlayState game_overlay_state(const GameState *gs)
{
    if (!gs) return GAME_OVERLAY_NONE;
    if (gs->completion.complete) return GAME_OVERLAY_LEVEL_COMPLETE;
    if (gs->game_over) return GAME_OVERLAY_GAME_OVER;
    if (gs->paused || gs->pause_reasons != 0) return GAME_OVERLAY_PAUSED;
    return GAME_OVERLAY_NONE;
}

int game_overlay_blocks_update(const GameState *gs)
{
    return game_overlay_state(gs) != GAME_OVERLAY_NONE;
}

unsigned int game_overlay_pause_reasons(const GameState *gs)
{
    if (!gs) return 0;
    if (gs->pause_reasons == 0 && gs->paused) return GAME_PAUSE_REASON_PLAYER;
    return gs->pause_reasons;
}

void game_overlay_set_pause_reason(GameState *gs, unsigned int reason, int enabled)
{
    if (!gs) return;
    if (enabled) {
        gs->pause_reasons |= reason;
    } else {
        gs->pause_reasons &= ~reason;
    }
    sync_pause_flag(gs);
}

void game_overlay_toggle_pause(GameState *gs)
{
    if (!gs) return;
    if (game_overlay_state(gs) == GAME_OVERLAY_LEVEL_COMPLETE) return;
    if (game_overlay_state(gs) == GAME_OVERLAY_GAME_OVER) return;
    game_overlay_set_pause_reason(gs, GAME_PAUSE_REASON_PLAYER,
                                  (gs->pause_reasons & GAME_PAUSE_REASON_PLAYER) == 0);
}

void game_overlay_resume(GameState *gs)
{
    if (!gs) return;
    if (game_overlay_state(gs) != GAME_OVERLAY_PAUSED) return;
    if (gs->pause_reasons == 0 && gs->paused) {
        gs->paused = 0;
        return;
    }
    game_overlay_set_pause_reason(gs, GAME_PAUSE_REASON_PLAYER, 0);
}
