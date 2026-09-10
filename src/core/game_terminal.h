#pragma once

#include "../game.h"

typedef enum {
    GAME_TERMINAL_ACTION_NONE = 0,
    GAME_TERMINAL_ACTION_NEXT_LEVEL,
    GAME_TERMINAL_ACTION_REPLAY,
    GAME_TERMINAL_ACTION_LEVEL_SELECT,
    GAME_TERMINAL_ACTION_EXIT,
    GAME_TERMINAL_ACTION_RETRY
} GameTerminalAction;

#define GAME_TERMINAL_MAX_ACTIONS 4

typedef struct {
    GameTerminalAction items[GAME_TERMINAL_MAX_ACTIONS];
    int count;
} GameTerminalActionList;

/* Build the one valid action list used by both terminal input and rendering. */
void game_terminal_actions(const GameState *gs, GameTerminalActionList *list);

/* Move focused row, wrapping within the valid action list. */
void game_terminal_move(GameState *gs, int direction);

/* Return current focused action, or NONE when no terminal overlay is active. */
GameTerminalAction game_terminal_focused_action(const GameState *gs);

const char *game_terminal_action_label(GameTerminalAction action);

/* Map a confirmed action to an explicit session route. Retry is in-place. */
GameRoute game_terminal_action_route(GameTerminalAction action);
