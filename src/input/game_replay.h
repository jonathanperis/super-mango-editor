/*
 * game_replay.h — Scripted input replay for smoke tests (--replay).
 *
 * A script lists (frame, key, action) events; they are parsed once at start-up
 * and injected on their frame as if the player had pressed the keys.
 */
#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */

typedef struct GameReplayEvent {
    int frame;
    int key;
    char action[16];
} GameReplayEvent;

/* Parse once at startup. A requested but invalid replay is a startup error. */
int game_replay_load(GameState *gs);
void game_replay_cleanup(GameState *gs);

/* Inject deterministic commands for the current replay frame. */
void game_replay_inject_events(GameState *gs);
