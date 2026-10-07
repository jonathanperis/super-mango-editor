#pragma once

#include "../game.h"

typedef enum GameOverlayState {
    GAME_OVERLAY_NONE = 0,
    GAME_OVERLAY_PAUSED,
    GAME_OVERLAY_LEVEL_COMPLETE,
    GAME_OVERLAY_GAME_OVER
} GameOverlayState;

typedef enum GamePauseReason {
    GAME_PAUSE_REASON_PLAYER = 1u << 0,
    GAME_PAUSE_REASON_FOCUS  = 1u << 1
} GamePauseReason;

GameOverlayState game_overlay_state(const GameState *gs);
int game_overlay_blocks_update(const GameState *gs);
unsigned int game_overlay_pause_reasons(const GameState *gs);
void game_overlay_set_pause_reason(GameState *gs, unsigned int reason, int enabled);
void game_overlay_toggle_pause(GameState *gs);
void game_overlay_resume(GameState *gs);

/*
 * game_simulation_blocked — 1 when another screen owns this frame: a pause,
 * game-over or completion overlay, the settings panel, a stopped game or a
 * pending route. No gameplay step runs and touch taps belong to that screen.
 */
int game_simulation_blocked(const GameState *gs);

/*
 * game_music_should_play — the single rule for gameplay music.
 *
 * Music is silent while the pause overlay is up (player pause or lost window
 * focus) or while the settings panel is open; it keeps playing under the
 * level-complete and game-over screens. Every place that pauses or resumes
 * music asks this predicate instead of deciding from its own event, so a
 * focus change can no longer resume music underneath the settings panel.
 */
int game_music_should_play(const GameState *gs);

/* Pause or resume the music stream to match game_music_should_play(). */
void game_music_sync(const GameState *gs);

/*
 * game_audio_apply_settings — apply the player's mute and volume settings
 * (from the profile borrowed in gs->profile) to the sound effects and the
 * level music. Does nothing without a profile.
 *
 * Call it after anything that reloads the level music: loading the music
 * starts it at the level's own music_volume, which ignores a player who
 * muted or turned the music down.
 */
void game_audio_apply_settings(const GameState *gs);
