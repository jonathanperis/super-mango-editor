#pragma once

#include "../game.h"

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
