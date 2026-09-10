#include "game_terminal.h"

#include <stddef.h>

void game_terminal_actions(const GameState *gs, GameTerminalActionList *list)
{
    if (!list) return;

    list->count = 0;
    if (!gs) return;

    if (gs->completion.complete) {
        if (gs->completion.pending_next_phase) {
            list->items[list->count++] = GAME_TERMINAL_ACTION_NEXT_LEVEL;
        }
        list->items[list->count++] = GAME_TERMINAL_ACTION_REPLAY;
        list->items[list->count++] = GAME_TERMINAL_ACTION_LEVEL_SELECT;
        list->items[list->count++] = GAME_TERMINAL_ACTION_EXIT;
    } else if (gs->game_over) {
        list->items[list->count++] = GAME_TERMINAL_ACTION_RETRY;
        list->items[list->count++] = GAME_TERMINAL_ACTION_LEVEL_SELECT;
        list->items[list->count++] = GAME_TERMINAL_ACTION_EXIT;
    }
}

static int normalized_index(const GameState *gs, int count)
{
    int index;

    if (!gs || count <= 0) return 0;
    index = gs->terminal_action_index;
    if (index < 0 || index >= count) return 0;
    return index;
}

void game_terminal_move(GameState *gs, int direction)
{
    GameTerminalActionList list;
    int index;

    if (!gs || direction == 0) return;
    game_terminal_actions(gs, &list);
    if (list.count == 0) return;

    index = normalized_index(gs, list.count);
    index += direction > 0 ? 1 : -1;
    if (index < 0) index = list.count - 1;
    if (index >= list.count) index = 0;
    gs->terminal_action_index = index;
}

GameTerminalAction game_terminal_focused_action(const GameState *gs)
{
    GameTerminalActionList list;

    game_terminal_actions(gs, &list);
    if (list.count == 0) return GAME_TERMINAL_ACTION_NONE;
    return list.items[normalized_index(gs, list.count)];
}

const char *game_terminal_action_label(GameTerminalAction action)
{
    switch (action) {
    case GAME_TERMINAL_ACTION_NEXT_LEVEL:    return "Next Level";
    case GAME_TERMINAL_ACTION_REPLAY:        return "Replay";
    case GAME_TERMINAL_ACTION_LEVEL_SELECT:  return "Level Select";
    case GAME_TERMINAL_ACTION_EXIT:          return "Exit";
    case GAME_TERMINAL_ACTION_RETRY:         return "Retry";
    default:                                 return "";
    }
}

GameRoute game_terminal_action_route(GameTerminalAction action)
{
    switch (action) {
    case GAME_TERMINAL_ACTION_NEXT_LEVEL:   return GAME_ROUTE_NEXT_LEVEL;
    case GAME_TERMINAL_ACTION_REPLAY:       return GAME_ROUTE_REPLAY;
    case GAME_TERMINAL_ACTION_LEVEL_SELECT: return GAME_ROUTE_LEVEL_SELECT;
    case GAME_TERMINAL_ACTION_EXIT:         return GAME_ROUTE_EXIT;
    case GAME_TERMINAL_ACTION_RETRY:        return GAME_ROUTE_NONE;
    default:                                return GAME_ROUTE_NONE;
    }
}
