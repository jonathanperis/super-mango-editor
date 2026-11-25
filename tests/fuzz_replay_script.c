/*
 * fuzz_replay_script.c — Fuzz the `--replay-script` parser.
 *
 * game_replay_load() (input/game_replay.c) reads a replay script: one event
 * per line, "<frame> <action> <key>", frames in order. A script is a file on
 * disk, written by tools/run_scripted_smoke.py or by hand, so the parser
 * must refuse anything malformed without crashing and without keeping half
 * of it. The input bytes are written as the script file and loaded the way
 * `--replay-dir DIR --replay-script jump-right` would.
 *
 * What must hold for an accepted script:
 *   - 1 to 4096 events, frames never negative and never going backwards,
 *     every key and action one the parser knows;
 *   - written back in canonical form ("frame action key", one per line),
 *     it loads again to the same events: the parser keeps nothing it did
 *     not understand;
 *   - played back, every event fires at its own frame, and the held-input
 *     mask only ever holds the movement bits a key can set.
 * And for a refused one: no events are left behind.
 *
 * See fuzz_replay_main.c for the standalone driver.  POSIX only.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  /* mkdir */
#include <unistd.h>    /* getpid */

#include "game.h"
#include "input/game_replay.h"
#include "input/input_backend.h"
#include "player/player.h"  /* PLAYER_INPUT_* */

#define SCRIPT_NAME "jump-right"   /* one of the names the game accepts */
#define MAX_EVENTS  4096

static char folder[512];
static char script_path[600];

static void remove_files(void)
{
    remove(script_path);
    rmdir(folder);
}

static void init_paths(void)
{
    const char *tmp;
    if (folder[0]) return;
    tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) tmp = "/tmp";
    snprintf(folder, sizeof(folder), "%s/mango-fuzz-replay-%ld", tmp, (long)getpid());
    snprintf(script_path, sizeof(script_path), "%s/" SCRIPT_NAME ".replay", folder);
    mkdir(folder, 0700);
    atexit(remove_files);
}

static void fail(const char *what)
{
    fprintf(stderr, "fuzz_replay_script: %s\n", what);
    abort();
}

static int write_file(const uint8_t *data, size_t size)
{
    FILE *fp = fopen(script_path, "wb");
    if (!fp) return -1;
    size_t put = size ? fwrite(data, 1, size, fp) : 0;
    return fclose(fp) == 0 && put == size ? 0 : -1;
}

static int load(GameState *gs)
{
    memset(&gs->screen, 0, sizeof(gs->screen));
    snprintf(gs->screen.replay_script_path, sizeof(gs->screen.replay_script_path), SCRIPT_NAME);
    snprintf(gs->screen.replay_dir, sizeof(gs->screen.replay_dir), "%s", folder);
    return game_replay_load(gs);
}

/* The parser's own spelling of each key it maps to (left, right, ...). */
static const char *key_name(int key)
{
    switch (key) {
    case KEY_LEFT: return "left";
    case KEY_RIGHT: return "right";
    case KEY_UP: return "up";
    case KEY_DOWN: return "down";
    case KEY_SPACE: return "space";
    case KEY_ENTER: return "enter";
    case KEY_ESCAPE: return "escape";
    case KEY_LEFT_SHIFT: return "shift";
    default: return NULL;
    }
}

static int known_action(const char *action)
{
    static const char *const actions[] = {"down", "press", "up", "release", "tap"};
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++)
        if (strcmp(action, actions[i]) == 0) return 1;
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    /* GameState is large; keep both copies off the stack. */
    static GameState first, second;
    const unsigned int movement = PLAYER_INPUT_LEFT | PLAYER_INPUT_RIGHT | PLAYER_INPUT_UP |
                                  PLAYER_INPUT_DOWN | PLAYER_INPUT_JUMP | PLAYER_INPUT_RUN;

    init_paths();
    if (write_file(data, size) != 0) return 0;
    if (load(&first) != 0) {
        if (first.screen.replay_events || first.screen.replay_event_count)
            fail("a refused script left events behind");
        return 0;
    }

    const int count = first.screen.replay_event_count;
    const GameReplayEvent *events = first.screen.replay_events;
    if (!events || count < 1 || count > MAX_EVENTS) fail("accepted an empty or oversized script");
    for (int i = 0; i < count; i++) {
        if (events[i].frame < 0) fail("negative frame");
        if (i && events[i].frame < events[i - 1].frame) fail("frames go backwards");
        if (!key_name(events[i].key)) fail("unknown key accepted");
        if (memchr(events[i].action, '\0', sizeof(events[i].action)) == NULL ||
            !known_action(events[i].action)) fail("unknown action accepted");
    }

    /* Canonical form back to the file, and load it again. */
    FILE *fp = fopen(script_path, "wb");
    if (!fp) fail("cannot rewrite the script");
    for (int i = 0; i < count; i++)
        fprintf(fp, "%d %s %s\n", events[i].frame, events[i].action, key_name(events[i].key));
    if (fclose(fp) != 0) fail("cannot rewrite the script");
    if (load(&second) != 0) fail("canonical form of an accepted script was refused");
    if (second.screen.replay_event_count != count) fail("canonical form has another length");
    for (int i = 0; i < count; i++) {
        const GameReplayEvent *a = &events[i], *b = &second.screen.replay_events[i];
        if (a->frame != b->frame || a->key != b->key || strcmp(a->action, b->action) != 0)
            fail("canonical form loads to other events");
    }
    game_replay_cleanup(&second);

    /* Play it: jump straight to each event's frame (a script may wait two
     * billion frames) and check that frame's events all fire. */
    while (first.screen.replay_cursor < count) {
        int before = first.screen.replay_cursor;
        first.screen.replay_frame = events[before].frame;
        game_replay_inject_events(&first);
        input_clear();   /* the key events it queued */
        if (first.screen.replay_cursor <= before) fail("an event did not fire at its frame");
        if (first.screen.replay_held_mask & ~movement) fail("held mask outside the movement bits");
        if (first.screen.replay_input_mask & ~movement) fail("input mask outside the movement bits");
    }
    game_replay_cleanup(&first);
    if (first.screen.replay_events || first.screen.replay_event_count)
        fail("cleanup left events behind");
    return 0;
}
