/*
 * game_replay.c — Deterministic replay injection for smoke tests.
 */

#include "game_replay.h"

#include "../player/player.h"

#include "input_backend.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>

#define MAX_REPLAY_EVENTS 4096

static int replay_keycode(const char *name)
{
    if (strcmp(name, "left") == 0 || strcmp(name, "a") == 0) return KEY_LEFT;
    if (strcmp(name, "right") == 0 || strcmp(name, "d") == 0) return KEY_RIGHT;
    if (strcmp(name, "up") == 0 || strcmp(name, "w") == 0) return KEY_UP;
    if (strcmp(name, "down") == 0 || strcmp(name, "s") == 0) return KEY_DOWN;
    if (strcmp(name, "space") == 0 || strcmp(name, "jump") == 0) return KEY_SPACE;
    if (strcmp(name, "enter") == 0 || strcmp(name, "return") == 0) return KEY_ENTER;
    if (strcmp(name, "escape") == 0 || strcmp(name, "esc") == 0) return KEY_ESCAPE;
    if (strcmp(name, "shift") == 0 || strcmp(name, "run") == 0) return KEY_LEFT_SHIFT;
    return KEY_NULL;
}

static unsigned int replay_input_bit(int key)
{
    switch (key) {
    case KEY_LEFT:
        return PLAYER_INPUT_LEFT;
    case KEY_RIGHT:
        return PLAYER_INPUT_RIGHT;
    case KEY_UP:
        return PLAYER_INPUT_UP;
    case KEY_DOWN:
        return PLAYER_INPUT_DOWN;
    case KEY_SPACE:
        return PLAYER_INPUT_JUMP;
    case KEY_LEFT_SHIFT:
        return PLAYER_INPUT_RUN;
    default:
        return 0;
    }
}

static void push_key(int key, InputKind type)
{
    InputEvent event = {.type=type,.key=key,.binding=input_binding_from_key(key)};
    input_push(&event);
}

static void apply_replay_action(GameState *gs, int key, const char *action)
{
    const unsigned int bit = replay_input_bit(key);

    if (strcmp(action, "down") == 0 || strcmp(action, "press") == 0) {
        if (bit) gs->replay_held_mask |= bit;
        push_key(key, INPUT_KEY_DOWN);
    } else if (strcmp(action, "up") == 0 || strcmp(action, "release") == 0) {
        if (bit) {
            gs->replay_held_mask &= ~bit;
            gs->replay_input_mask &= ~bit;
        }
        push_key(key, INPUT_KEY_UP);
    } else if (strcmp(action, "tap") == 0) {
        if (bit) gs->replay_input_mask |= bit;
        push_key(key, INPUT_KEY_DOWN);
        push_key(key, INPUT_KEY_UP);
    }
}

static const char *replay_script_path(const char *name)
{
    if (strcmp(name, "move-right") == 0) return "out/replays-smoke/move-right.replay";
    if (strcmp(name, "jump-right") == 0) return "out/replays-smoke/jump-right.replay";
    if (strcmp(name, "pause-resume") == 0) return "out/replays-smoke/pause-resume.replay";
    return NULL;
}

int game_replay_load(GameState *gs)
{
    if (!gs->replay_script_path[0]) return 0;
    const char *replay_path = replay_script_path(gs->replay_script_path);
    if (!replay_path) {
        fprintf(stderr, "Error: unknown replay script '%s'\n", gs->replay_script_path);
        return -1;
    }
    FILE *fp = fopen(replay_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: could not open replay script '%s'\n", replay_path);
        return -1;
    }
    gs->replay_events = calloc(MAX_REPLAY_EVENTS, sizeof(*gs->replay_events));
    if (!gs->replay_events) { fclose(fp); return -1; }
    char line[160];
    while (fgets(line, sizeof(line), fp)) {
        char *text = line, *end;
        char action[16] = {0};
        char key_name[32] = {0};
        char extra;
        if (!strchr(line, '\n') && !feof(fp)) goto invalid;
        while (isspace((unsigned char)*text)) text++;
        if (*text == '#' || *text == '\0') continue;
        errno = 0;
        long frame = strtol(text, &end, 10);
        if (errno || end == text || frame < 0 || frame >= INT_MAX ||
            !isspace((unsigned char)*end) ||
            sscanf(end, "%15s %31s %c", action, key_name, &extra) != 2 ||
            gs->replay_event_count >= MAX_REPLAY_EVENTS) goto invalid;
        int key = replay_keycode(key_name);
        if (key == KEY_NULL ||
            (strcmp(action, "down") && strcmp(action, "press") &&
             strcmp(action, "up") && strcmp(action, "release") && strcmp(action, "tap"))) goto invalid;
        if (gs->replay_event_count && frame < gs->replay_events[gs->replay_event_count - 1].frame)
            goto invalid;
        GameReplayEvent *event = &gs->replay_events[gs->replay_event_count++];
        event->frame = (int)frame;
        event->key = key;
        memcpy(event->action, action, sizeof(event->action));
    }
    if (ferror(fp) || gs->replay_event_count == 0) goto invalid;
    fclose(fp);
    return 0;
invalid:
    fprintf(stderr, "Error: malformed, empty, oversized, or unsorted replay '%s'\n", replay_path);
    fclose(fp);
    game_replay_cleanup(gs);
    return -1;
}

void game_replay_cleanup(GameState *gs)
{
    free(gs->replay_events);
    gs->replay_events = NULL;
    gs->replay_event_count = gs->replay_cursor = 0;
}

void game_replay_inject_events(GameState *gs)
{
    gs->replay_input_mask = gs->replay_held_mask;
    if (!gs->replay_events) return;
    while (gs->replay_cursor < gs->replay_event_count &&
           gs->replay_events[gs->replay_cursor].frame == gs->replay_frame) {
        const GameReplayEvent *event = &gs->replay_events[gs->replay_cursor++];
        apply_replay_action(gs, event->key, event->action);
    }
    gs->replay_input_mask |= gs->replay_held_mask;
    if (gs->replay_frame < INT_MAX) gs->replay_frame++;
}
