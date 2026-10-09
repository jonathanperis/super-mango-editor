#include "game_overlay.h"
#include "../game.h"  /* GameState: this file reads its fields */

#include "../screens/settings_menu.h" /* SettingsMenu.open */
#include "../shared/audio.h"          /* music_pause, music_resume */
#include "../levels/level.h"          /* LevelDef.music_volume */
#include "game_profile.h"             /* GameProfile, GameSettings */

static void sync_pause_flag(GameState *gs)
{
    if (!gs) return;
    gs->screen.paused = gs->screen.pause_reasons != 0;
}

GameOverlayState game_overlay_state(const GameState *gs)
{
    if (!gs) return GAME_OVERLAY_NONE;
    if (gs->screen.completion.complete) return GAME_OVERLAY_LEVEL_COMPLETE;
    if (gs->screen.game_over) return GAME_OVERLAY_GAME_OVER;
    if (gs->screen.paused || gs->screen.pause_reasons != 0) return GAME_OVERLAY_PAUSED;
    return GAME_OVERLAY_NONE;
}

int game_overlay_blocks_update(const GameState *gs)
{
    return game_overlay_state(gs) != GAME_OVERLAY_NONE;
}

unsigned int game_overlay_pause_reasons(const GameState *gs)
{
    if (!gs) return 0;
    if (gs->screen.pause_reasons == 0 && gs->screen.paused) return GAME_PAUSE_REASON_PLAYER;
    return gs->screen.pause_reasons;
}

void game_overlay_set_pause_reason(GameState *gs, unsigned int reason, int enabled)
{
    if (!gs) return;
    if (enabled) {
        gs->screen.pause_reasons |= reason;
    } else {
        gs->screen.pause_reasons &= ~reason;
    }
    sync_pause_flag(gs);
}

void game_overlay_toggle_pause(GameState *gs)
{
    if (!gs) return;
    if (game_overlay_state(gs) == GAME_OVERLAY_LEVEL_COMPLETE) return;
    if (game_overlay_state(gs) == GAME_OVERLAY_GAME_OVER) return;
    game_overlay_set_pause_reason(gs, GAME_PAUSE_REASON_PLAYER,
                                  (gs->screen.pause_reasons & GAME_PAUSE_REASON_PLAYER) == 0);
}

void game_overlay_resume(GameState *gs)
{
    if (!gs) return;
    if (game_overlay_state(gs) != GAME_OVERLAY_PAUSED) return;
    if (gs->screen.pause_reasons == 0 && gs->screen.paused) {
        gs->screen.paused = 0;
        return;
    }
    game_overlay_set_pause_reason(gs, GAME_PAUSE_REASON_PLAYER, 0);
}

int game_simulation_blocked(const GameState *gs)
{
    if (!gs) return 1;
    return game_overlay_blocks_update(gs) ||
           (gs->screen.settings_menu && gs->screen.settings_menu->open) ||
           !gs->screen.running || gs->screen.route != GAME_ROUTE_NONE;
}

int game_music_should_play(const GameState *gs)
{
    if (!gs) return 0;
    if (gs->screen.settings_menu && gs->screen.settings_menu->open) return 0;
    return game_overlay_state(gs) != GAME_OVERLAY_PAUSED;
}

void game_music_sync(const GameState *gs)
{
    /* Pausing an already paused stream (or resuming a playing one) is a
     * no-op in raylib, so callers may sync after any state change. */
    if (game_music_should_play(gs)) music_resume();
    else music_pause();
}

void game_audio_apply_settings(const GameState *gs)
{
    if (!gs || !gs->screen.profile) return;
    const GameSettings *s = &gs->screen.profile->data.settings;
    int volume = s->muted ? 0 : s->effects_volume;

    sound_set_volume(gs->assets.audio.coin, volume);
    sound_set_volume(gs->assets.audio.jump, volume);
    sound_set_volume(gs->assets.audio.hit, volume);
    sound_set_volume(gs->assets.audio.spring, volume);
    sound_set_volume(gs->assets.audio.axe, volume);
    sound_set_volume(gs->assets.audio.flap, volume);
    sound_set_volume(gs->assets.audio.spider_attack, volume);
    sound_set_volume(gs->assets.audio.dive, volume);

    /* The settings slider scales the level's authored volume (0-128). */
    const LevelDef *level = gs->world.runtime.current_level;
    if (level)
        music_set_volume(s->muted ? 0 : level->music_volume * s->music_volume / 128);
}
