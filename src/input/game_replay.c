/*
 * game_replay.c — Deterministic replay injection for smoke tests.
 */

#include "game_replay.h"

#include "../player/player.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

static SDL_Keycode replay_keycode(const char *name)
{
    if (strcmp(name, "left") == 0 || strcmp(name, "a") == 0) return SDLK_LEFT;
    if (strcmp(name, "right") == 0 || strcmp(name, "d") == 0) return SDLK_RIGHT;
    if (strcmp(name, "up") == 0 || strcmp(name, "w") == 0) return SDLK_UP;
    if (strcmp(name, "down") == 0 || strcmp(name, "s") == 0) return SDLK_DOWN;
    if (strcmp(name, "space") == 0 || strcmp(name, "jump") == 0) return SDLK_SPACE;
    if (strcmp(name, "enter") == 0 || strcmp(name, "return") == 0) return SDLK_RETURN;
    if (strcmp(name, "escape") == 0 || strcmp(name, "esc") == 0) return SDLK_ESCAPE;
    if (strcmp(name, "shift") == 0 || strcmp(name, "run") == 0) return SDLK_LSHIFT;
    return SDLK_UNKNOWN;
}

static unsigned int replay_input_bit(SDL_Keycode key)
{
    switch (key) {
    case SDLK_LEFT:
        return PLAYER_INPUT_LEFT;
    case SDLK_RIGHT:
        return PLAYER_INPUT_RIGHT;
    case SDLK_UP:
        return PLAYER_INPUT_UP;
    case SDLK_DOWN:
        return PLAYER_INPUT_DOWN;
    case SDLK_SPACE:
        return PLAYER_INPUT_JUMP;
    case SDLK_LSHIFT:
        return PLAYER_INPUT_RUN;
    default:
        return 0;
    }
}

static void push_key(SDL_Keycode key, Uint32 type)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.type = type;
    event.key.state = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = 0;
    event.key.keysym.sym = key;
    event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
    SDL_PushEvent(&event);
}

static void apply_replay_action(GameState *gs, SDL_Keycode key, const char *action)
{
    const unsigned int bit = replay_input_bit(key);

    if (strcmp(action, "down") == 0 || strcmp(action, "press") == 0) {
        if (bit) gs->replay_held_mask |= bit;
        push_key(key, SDL_KEYDOWN);
    } else if (strcmp(action, "up") == 0 || strcmp(action, "release") == 0) {
        if (bit) {
            gs->replay_held_mask &= ~bit;
            gs->replay_input_mask &= ~bit;
        }
        push_key(key, SDL_KEYUP);
    } else if (strcmp(action, "tap") == 0) {
        if (bit) gs->replay_input_mask |= bit;
        push_key(key, SDL_KEYDOWN);
        push_key(key, SDL_KEYUP);
    }
}

static const char *replay_script_path(const char *name)
{
    if (strcmp(name, "move-right") == 0) return "out/replays-smoke/move-right.replay";
    if (strcmp(name, "jump-right") == 0) return "out/replays-smoke/jump-right.replay";
    if (strcmp(name, "pause-resume") == 0) return "out/replays-smoke/pause-resume.replay";
    return NULL;
}

void game_replay_inject_events(GameState *gs)
{
    gs->replay_input_mask = gs->replay_held_mask;
    if (!gs->replay_script_path[0]) return;

    const char *replay_path = replay_script_path(gs->replay_script_path);
    if (!replay_path) {
        fprintf(stderr, "Warning: unknown replay script '%s'\n",
                gs->replay_script_path);
        gs->replay_script_path[0] = '\0';
        gs->replay_input_mask = 0;
        gs->replay_held_mask = 0;
        return;
    }

    FILE *fp = fopen(replay_path, "r");
    if (!fp) {
        fprintf(stderr, "Warning: could not open replay script '%s'\n",
                replay_path);
        gs->replay_script_path[0] = '\0';
        gs->replay_input_mask = 0;
        gs->replay_held_mask = 0;
        return;
    }

    char line[160];
    while (fgets(line, sizeof(line), fp)) {
        int frame = -1;
        char action[16] = {0};
        char key_name[32] = {0};
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "%d %15s %31s", &frame, action, key_name) != 3) continue;
        if (frame != gs->replay_frame) continue;

        SDL_Keycode key = replay_keycode(key_name);
        if (key == SDLK_UNKNOWN) continue;

        apply_replay_action(gs, key, action);
    }

    fclose(fp);
    gs->replay_input_mask |= gs->replay_held_mask;
    gs->replay_frame++;
}
