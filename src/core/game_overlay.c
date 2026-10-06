#include "game_overlay.h"

#include "../screens/settings_menu.h" /* SettingsMenu.open */
#include "../shared/audio.h"          /* music_pause, music_resume */

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

int game_simulation_blocked(const GameState *gs)
{
    if (!gs) return 1;
    return game_overlay_blocks_update(gs) ||
           (gs->settings_menu && gs->settings_menu->open) ||
           !gs->running || gs->route != GAME_ROUTE_NONE;
}

int game_music_should_play(const GameState *gs)
{
    if (!gs) return 0;
    if (gs->settings_menu && gs->settings_menu->open) return 0;
    return game_overlay_state(gs) != GAME_OVERLAY_PAUSED;
}

void game_music_sync(const GameState *gs)
{
    /* Pausing an already paused stream (or resuming a playing one) is a
     * no-op in raylib, so callers may sync after any state change. */
    if (game_music_should_play(gs)) music_resume();
    else music_pause();
}
