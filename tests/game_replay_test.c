/*
 * game_replay_test.c — Scripted input replay (input/game_replay.c).
 *
 * `--replay-script <name>` reads <name>.replay from the `--replay-dir`
 * folder (out/replays-smoke by default). These tests point
 * gs->screen.replay_dir, which that flag fills, at TEST_OUT "replays" so their
 * scratch scripts stay inside this build's output tree.
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L /* mkdir under -std=c11 */
#endif

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define test_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define test_mkdir(path) mkdir(path, 0755)
#endif

#include "core/game_inspector.h"
#include "core/game_overlay.h"
#include "core/game_timing.h"
#include "core/game_update.h"
#include "input/game_events.h"
#include "input/game_replay.h"
#include "input/input_backend.h"
#include "player/player.h"
#include "gameplay_mechanics_test.h"
#include "test_paths.h"

#define REPLAY_LEVEL "tests/fixtures/runtime/climbing.toml"
#define REPLAY_DIR TEST_OUT "replays"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "game_replay_test:%d: %s\n", __LINE__, #test); \
    failed = 1; goto done; } } while (0)

static int make_dir(const char *path)
{
    return test_mkdir(path) == 0 || errno == EEXIST ? 0 : -1;
}

static int write_script(const char *name, const char *text)
{
    char path[512];
    if (make_dir(REPLAY_DIR)) return -1;
    snprintf(path, sizeof(path), REPLAY_DIR "/%s.replay", name);
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    int ok = fputs(text, file) >= 0;
    return fclose(file) == 0 && ok ? 0 : -1;
}

static void remove_script(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), REPLAY_DIR "/%s.replay", name);
    remove(path);
}

/* Load script `name` from REPLAY_DIR, as `--replay-dir REPLAY_DIR
 * --replay-script name` would. */
static int load_script(GameState *gs, const char *name)
{
    snprintf(gs->screen.replay_script_path, sizeof(gs->screen.replay_script_path), "%s", name);
    snprintf(gs->screen.replay_dir, sizeof(gs->screen.replay_dir), "%s", REPLAY_DIR);
    return game_replay_load(gs);
}

typedef struct {
    float x[150], vy[150], final_x, final_y, final_vx;
    int on_ground, frames, cursor, held;
} ReplayRun;

/*
 * One game frame without drawing it: the same replay injection, event
 * handling and fixed-step loop game_frame runs, in the same order. Software
 * rendering costs far more than the simulation, so long runs use this and
 * one short run checks it against game_frame itself.
 */
static void replay_frame_unrendered(GameState *gs)
{
    game_replay_inject_events(gs);
    game_handle_events(gs);
    int steps = game_inspector_steps(gs, GAME_FIXED_STEP);
    for (int i = 0; i < steps; i++) {
        game_update_active(gs, GAME_FIXED_STEP, (int)gs->world.camera.x);
        if (game_simulation_blocked(gs)) break;
    }
    game_timing_tick_smoke(gs);
}

/* Play the move-right script for `frames` frames; rendered frames go
 * through game_frame, the rest through replay_frame_unrendered. */
static int play_move_right(ReplayRun *run, int frames, int rendered)
{
    int failed = 0;
    GameState gs;
    memset(run, 0, sizeof(*run));
    CHECK(frames <= (int)(sizeof(run->x) / sizeof(run->x[0])));
    CHECK(mechanics_open_level(&gs, REPLAY_LEVEL, 0) == 0);
    CHECK(load_script(&gs, "move-right") == 0);
    CHECK(gs.screen.replay_event_count == 3);
    for (int frame = 0; frame < frames; frame++) {
        if (frame < rendered) CHECK(mechanics_frames(&gs, 1) == 1);
        else replay_frame_unrendered(&gs);
        run->x[frame] = gs.world.player.x;
        run->vy[frame] = gs.world.player.vy;
    }
    run->final_x = gs.world.player.x;
    run->final_y = gs.world.player.y;
    run->final_vx = gs.world.player.vx;
    run->on_ground = gs.world.player.on_ground;
    run->frames = gs.screen.replay_frame;
    run->cursor = gs.screen.replay_cursor;
    run->held = (int)gs.screen.replay_held_mask;
done:
    game_cleanup(&gs);
    return failed;
}

