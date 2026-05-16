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
