/*
 * game_replay.h — Scripted key presses for smoke tests (--replay).
 *
 * A replay script is a text file of lines "<frame> <action> <key>", for
 * example "120 tap jump": on simulation step 120, press and release Space.
 * game_replay_load reads the whole script once, when the game starts, and
 * game_replay_inject_events feeds each step its events, so a run is the
 * same every time.  Scripts live in replays/ (or --replay-dir) and are
 * named by --replay; see game_replay.c for the names it accepts.
 */
#pragma once

#include "../game_fwd.h"  /* GameState, used by pointer only */

/*
 * GameReplayEvent — one line of a script.
 *
 * frame  : the simulation step it happens on (0 = the first).
 * key    : the raylib key code the line's key name stands for.
 * action : "down", "press", "up", "release" or "tap", NUL-terminated.  16
 *          bytes hold the longest (7 letters) with room to spare; the parser
 *          reads at most 15 characters into it (sscanf's "%15s").
 */
typedef struct GameReplayEvent {
    int frame;
    int key;
    char action[16];
} GameReplayEvent;

/*
 * game_replay_load — Parse the script --replay named, once at startup.
 *
 * Does nothing (returns 0) when no replay was asked for.  Otherwise it
 * allocates gs->screen.replay_events with calloc; the GameState owns that
 * array from then on and game_replay_cleanup frees it (game_cleanup calls
 * it).  A requested but invalid replay is a startup error: returns -1,
 * having freed the array already.
 */
int game_replay_load(GameState *gs);

/* Free gs->screen.replay_events and reset the replay cursor.  Safe to call
 * when no replay was loaded, and more than once. */
void game_replay_cleanup(GameState *gs);

/* Inject deterministic commands for the current replay frame. */
void game_replay_inject_events(GameState *gs);