int replay_scripts_drive_the_game(void)
{
    int failed = 0;
    ReplayRun first, second;
    CHECK(write_script("move-right",
                       "# Hold right, jump once on the way, then let go.\n"
                       "0 down right\n"
                       "  20 tap space\n"
                       "\n"
                       "60 up right\n") == 0);
    CHECK(play_move_right(&first, 150, 0) == 0);
    /* Every event ran, in order, and nothing is left held. */
    CHECK(first.frames == 150 && first.cursor == 3 && first.held == 0);
    /* Held right walks; the tap on frame 20 jumps; release stops. */
    CHECK(first.x[19] > first.x[0] + 10.0f);
    CHECK(first.x[59] > first.x[19] + 30.0f);
    /* A one-frame tap starts a full-speed jump, and letting go on the next
     * frame cuts it short: a tap is a small hop. */
    CHECK(first.vy[19] == 0.0f && first.vy[20] < -300.0f);
    CHECK(first.vy[21] > -150.0f && first.vy[21] < 0.0f);
    CHECK(first.on_ground && first.final_vx == 0.0f);
    CHECK(fabsf(first.final_x - first.x[100]) < 0.001f);

    /* A replay is deterministic: the same script gives the same run,
     * frame by frame, bit for bit, whether or not frames are drawn. */
    CHECK(play_move_right(&second, 150, 12) == 0);
    CHECK(memcmp(first.x, second.x, sizeof(first.x)) == 0);
    CHECK(memcmp(first.vy, second.vy, sizeof(first.vy)) == 0);
    CHECK(first.final_x == second.final_x && first.final_y == second.final_y);

    /* Escape taps pause and resume the game through the normal key path;
     * nothing moves while it is paused. */
    {
        GameState gs;
        CHECK(write_script("pause-resume", "0 down d\n5 tap esc\n30 tap escape\n") == 0);
        CHECK(mechanics_open_level(&gs, REPLAY_LEVEL, 0) == 0);
        int loaded = load_script(&gs, "pause-resume") == 0;
        int paused_ok = 0, frozen_ok = 0, resumed_ok = 0;
        if (loaded) {
            for (int i = 0; i < 6; i++) replay_frame_unrendered(&gs);
            paused_ok = gs.screen.paused == 1;
            float x = gs.world.player.x;
            for (int i = 0; i < 20; i++) replay_frame_unrendered(&gs);
            /* Draw the pause overlay once. */
            frozen_ok = mechanics_frames(&gs, 1) == 1 && gs.world.player.x == x;
            for (int i = 0; i < 10; i++) replay_frame_unrendered(&gs);
            resumed_ok = gs.screen.paused == 0 && gs.world.player.x > x;
        }
        game_cleanup(&gs);
        CHECK(loaded && paused_ok && frozen_ok && resumed_ok);
    }

    /* Key aliases: a/d/w/s, jump, run, return and esc all parse. */
    {
        GameState gs = {0};
        CHECK(write_script("jump-right",
                           "0 press a\n0 release d\n1 down w\n1 up s\n"
                           "2 tap jump\n2 tap run\n3 tap return\n3 tap esc\n") == 0);
        int loaded = load_script(&gs, "jump-right");
        int count = gs.screen.replay_event_count;
        int keys_ok = loaded == 0 && count == 8 &&
                      gs.screen.replay_events[0].key == KEY_LEFT &&
                      gs.screen.replay_events[1].key == KEY_RIGHT &&
                      gs.screen.replay_events[2].key == KEY_UP &&
                      gs.screen.replay_events[3].key == KEY_DOWN &&
                      gs.screen.replay_events[4].key == KEY_SPACE &&
                      gs.screen.replay_events[5].key == KEY_LEFT_SHIFT &&
                      gs.screen.replay_events[6].key == KEY_ENTER &&
                      gs.screen.replay_events[7].key == KEY_ESCAPE &&
                      strcmp(gs.screen.replay_events[1].action, "release") == 0;
        /* Inject without a level: press/release only change held keys;
         * a tap holds its bit for exactly one frame. */
        int held_ok = 0;
        if (keys_ok) {
            input_clear();
            game_replay_inject_events(&gs);   /* frame 0: press a */
            held_ok = gs.screen.replay_input_mask == PLAYER_INPUT_LEFT;
            game_replay_inject_events(&gs);   /* frame 1: hold w */
            held_ok &= gs.screen.replay_input_mask == (PLAYER_INPUT_LEFT | PLAYER_INPUT_UP);
            game_replay_inject_events(&gs);   /* frame 2: taps */
            held_ok &= gs.screen.replay_input_mask ==
                       (PLAYER_INPUT_LEFT | PLAYER_INPUT_UP | PLAYER_INPUT_JUMP | PLAYER_INPUT_RUN);
            game_replay_inject_events(&gs);   /* frame 3: taps end */
            held_ok &= gs.screen.replay_input_mask == (PLAYER_INPUT_LEFT | PLAYER_INPUT_UP);
            held_ok &= gs.screen.replay_cursor == 8 && gs.screen.replay_frame == 4;
            input_clear();
        }
        game_replay_cleanup(&gs);
        CHECK(keys_ok && held_ok);
        CHECK(gs.screen.replay_events == NULL && gs.screen.replay_event_count == 0 && gs.screen.replay_cursor == 0);
        /* With no script, injection only repeats the held keys. */
        gs.screen.replay_held_mask = PLAYER_INPUT_RIGHT;
        game_replay_inject_events(&gs);
        CHECK(gs.screen.replay_input_mask == PLAYER_INPUT_RIGHT);
    }
done:
    remove_script("move-right");
    remove_script("pause-resume");
    remove_script("jump-right");
    return failed;
}

