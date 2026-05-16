#pragma once

#include "../game.h"

/* Inject deterministic SDL key events for the current replay frame. */
void game_replay_inject_events(GameState *gs);
