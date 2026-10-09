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
        if (bit) gs->screen.replay_held_mask |= bit;
        push_key(key, INPUT_KEY_DOWN);
    } else if (strcmp(action, "up") == 0 || strcmp(action, "release") == 0) {
        if (bit) {
            gs->screen.replay_held_mask &= ~bit;
            gs->screen.replay_input_mask &= ~bit;
        }
        push_key(key, INPUT_KEY_UP);
    } else if (strcmp(action, "tap") == 0) {
        if (bit) gs->screen.replay_input_mask |= bit;
        push_key(key, INPUT_KEY_DOWN);
        push_key(key, INPUT_KEY_UP);
    }
}

/* Where `make scripted-smoke` writes its scripts when no --replay-dir is
 * given, relative to the working directory (the repository root). */
#define DEFAULT_REPLAY_DIR "out/replays-smoke"

/* Only these script names are accepted, so --replay-script cannot name an
 * arbitrary file; --replay-dir only chooses the folder they are read from. */
static const char *replay_script_file(const char *name)
{
    if (strcmp(name, "move-right") == 0) return "move-right.replay";
    if (strcmp(name, "jump-right") == 0) return "jump-right.replay";
    if (strcmp(name, "pause-resume") == 0) return "pause-resume.replay";
    return NULL;
}

int game_replay_load(GameState *gs)
{
    if (!gs->screen.replay_script_path[0]) return 0;
    const char *file = replay_script_file(gs->screen.replay_script_path);
    if (!file) {
        fprintf(stderr, "Error: unknown replay script '%s'\n", gs->screen.replay_script_path);
        return -1;
    }
    const char *dir = gs->screen.replay_dir[0] ? gs->screen.replay_dir : DEFAULT_REPLAY_DIR;
    /* replay_dir holds at most 255 bytes, so this buffer fits any folder
     * plus "/" and the longest script name; the check keeps that true. */
    char replay_path[sizeof(gs->screen.replay_dir) + 32];
    int written = snprintf(replay_path, sizeof(replay_path), "%s/%s", dir, file);
    if (written < 0 || (size_t)written >= sizeof(replay_path)) {
        fprintf(stderr, "Error: replay script path is too long\n");
        return -1;
    }
    FILE *fp = fopen(replay_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: could not open replay script '%s'\n", replay_path);
        return -1;
    }
    gs->screen.replay_events = calloc(MAX_REPLAY_EVENTS, sizeof(*gs->screen.replay_events));
    if (!gs->screen.replay_events) { fclose(fp); return -1; }
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
            gs->screen.replay_event_count >= MAX_REPLAY_EVENTS) goto invalid;
        int key = replay_keycode(key_name);
        if (key == KEY_NULL ||
            (strcmp(action, "down") && strcmp(action, "press") &&
             strcmp(action, "up") && strcmp(action, "release") && strcmp(action, "tap"))) goto invalid;
        if (gs->screen.replay_event_count && frame < gs->screen.replay_events[gs->screen.replay_event_count - 1].frame)
            goto invalid;
        GameReplayEvent *event = &gs->screen.replay_events[gs->screen.replay_event_count++];
        event->frame = (int)frame;
        event->key = key;
        memcpy(event->action, action, sizeof(event->action));
    }
    if (ferror(fp) || gs->screen.replay_event_count == 0) goto invalid;
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
    free(gs->screen.replay_events);
    gs->screen.replay_events = NULL;
    gs->screen.replay_event_count = gs->screen.replay_cursor = 0;
}

void game_replay_inject_events(GameState *gs)
{
    gs->screen.replay_input_mask = gs->screen.replay_held_mask;
    if (!gs->screen.replay_events) return;
    while (gs->screen.replay_cursor < gs->screen.replay_event_count &&
           gs->screen.replay_events[gs->screen.replay_cursor].frame == gs->screen.replay_frame) {
        const GameReplayEvent *event = &gs->screen.replay_events[gs->screen.replay_cursor++];
        apply_replay_action(gs, event->key, event->action);
    }
    gs->screen.replay_input_mask |= gs->screen.replay_held_mask;
    if (gs->screen.replay_frame < INT_MAX) gs->screen.replay_frame++;
}