int replay_scripts_reject_malformed_files(void)
{
    int failed = 0;
    static const char *const bad[] = {
        "",                              /* no events */
        "# only a comment\n\n   \n",     /* still no events */
        "5 hop left\n",                  /* unknown action */
        "5 tap banana\n",                /* unknown key */
        "10 tap left\n5 tap left\n",     /* frames out of order */
        "-1 tap left\n",                 /* negative frame */
        "5 tap left now\n",              /* trailing token */
        "five tap left\n",               /* not a number */
        "5tap left\n",                   /* number glued to the action */
        "5 tap\n",                       /* missing key */
        "99999999999999999999 tap left\n" /* frame out of range */
    };
    GameState gs = {0};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        CHECK(write_script("move-right", bad[i]) == 0);
        int result = load_script(&gs, "move-right");
        if (result != -1 || gs.screen.replay_events != NULL || gs.screen.replay_event_count != 0)
            fprintf(stderr, "game_replay_test: accepted bad script %zu\n", i);
        CHECK(result == -1 && gs.screen.replay_events == NULL && gs.screen.replay_event_count == 0);
    }

    /* A line longer than the 160-byte reader is refused, not split. */
    {
        char text[400];
        memset(text, ' ', 300);
        memcpy(text + 290, "1 tap left\n", 12);
        CHECK(write_script("move-right", text) == 0);
        CHECK(load_script(&gs, "move-right") == -1 && gs.screen.replay_events == NULL);
    }

    /* At most 4096 events: one more is refused as oversized. */
    {
        size_t size = 4097 * 12 + 1, used = 0;
        char *text = malloc(size);
        CHECK(text != NULL);
        for (int i = 0; i < 4097; i++)
            used += (size_t)snprintf(text + used, size - used, "%d tap left\n", i / 1000);
        int written = write_script("move-right", text);
        free(text);
        CHECK(written == 0);
        CHECK(load_script(&gs, "move-right") == -1 && gs.screen.replay_events == NULL);
    }

    /* Only the known script names map to files, and they must exist. */
    CHECK(load_script(&gs, "not-a-script") == -1);
    remove_script("jump-right");
    CHECK(load_script(&gs, "jump-right") == -1);
    /* The folder is read as given: a script that exists in REPLAY_DIR is
     * not found through a folder that does not. */
    CHECK(write_script("move-right", "0 tap left\n") == 0);
    CHECK(load_script(&gs, "move-right") == 0);
    game_replay_cleanup(&gs);
    snprintf(gs.screen.replay_dir, sizeof(gs.screen.replay_dir), "%s", TEST_OUT "no-such-replays");
    CHECK(game_replay_load(&gs) == -1 && gs.screen.replay_events == NULL);
    /* No name means no replay, which is not an error. */
    gs.screen.replay_script_path[0] = '\0';
    CHECK(game_replay_load(&gs) == 0 && gs.screen.replay_events == NULL);
done:
    game_replay_cleanup(&gs);
    remove_script("move-right");
    return failed;
}
