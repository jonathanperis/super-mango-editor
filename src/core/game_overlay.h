#pragma once

#include "../game.h"

typedef enum GameOverlayState {
    GAME_OVERLAY_NONE = 0,
    GAME_OVERLAY_PAUSED,
    GAME_OVERLAY_LEVEL_COMPLETE,
    GAME_OVERLAY_GAME_OVER
} GameOverlayState;

GameOverlayState game_overlay_state(const GameState *gs);
int game_overlay_blocks_update(const GameState *gs);
