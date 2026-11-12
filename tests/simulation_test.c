/* Multi-frame behavior proofs using stable fixtures and production update paths. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L  /* mkdir, rmdir under -std=c11 */
#endif
#include "core/game_experiment.h"
#include "core/game_inspector.h"
#include "core/game_overlay.h"
#include "core/game_resources.h"
#include "core/game_random.h"
#include "core/game_movers.h"
#include "core/game_completion.h"
#include "core/game_trig.h"
#include "core/game_timing.h"
#include "core/game_player_step.h"
#include "core/game_float_platforms.h"
#include "collision/game_collision.h"
#include "core/game_update.h"
#include "collision/collision_damage.h"
#include "screens/settings_menu.h"
#include "input/game_events.h"
#include "player/player_internal.h"
#include "player/player_jump.h"
#include "shared/platform.h"   /* clock_millis */
#include "test_paths.h"         /* TEST_OUT scratch directory */
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <errno.h>     /* EEXIST */
#include <sys/stat.h>  /* mkdir */
#include <unistd.h>    /* rmdir */
#endif

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "simulation_test:%d: %s\n", __LINE__, #test); failed = 1; goto done; } } while (0)
#define NEAR(a,b) (fabsf((a)-(b)) < 0.001f)

static void key(GameState *gs, int keycode)
{
    InputEvent event = {.type=INPUT_KEY_DOWN,.key=keycode};
    (void)game_inspector_event(gs, &event);
}

/* The file F9 writes for stamp 1000 on its attempt-th try. */
static void export_name(char *out, size_t size, int attempt)
{
    if (attempt == 1) snprintf(out, size, TEST_OUT "mango-experiment-1000.toml");
    else snprintf(out, size, TEST_OUT "mango-experiment-1000-%d.toml", attempt);
}

static void remove_exports(void)
{
    char name[160];
    for (int i = 1; i <= EXPERIMENT_EXPORT_ATTEMPTS; i++) {
        export_name(name, sizeof(name), i);
        remove(name);
    }
}

static int inspection_and_replay(void)
{
    int failed = 0;
    GameState gs = {0};
    gs.screen.debug_mode = 1;
    gs.screen.random_seed = 7;
    strcpy(gs.world.level_path, "tests/fixtures/runtime/transition.toml");
    CHECK(game_init(&gs) == 0);
    input_clear();
    /* F5 opens and closes the key help; F6 keeps the tuning line visible for
     * a few seconds instead of a permanent panel row. */
    CHECK(!gs.screen.inspector.show_keys);
    key(&gs, KEY_F5);
    CHECK(gs.screen.inspector.show_keys);
    key(&gs, KEY_F5);
    CHECK(!gs.screen.inspector.show_keys);
    {
        /* The deadline is set from the clock at the key press, so it is at
         * least 3 s after a reading taken before the press, however long
         * the test process pauses afterwards. */
        uint64_t before_f6 = clock_millis();
        key(&gs, KEY_F6);
        CHECK(gs.screen.inspector.tuning_visible_until >= before_f6 + 3000);
    }
    gs.screen.inspector.physics_field = 0;
    /* Live play: 40 ms of real time holds two 1/60 s steps (+ the half-step
     * slack a restarted clock starts with), never a 40 ms step. */
    game_timing_restart_clock(&gs);
    CHECK(game_inspector_steps(&gs, 0.04f) == 2);
    key(&gs, KEY_F2);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    key(&gs, KEY_F3);
    CHECK(game_inspector_steps(&gs, 0.04f) == 1);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    key(&gs, KEY_F3);
    game_overlay_set_pause_reason(&gs, GAME_PAUSE_REASON_FOCUS, 1);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0 && !gs.screen.inspector.step_requested);
    game_overlay_set_pause_reason(&gs, GAME_PAUSE_REASON_FOCUS, 0);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    /* Slow mode 0.25x: 40 ms of real time is 10 ms of game time, so steps
     * arrive every other frame instead of becoming shorter. */
    key(&gs, KEY_F2); key(&gs, KEY_F4);
    CHECK(game_inspector_steps(&gs, 0.04f) == 1);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    CHECK(game_inspector_steps(&gs, 0.04f) == 1);
    SettingsMenu settings = {.open = 1};
    gs.screen.settings_menu = &settings;
    key(&gs, KEY_F3);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    /* While a binding is being captured, the inspector's keys go to the
     * settings panel, which refuses them as bindings (in every run). */
    static GameProfile profile;
    game_profile_init(&profile);
    gs.screen.profile = &profile;
    settings.page = 1;
    settings.capture = 1;
    InputEvent reserved = {.type=INPUT_KEY_DOWN,.key=KEY_F3,.binding=input_binding_from_key(KEY_F3)};
    input_push(&reserved); game_handle_events(&gs);
    CHECK(settings.capture == 1 && strstr(settings.message, "debug inspector") != NULL);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    gs.screen.settings_menu = NULL;
    gs.screen.profile = NULL;
    float speed = gs.world.player.walk_max_speed;
    key(&gs, KEY_EQUAL);
    CHECK(gs.world.player.walk_max_speed == speed + 25);
    key(&gs, KEY_F7);
    CHECK(gs.world.player.walk_max_speed == speed);
    CHECK(game_experiment_begin(&gs) == 0);
    for (int i = 0; i < 180; i++) {
        gs.screen.replay_input_mask = i < 120 ? PLAYER_INPUT_RIGHT : 0;
        if (i == 30) gs.screen.replay_input_mask |= PLAYER_INPUT_JUMP;
        if (i == 60) gs.world.player.walk_max_speed = 150;
        game_update_active(&gs, GAME_FIXED_STEP, (int)gs.world.camera.x);
    }
    CHECK(gs.screen.experiment->count == 180 && gs.world.player.x > 150);
    Player recorded = gs.world.player;
    uint32_t elapsed = gs.screen.completion.level_steps;
    int score = gs.world.score, checkpoint = gs.world.checkpoint_index;
    remove(TEST_OUT "school-experiment.toml");
    CHECK(game_experiment_save(&gs, TEST_OUT "school-experiment.toml") == 0);
    CHECK(game_experiment_save(&gs, TEST_OUT "school-experiment.toml") == -1);
    /* F9 export names: an export used to reuse a time-since-boot name after
     * a reboot, collide with an old file, and report "F8 starts a recording"
     * although a recording existed. A taken name now moves on to -2, -3...,
     * and running out of names has its own result. */
    {
        char exported[160], expected[160];
        remove_exports();
        CHECK(game_experiment_export_at(&gs, TEST_OUT, 1000, exported, sizeof(exported)) == EXPERIMENT_EXPORT_OK);
        export_name(expected, sizeof(expected), 1);
        CHECK(strcmp(exported, expected) == 0);
        CHECK(game_experiment_export_at(&gs, TEST_OUT, 1000, exported, sizeof(exported)) == EXPERIMENT_EXPORT_OK);
        export_name(expected, sizeof(expected), 2);
        CHECK(strcmp(exported, expected) == 0);
        for (int i = 3; i <= EXPERIMENT_EXPORT_ATTEMPTS; i++) {
            export_name(expected, sizeof(expected), i);
            FILE *taken = fopen(expected, "w");
            CHECK(taken != NULL);
            fclose(taken);
        }
        CHECK(game_experiment_export_at(&gs, TEST_OUT, 1000, exported, sizeof(exported)) == EXPERIMENT_EXPORT_NAME_TAKEN);
        GameExperiment *tape = gs.screen.experiment;
        gs.screen.experiment = NULL;
        CHECK(game_experiment_export_at(&gs, TEST_OUT, 1001, exported, sizeof(exported)) == EXPERIMENT_EXPORT_NOTHING);
        gs.screen.experiment = tape;
        /* A folder holding a control character (from a crafted HOME or
         * XDG_DATA_HOME) is refused before anything is written, even when
         * that folder exists and could take the file. */
        CHECK(path_copy_checked(expected, TEST_OUT "bad\tfolder/", sizeof(expected)) == -1 &&
              expected[0] == '\0');
        CHECK(path_copy_checked(expected, TEST_OUT, sizeof(expected)) == 0 &&
              strcmp(expected, TEST_OUT) == 0);
#ifndef _WIN32
        CHECK(mkdir(TEST_OUT "bad\tfolder", 0700) == 0 || errno == EEXIST);
#endif
        CHECK(game_experiment_export_at(&gs, TEST_OUT "bad\tfolder/", 1002, exported,
                                        sizeof(exported)) == EXPERIMENT_EXPORT_WRITE_FAILED);
#ifndef _WIN32
        CHECK(rmdir(TEST_OUT "bad\tfolder") == 0);
#endif
        remove_exports();
    }
    CHECK(game_experiment_load(&gs, TEST_OUT "school-experiment.toml") == 0);
    /* Replay through the same frame loop shape as game_frame: real frames of
     * 70 ms run several fixed steps each, one recorded row per step. */
    int replayed = 0;
    for (int frame = 0; frame < 400 && replayed < 180; frame++) {
        /* Opposite live input must not perturb replay. */
        gs.screen.replay_input_mask = PLAYER_INPUT_LEFT;
        int steps = game_inspector_steps(&gs, 0.07f);
        for (int s = 0; s < steps; s++) {
            float dt = game_experiment_dt(&gs, GAME_FIXED_STEP);
            if (dt <= 0) break;
            game_update_active(&gs, dt, (int)gs.world.camera.x);
            replayed++;
        }
    }
    CHECK(replayed == 180);
    CHECK(NEAR(gs.world.player.x, recorded.x) && NEAR(gs.world.player.y, recorded.y));
    CHECK(NEAR(gs.world.player.vx, recorded.vx) && NEAR(gs.world.player.vy, recorded.vy));
    CHECK(gs.screen.completion.level_steps == elapsed && gs.world.score == score && gs.world.checkpoint_index == checkpoint);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    FILE *bad = fopen(TEST_OUT "school-experiment-invalid.toml", "w");
    CHECK(bad != NULL);
    fputs("format_version = 9\n", bad); fclose(bad);
    GameExperiment *before = gs.screen.experiment;
    CHECK(game_experiment_load(&gs, TEST_OUT "school-experiment-invalid.toml") == -1 && gs.screen.experiment == before);
    /* A capture that is not there, and one over the size limit, are refused
     * before any of it is parsed; the running game keeps what it had. */
    CHECK(game_experiment_load(&gs, TEST_OUT "school-experiment-missing.toml") == -1 && gs.screen.experiment == before);
    bad = fopen(TEST_OUT "school-experiment-huge.toml", "wb");
    CHECK(bad != NULL);
    {
        static char filler[4096];
        memset(filler, '#', sizeof(filler) - 1);
        filler[sizeof(filler) - 1] = '\n';
        for (size_t written = 0; written < (size_t)EXPERIMENT_TEXT_MAX; written += sizeof(filler))
            CHECK(fwrite(filler, 1, sizeof(filler), bad) == sizeof(filler));
    }
    CHECK(fclose(bad) == 0);
    CHECK(game_experiment_load(&gs, TEST_OUT "school-experiment-huge.toml") == -1 && gs.screen.experiment == before);
    remove(TEST_OUT "school-experiment-huge.toml");
    /* Rows are [input, 9 physics values]. Format 1 rows (with a leading
     * frame duration) came from the variable-timestep engine: refused.
     * Format 2 rows are well formed, but were recorded while the camera
     * still panned in from x = 0, which changes when waiting spike blocks
     * start: refused too, rather than replayed into another run. Format 3
     * rows came from before spike blocks and bouncepads stopped depending
     * on the Reduced motion setting and on jump input: refused as well. */
    const char *bad_rows[] = {
        "[0, nan, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[64, 100, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[0, 1e100, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[0.016, 0, 100, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[0, 100, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[0, 100, 250, 750, 600, 550, 100, 350, 180, 80]"
    };
    const int bad_versions[] = {EXPERIMENT_FORMAT_VERSION, EXPERIMENT_FORMAT_VERSION,
                                EXPERIMENT_FORMAT_VERSION, 1, 2, 3};
    for (size_t i = 0; i < sizeof(bad_rows) / sizeof(bad_rows[0]); i++) {
        bad = fopen(TEST_OUT "school-experiment-invalid.toml", "w");
        CHECK(bad != NULL);
        fprintf(bad, "format_version = %d\nlevel_path = \"fixture\"\nseed = 7\nlevel_hash = \"%016llx\"\nframes = [%s]\n",
                bad_versions[i], (unsigned long long)gs.world.source_level_hash, bad_rows[i]);
        fclose(bad);
        CHECK(game_experiment_load(&gs, TEST_OUT "school-experiment-invalid.toml") == -1 && gs.screen.experiment == before);
    }
    gs.world.source_level_hash ^= 1; /* Loaded bytes no longer match the file. */
    CHECK(game_experiment_begin(&gs) == -1 && gs.screen.experiment == before);
    gs.world.source_level_hash ^= 1;
done:
    remove(TEST_OUT "school-experiment.toml"); remove(TEST_OUT "school-experiment-invalid.toml");
    remove_exports();
    game_cleanup(&gs);
    return failed;
}

static int moving_support_and_damage(void)
{
    int failed = 0;
    GameState gs = {0};
    strcpy(gs.world.level_path, "tests/fixtures/runtime/moving_support.toml");
    CHECK(game_init(&gs) == 0);
    gs.world.player.x = gs.world.float_platforms[0].x;
    gs.world.player.y = gs.world.float_platforms[0].y - gs.world.player.h + PLAYER_FLOOR_SINK;
    gs.world.player.on_ground = 1;
    gs.screen.loop.fp_prev_riding = 0;
    float offset = gs.world.player.x - gs.world.float_platforms[0].x;
    for (int i = 0; i < 180; i++) game_update_active(&gs, 1.0f / 60, (int)gs.world.camera.x);
    CHECK(gs.screen.loop.fp_prev_riding == 0 && NEAR(gs.world.player.x - gs.world.float_platforms[0].x, offset));

    /* A saw starts one pixel outside the player and enters during this step. */
    gs.world.player.x = 48; gs.world.player.y = FLOOR_Y - gs.world.player.h + PLAYER_FLOOR_SINK;
    gs.world.player.vx = gs.world.player.vy = gs.world.player.hurt_timer = 0;
    IntRect hit = player_get_hitbox(&gs.world.player);
    gs.world.circular_saw_count = 1;
    gs.world.circular_saws[0] = (CircularSaw){.x = hit.x + hit.w - 3, .y = 220,
        .w = 32, .h = 32, .active = 1, .direction = -1, .patrol_x0 = 0, .patrol_x1 = 300};
    int hearts = gs.world.hearts;
    game_update_active(&gs, 1.0f / 60, 0);
    CHECK(gs.world.hearts == hearts - 1);
    LevelDef *def = gs.world.level_def;
    def->circular_saw_count = 1;
    Texture2D *texture = gs.assets.textures.circular_saw;
    gs.assets.textures.circular_saw = NULL;
    int missing = game_resources_require_level_textures(&gs, def);
    gs.assets.textures.circular_saw = texture;
    CHECK(missing == -1);

    /* Authored checkpoints persist through an actual lethal-damage reset. */
    def->checkpoint_count = 1; def->checkpoints[0] = (CheckpointPlacement){304, 252};
    gs.world.player.x = 320;
    gs.world.player.hurt_timer = 0;
    game_update_active(&gs, 1.0f / 60, 0);
    CHECK(gs.world.checkpoint_index == 0);
    gs.world.hearts = 1; gs.world.player.hurt_timer = 0;
    apply_damage(&gs, 1, 0, gs.world.player.x, gs.world.player.y);
    CHECK(gs.world.checkpoint_index == 0 && NEAR(gs.world.player.x, 304));
done:
    game_cleanup(&gs);
    return failed;
}

/*
 * Float platform 1 of moving_support.toml rides a tall RECT rail. Going down
 * its right side, the rider must stay on it every step (it used to fall a
 * little, land, fall again: FALL/IDLE flicker). Going up its left side at the
 * top rail speed, a player falling onto it must land, not drop through: the
 * platform rises past their feet during its own update, after the player
 * already moved that step.
 */
static int rect_rail_platform_carries_rider_down_and_catches_from_above(void)
{
    int failed = 0;
    GameState gs = {0};
    strcpy(gs.world.level_path, "tests/fixtures/runtime/moving_support.toml");
    CHECK(game_init(&gs) == 0);
    CHECK(gs.world.float_platform_count == 2 && gs.world.rail_count == 2);
    FloatPlatform *fp = &gs.world.float_platforms[1];
    Player *p = &gs.world.player;

    /* Down: t = 3 is the top-right corner; the next 9 tiles go straight down. */
    p->x = fp->x + fp->w / 2.0f - p->w / 2.0f;
    p->y = fp->y - p->h + PLAYER_FLOOR_SINK;
    p->vx = p->vy = 0.0f;
    p->on_ground = 1;
    gs.screen.loop.fp_prev_riding = 1;
    float start_y = fp->y;
    for (int step = 0; step < 150; step++) {
        game_update_active(&gs, GAME_FIXED_STEP, (int)gs.world.camera.x);
        CHECK(gs.screen.loop.fp_prev_riding == 1 && p->on_ground);
        CHECK(fabsf(p->y + p->h - PLAYER_FLOOR_SINK - fp->y) < 0.01f);
    }
    CHECK(fp->y > start_y + 100.0f);   /* it really went down: 3 tiles/s × 2.5 s */

    /* Up: t = 16 is the bottom of the left side; at 30 tiles/s it rises
     * 8 px per step. The player hovers 1 px above it, falling from rest. */
    float_platform_init(fp, FLOAT_PLATFORM_RAIL, 0.0f, 0.0f, 4, 0.0f,
                        &gs.world.rails[1], 16.0f, (float)MAX_RAIL_SPEED);
    p->x = fp->x + fp->w / 2.0f - p->w / 2.0f;
    p->y = fp->y - 1.0f - p->h + PLAYER_FLOOR_SINK;
    p->vx = p->vy = 0.0f;
    p->on_ground = 0;
    gs.screen.loop.fp_prev_riding = -1;
    start_y = fp->y;
    game_update_active(&gs, GAME_FIXED_STEP, (int)gs.world.camera.x);
    CHECK(!p->on_ground);              /* still 0.8 px above the old top */
    game_update_active(&gs, GAME_FIXED_STEP, (int)gs.world.camera.x);
    CHECK(p->on_ground && gs.screen.loop.fp_prev_riding == 1);
    for (int step = 0; step < 10; step++) {
        game_update_active(&gs, GAME_FIXED_STEP, (int)gs.world.camera.x);
        CHECK(gs.screen.loop.fp_prev_riding == 1 && p->on_ground);
        CHECK(fabsf(p->y + p->h - PLAYER_FLOOR_SINK - fp->y) < 0.01f);
    }
    CHECK(fp->y < start_y - 80.0f);    /* it really went up: 8 px per step */
done:
    game_cleanup(&gs);
    return failed;
}

/* A bare GameState with one player standing on the ground floor. */
static void stand_player_on_floor(GameState *gs, float x)
{
    gs->world.runtime.world_w = 1600;
    gs->world.player.w = gs->world.player.h = 48;
    player_apply_default_physics(&gs->world.player);
    gs->world.player.spawn_x = x;
    gs->world.player.spawn_y = FLOOR_Y;
    player_reset(&gs->world.player);
    gs->screen.loop.fp_prev_riding = -1;
}

/*
 * Jump once while frames arrive at render_hz, through the same accumulator
 * game_frame uses. Jump is held for the first 30 steps (a full jump); the
 * input is indexed by step, not by frame, as it is for a recorded run.
 * Returns how high the player rose above the floor, in pixels.
 */
static float jump_apex_height(float render_hz, float jitter)
{
    GameState gs = {0};
    stand_player_on_floor(&gs, 100.0f);
    float floor_y = gs.world.player.y, apex_y = gs.world.player.y;
    int step_index = 0;
    game_timing_restart_clock(&gs);
    for (int frame = 0; frame < (int)(render_hz * 2.0f); frame++) {
        float seconds = 1.0f / render_hz + (frame % 2 ? jitter : -jitter);
        int steps = game_timing_take_steps(&gs, seconds);
        for (int s = 0; s < steps; s++, step_index++) {
            gs.screen.replay_input_mask = step_index < 30 ? PLAYER_INPUT_JUMP : 0;
            game_player_step(&gs, GAME_FIXED_STEP);
            if (gs.world.player.y < apex_y) apex_y = gs.world.player.y;
        }
    }
    return floor_y - apex_y;
}

static int jump_height_ignores_frame_rate(void)
{
    int failed = 0;
    float at60 = jump_apex_height(60.0f, 0.0f);
    /* JUMP_VY 325 px/s against GRAVITY 800 px/s² peaks near v²/2g = 66 px;
     * semi-implicit Euler at 1/60 s lands a little below that. */
    CHECK(at60 > 60.0f && at60 < 67.0f);
    /* Exactly equal, not "close": every display rate runs the same steps. */
    CHECK(jump_apex_height(30.0f, 0.0f) == at60);
    CHECK(jump_apex_height(144.0f, 0.0f) == at60);
    CHECK(jump_apex_height(60.0f, 0.0006f) == at60);
done:
    return failed;
}

static int fast_fall_does_not_tunnel_through_platform(void)
{
    int failed = 0;
    /* A one-way pillar top at y=150 and a player falling at 3000 px/s:
     * 50 px per step, far more than any surface is thick. The crossing test
     * (bottom before vs. after the step) must still land the player. */
    Platform pillar = {.x = 0, .y = 150, .w = 200, .h = 102};
    Player player = {.x = 50, .y = 0, .w = 48, .h = 48, .vy = 3000};
    player_apply_default_physics(&player);
    const PlayerSurfaces surfaces = { .platforms = &pillar, .platform_count = 1, .world_w = 400 };
    PlayerLanding landed;
    for (int step = 0; step < 60 && !player.on_ground; step++)
        player_update(&player, GAME_FIXED_STEP, NULL, &surfaces, -1, &landed);
    CHECK(player.on_ground);
    CHECK(NEAR(player.y + player.h - PLAYER_FLOOR_SINK, pillar.y));
    CHECK(player.vy == 0.0f);
done:
    return failed;
}

/*
 * A player already 10 px down inside a floor gap, running right, must keep
 * falling: the gap's sides hold them in. The floor snap used to lift anyone
 * whose centre reached grass again, even from inside the hole.
 */
static int sinking_into_a_gap_cannot_steer_back_onto_the_floor(void)
{
    int failed = 0;
    int gaps[] = {160};
    Player player = {.w = 48, .h = 48};
    player_apply_default_physics(&player);
    player.x = 160.0f + FLOOR_GAP_W / 2.0f - player.w / 2.0f;
    player.y = (float)FLOOR_Y + 10.0f - player.h + PLAYER_FLOOR_SINK;
    player.vx = 100.0f;
    player.move_dir = 1;
    const PlayerSurfaces surfaces = { .floor_gaps = gaps, .floor_gap_count = 1, .world_w = 1600 };
    PlayerLanding landed;
    for (int step = 0; step < 30; step++) {
        player_update(&player, GAME_FIXED_STEP, NULL, &surfaces, -1, &landed);
        float centre = player.x + player.w / 2.0f;
        CHECK(!player.on_ground);
        CHECK(player.y + player.h - PLAYER_FLOOR_SINK > (float)FLOOR_Y + 10.0f);
        CHECK(centre >= 160.0f && centre < 160.0f + FLOOR_GAP_W);
    }

    /* Running across the gap at floor height still reaches the far side. */
    player = (Player){.w = 48, .h = 48};
    player_apply_default_physics(&player);
    player.x = 160.0f + FLOOR_GAP_W - 1.0f - player.w / 2.0f;
    player.y = (float)FLOOR_Y - player.h + PLAYER_FLOOR_SINK;
    player.vx = 100.0f;
    player.move_dir = 1;
    player_update(&player, GAME_FIXED_STEP, NULL, &surfaces, -1, &landed);
    CHECK(player.on_ground);
    CHECK(NEAR(player.y + player.h - PLAYER_FLOOR_SINK, (float)FLOOR_Y));
done:
    return failed;
}

static int collision_hurts_once_and_collects_coins(void)
{
    int failed = 0;
    GameState gs = {0};
    stand_player_on_floor(&gs, 100.0f);
    gs.world.hearts = 3;
    gs.world.lives = 3;
    gs.world.rules.coin_score = 100;
    gs.world.rules.score_per_life = 1000;
    gs.world.score_life_next = 1000;
    gs.world.coin_count = 1;
    gs.world.coins[0] = (Coin){.x = 300, .y = 230, .active = 1};
    gs.world.spike_row_count = 1;
    gs.world.spike_rows[0] = (SpikeRow){.x = 110, .y = FLOOR_Y - SPIKE_TILE_H, .count = 2, .active = 1};

    /* Standing in the spikes: one heart lost, knockback, invincibility. */
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.hearts == 2 && gs.world.player.hurt_timer > 0.0f && gs.world.player.vx != 0.0f);
    CHECK(gs.world.coins[0].active == 1 && gs.world.score == 0);
    /* Still overlapping during invincibility: no second hit. */
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.hearts == 2);

    /* Overlapping the coin collects it exactly once. */
    gs.world.player.x = 290.0f;
    game_collide(&gs, GAME_FIXED_STEP);
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.coins[0].active == 0 && gs.world.score == 100 && gs.world.hearts == 2);
    CHECK(gs.screen.coin_streak.count == 1);
done:
    return failed;
}

/* Quick pickups raise the coin chime a semitone each, up to a cap, and a
 * pause starts again at the recorded pitch. Same steps, same pitches. */
static int coin_streak_raises_the_chime(void)
{
    int failed = 0;
    CoinStreak streak = {0};
    CHECK(coin_streak_pitch(&streak, 100) == 1.0f);
    float second = coin_streak_pitch(&streak, 100 + COIN_STREAK_GAP_STEPS);
    CHECK(second > 1.05f && second < 1.06f && streak.count == 2);
    float pitch = second;
    for (int i = 0; i < 20; i++) {
        float next = coin_streak_pitch(&streak, 146 + (uint32_t)i);
        CHECK(next >= pitch);
        pitch = next;
    }
    CHECK(pitch > 1.58f && pitch < 1.59f);  /* capped eight semitones up */
    CHECK(coin_streak_pitch(&streak, 165 + COIN_STREAK_GAP_STEPS + 1) == 1.0f);
    /* A restarted step clock (a new attempt) never continues a streak. */
    coin_streak_pitch(&streak, 400);
    CHECK(coin_streak_pitch(&streak, 3) == 1.0f && streak.count == 1);

    /* In play: two coins on consecutive steps make a streak of two, and the
     * world comes out exactly as without one. */
    GameState gs = {0}, plain = {0};
    for (int run = 0; run < 2; run++) {
        GameState *g = run ? &plain : &gs;
        stand_player_on_floor(g, 290.0f);
        g->world.hearts = g->world.lives = 3;
        g->world.rules.coin_score = 100;
        g->world.score_life_next = 1000;
        g->world.coin_count = 2;
        g->world.coins[0] = (Coin){.x = 300, .y = 230, .active = 1};
        g->world.coins[1] = (Coin){.x = 330, .y = 230, .active = 1};
    }
    plain.screen.coin_streak.count = 5;  /* a streak already going */
    for (int run = 0; run < 2; run++) {
        GameState *g = run ? &plain : &gs;
        g->world.sim_steps = 10;
        game_collide(g, GAME_FIXED_STEP);
        g->world.player.x = 320.0f;
        g->world.sim_steps = 11;
        game_collide(g, GAME_FIXED_STEP);
    }
    CHECK(gs.screen.coin_streak.count == 2 && gs.world.score == 200);
    CHECK(memcmp(&gs.world, &plain.world, sizeof(gs.world)) == 0);
done:
    return failed;
}

/*
 * health_star.c: every colour heals one heart on touch, never above
 * MAX_HEARTS, and a player who is already full leaves the star in place.
 */
static int health_stars_heal_one_heart_up_to_the_cap(void)
{
    int failed = 0;
    GameState gs = {0};
    stand_player_on_floor(&gs, 100.0f);
    gs.world.lives = 3;
    IntRect phit = player_get_hitbox(&gs.world.player);
    HealthStar on_player = {.x = (float)phit.x, .y = (float)phit.y, .active = 1};

    /* The hitbox is the whole 16x16 sprite at the star's position. */
    IntRect star_hit = health_star_get_hitbox(&on_player);
    CHECK(star_hit.x == phit.x && star_hit.y == phit.y);
    CHECK(star_hit.w == HEALTH_STAR_DISPLAY_W && star_hit.h == HEALTH_STAR_DISPLAY_H);

    /* One heart left: a yellow star heals one, and is gone. */
    gs.world.hearts = 1;
    gs.world.star_yellow_count = 1;
    gs.world.star_yellows[0] = on_player;
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.hearts == 2 && !gs.world.star_yellows[0].active && gs.world.score == 0);
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.hearts == 2);

    /* Green and red together, one heart short: the green one (tested
     * first) fills the gap, and the red one stays for later because a
     * full player has nothing to heal. */
    gs.world.hearts = MAX_HEARTS - 1;
    gs.world.star_green_count = 1;
    gs.world.star_greens[0] = on_player;
    gs.world.star_red_count = 1;
    gs.world.star_reds[0] = on_player;
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.hearts == MAX_HEARTS);
    CHECK(!gs.world.star_greens[0].active && gs.world.star_reds[0].active);
    /* Still there on later steps, and taken once a heart is missing. */
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.star_reds[0].active);
    gs.world.hearts = MAX_HEARTS - 1;
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.hearts == MAX_HEARTS && !gs.world.star_reds[0].active);

    /* A star the player does not touch stays. */
    gs.world.star_yellows[0] = (HealthStar){.x = phit.x + 200.0f, .y = (float)phit.y, .active = 1};
    gs.world.hearts = 1;
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.world.hearts == 1 && gs.world.star_yellows[0].active);
done:
    return failed;
}

/*
 * player_jump.c: coyote time, the jump buffer and the short-hop cut, each
 * through the functions player_handle_input and player_update call.
 */
static int jump_buffer_coyote_time_and_short_hops(void)
{
    int failed = 0;
    Player p = {.w = 48, .h = 48};
    player_apply_default_physics(&p);

    /* Coyote time: just off a ledge (airborne, not rising), a press still
     * jumps, because the timers armed the grace window. */
    p.on_ground = 0;
    p.vy = 0.0f;
    CHECK(player_update_jump_timers(&p, GAME_FIXED_STEP, 1) == 0);
    CHECK(NEAR(p.coyote_timer, PLAYER_COYOTE_TIME));
    player_press_jump(&p, NULL);
    CHECK(p.vy == JUMP_VY && p.coyote_timer == 0.0f && p.jump_held);

    /* After the grace window runs out in the air, a press only buffers. */
    p = (Player){.w = 48, .h = 48};
    player_apply_default_physics(&p);
    p.coyote_timer = PLAYER_COYOTE_TIME;
    p.vy = 50.0f;
    for (int step = 0; step < 7; step++)       /* 7/60 s > 0.10 s */
        player_update_jump_timers(&p, GAME_FIXED_STEP, 0);
    CHECK(p.coyote_timer == 0.0f);
    player_press_jump(&p, NULL);
    CHECK(p.vy == 50.0f && p.jump_buffer_timer > 0.0f);

    /* Jump buffer: landing within 0.10 s of that press jumps on contact... */
    for (int step = 0; step < 4; step++)
        CHECK(player_update_jump_timers(&p, GAME_FIXED_STEP, 0) == 0);
    p.on_ground = 1;
    CHECK(player_update_jump_timers(&p, GAME_FIXED_STEP, 0) == 1);

    /* ...but a press more than 0.10 s before landing has expired. */
    p.on_ground = 0;
    p.jump_buffer_timer = 0.0f;
    player_press_jump(&p, NULL);
    p.coyote_timer = 0.0f;
    for (int step = 0; step < 7; step++)
        player_update_jump_timers(&p, GAME_FIXED_STEP, 0);
    CHECK(p.jump_buffer_timer == 0.0f);
    p.on_ground = 1;
    CHECK(player_update_jump_timers(&p, GAME_FIXED_STEP, 0) == 0);

    /* Short hop: letting go while rising cuts the upward speed to 45%;
     * letting go while falling changes nothing. */
    p.vy = -200.0f;
    p.jump_held = 1;
    p.jump_cut_armed = 1;
    player_release_jump(&p);
    CHECK(NEAR(p.vy, -90.0f) && !p.jump_held && !p.jump_cut_armed);
    p.vy = 120.0f;
    p.jump_held = 1;
    p.jump_cut_armed = 1;
    player_release_jump(&p);
    CHECK(p.vy == 120.0f);

    /* Only a jump arms the cut. A press in mid-air sets jump_held (so the
     * key has to be let go before it counts again) but shortens nothing. */
    p.vy = -200.0f;
    p.on_ground = 0;
    p.coyote_timer = 0.0f;
    p.jump_held = 0;
    p.jump_cut_armed = 0;
    player_press_jump(&p, NULL);
    CHECK(p.jump_held && !p.jump_cut_armed);
    player_release_jump(&p);
    CHECK(p.vy == -200.0f);

    /* End to end: a tap rises much less than a held jump. */
    {
        GameState gs = {0};
        float floor_y, apex_y;
        stand_player_on_floor(&gs, 100.0f);
        floor_y = apex_y = gs.world.player.y;
        for (int step = 0; step < 60; step++) {
            gs.screen.replay_input_mask = step < 2 ? PLAYER_INPUT_JUMP : 0;
            game_player_step(&gs, GAME_FIXED_STEP);
            if (gs.world.player.y < apex_y) apex_y = gs.world.player.y;
        }
        float tap = floor_y - apex_y;
        CHECK(tap > 5.0f && tap < 30.0f);   /* a full jump rises 60+ px */
    }
done:
    return failed;
}

/*
 * The debug overlay outlines a coin with coin_get_hitbox and a bouncepad
 * with bouncepad_trigger_rect. Those must be the game's own tests: for
 * every column a player can stand in around them, "the player's hitbox
 * meets the outline" must say exactly what the game does. The overlay
 * used to draw a pad as the full height of its sprite, which a player in
 * mid-air touches without being launched.
 */
static int debug_coin_and_bouncepad_outlines_match_the_game(void)
{
    int failed = 0;

    /* A coin is collected exactly when the hitbox meets its outline. */
    for (int dx = -40; dx <= 40; dx++) {
        GameState gs = {0};
        stand_player_on_floor(&gs, 0.0f);
        gs.world.hearts = gs.world.lives = 3;
        gs.world.score_life_next = 1000;
        gs.world.coin_count = 1;
        gs.world.coins[0] = (Coin){.x = 300, .y = 230, .active = 1};
        gs.world.player.x = 300.0f + (float)dx;
        IntRect body = player_get_hitbox(&gs.world.player);
        IntRect coin = coin_get_hitbox(&gs.world.coins[0]);
        int touches = rect_intersects(&body, &coin);
        game_collide(&gs, GAME_FIXED_STEP);
        CHECK(touches == !gs.world.coins[0].active);
    }

    /* A pad launches a player landing on the floor exactly when their
     * hitbox, standing there, meets the outlined strip. */
    int launched_any = 0, missed_any = 0;
    for (int dx = -40; dx <= 40; dx++) {
        GameState gs = {0};
        stand_player_on_floor(&gs, 0.0f);
        gs.world.bouncepad_medium_count = 1;
        bouncepad_place(&gs.world.bouncepads_medium[0], 200.0f, BOUNCEPAD_VY_MEDIUM,
                        BOUNCEPAD_WOOD);
        Player *p = &gs.world.player;
        p->x = 200.0f + (float)dx;
        IntRect standing = player_get_hitbox(p);
        IntRect strip = bouncepad_trigger_rect(&gs.world.bouncepads_medium[0]);
        int touches = rect_intersects(&standing, &strip);
        /* Just above the floor and falling onto it. Up there the player
         * is not launched yet, so the outline must not reach that high:
         * it is the floor's top row, not the pad's sprite. */
        p->y -= 2.0f;
        IntRect above = player_get_hitbox(p);
        CHECK(!rect_intersects(&above, &strip));
        p->vy = 120.0f;
        p->on_ground = 0;
        game_player_step(&gs, GAME_FIXED_STEP);
        int launched = p->vy < 0.0f;
        CHECK(touches == launched);
        if (launched) launched_any = 1; else missed_any = 1;
    }
    CHECK(launched_any && missed_any);   /* the range covered both cases */
done:
    return failed;
}

/*
 * game_sinf and game_cosf (core/game_trig.h) agree with the C library to
 * within one float step everywhere the game uses them, and give exactly
 * the values below on every machine. The values were printed on macOS
 * arm64; if Linux, Windows or the WebAssembly build disagrees in a single
 * bit, this test fails there, before a replay quietly goes another way.
 */
static int fixed_step_sine_is_the_same_everywhere(void)
{
    int failed = 0;
    static const struct { float x, sin_x, cos_x; } known[] = {
        {0x0p+0f, 0x0p+0f, 0x1p+0f},                          /* 0       */
        {0x1p-1f, 0x1.eaee88p-2f, 0x1.c1528p-1f},             /* 0.5     */
        {0x1p+0f, 0x1.aed548p-1f, 0x1.14a28p-1f},             /* 1       */
        {-0x1.4p+1f, -0x1.326afp-1f, -0x1.9a2f7ep-1f},        /* -2.5    */
        {0x1.921fb6p+1f, -0x1.777a5cp-24f, -0x1p+0f},         /* π       */
        {0x1.4p+3f, -0x1.1689fp-1f, -0x1.ad9ac8p-1f},         /* 10      */
        {0x1.edd2f2p+6f, -0x1.9b9dc8p-1f, -0x1.307e36p-1f},   /* 123.456 */
        {-0x1.edap+9f, -0x1.6baad2p-1f, 0x1.686718p-1f},      /* -987.25 */
        {0x1.f4p+11f, -0x1.5df436p-1f, -0x1.75bb9cp-1f},      /* 4000    */
    };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        CHECK(game_sinf(known[i].x) == known[i].sin_x);
        CHECK(game_cosf(known[i].x) == known[i].cos_x);
    }
    /* Close to the library's answer: an axe angle in radians stays within
     * ±2π, a bird's wave argument within a few thousand. */
    for (float x = -5000.0f; x < 5000.0f; x += 0.0371f) {
        CHECK(fabsf(game_sinf(x) - sinf(x)) < 2e-7f);
        CHECK(fabsf(game_cosf(x) - cosf(x)) < 2e-7f);
    }
done:
    return failed;
}

/*
 * A jump key still held when a life is lost must not jump the moment the
 * player is back on the ground: the press belongs to the life that ended.
 * Letting go and pressing again jumps as usual. The first step of a level
 * is different: there nothing was held "from before", so a jump recorded
 * on step 0 (a replay, an experiment) still jumps at once; a key that was
 * physically held through the level change is the input release latch's
 * job (game_input.c), not the player's.
 */
static int held_jump_does_not_jump_again_after_a_lost_life(void)
{
    int failed = 0;
    GameState gs = {0};
    strcpy(gs.world.level_path, "tests/fixtures/runtime/transition.toml");
    CHECK(game_init(&gs) == 0);
    Player *p = &gs.world.player;

    /* Level start: a jump held on the very first step jumps. */
    gs.screen.replay_input_mask = PLAYER_INPUT_JUMP;
    game_player_step(&gs, GAME_FIXED_STEP);
    CHECK(p->vy < 0.0f && !p->on_ground);

    /* Still holding jump, take the last heart. */
    for (int step = 0; step < 5; step++) game_player_step(&gs, GAME_FIXED_STEP);
    int lives = gs.world.lives;
    gs.world.hearts = 1;
    p->hurt_timer = 0.0f;
    apply_damage(&gs, 1, 0, p->x, p->y);
    CHECK(gs.world.lives == lives - 1 && p->on_ground);

    /* Jump is still down on the steps after the respawn: no jump. */
    for (int step = 0; step < 3; step++) {
        game_player_step(&gs, GAME_FIXED_STEP);
        CHECK(p->vy == 0.0f && p->on_ground);
    }
    /* Let go, press again: a normal jump. */
    gs.screen.replay_input_mask = 0;
    game_player_step(&gs, GAME_FIXED_STEP);
    gs.screen.replay_input_mask = PLAYER_INPUT_JUMP;
    game_player_step(&gs, GAME_FIXED_STEP);
    CHECK(p->vy < 0.0f && !p->on_ground);
done:
    game_cleanup(&gs);
    return failed;
}

/*
 * The level timer counts fixed steps, not a running float sum of 1/60 s.
 * Adding 1/60 in float loses a little on every step: after ten minutes
 * of play the sum read 599.90 s, and after an hour 3597.2 s. A count of
 * steps divided by 60 at the end is exact.
 */
static int level_timer_stays_exact_over_a_long_run(void)
{
    int failed = 0;
    GameState gs = {0};
    strcpy(gs.world.level_path, "tests/fixtures/runtime/transition.toml");
    CHECK(game_init(&gs) == 0);
    for (int step = 0; step < 10 * 60 * 60; step++)   /* ten minutes */
        game_update_active(&gs, GAME_FIXED_STEP, (int)gs.world.camera.x);
    CHECK(gs.screen.completion.level_steps == 10 * 60 * 60);
    CHECK(game_level_seconds(&gs) == 600.0f);
    /* A Continue point stores seconds; reading them back gives the same
     * count, also after an hour, and odd values are held in range. */
    CHECK(game_level_steps_from_seconds(game_level_seconds(&gs)) == 10 * 60 * 60);
    gs.screen.completion.level_steps = 60 * 60 * 60;
    CHECK(game_level_seconds(&gs) == 3600.0f);
    CHECK(game_level_steps_from_seconds(3600.0f) == 60 * 60 * 60);
    CHECK(game_level_steps_from_seconds(12.5f) == 750);
    CHECK(game_level_steps_from_seconds(-1.0f) == 0);
    CHECK(game_level_steps_from_seconds(NAN) == 0);
    CHECK(game_level_steps_from_seconds(1e9f) == UINT32_MAX);
done:
    game_cleanup(&gs);
    return failed;
}

/* A crumbling platform that has started to fall carries its rider down:
 * they stay standing on it on every step, so the animation does not
 * flicker between idle and falling and a jump off it always works. */
static int falling_crumble_platform_keeps_its_rider_standing(void)
{
    int failed = 0;
    GameState gs = {0};
    stand_player_on_floor(&gs, 100.0f);
    FloatPlatform *fp = &gs.world.float_platforms[0];
    gs.world.float_platform_count = 1;
    float_platform_init(fp, FLOAT_PLATFORM_CRUMBLE, 90.0f, 60.0f, 4,
                        2.0f * GAME_FIXED_STEP, NULL, 0.0f, 0.0f);
    gs.world.player.y = fp->y - gs.world.player.h + PLAYER_FLOOR_SINK;
    gs.world.player.on_ground = 1;
    gs.screen.loop.fp_prev_riding = 0;

    int falling_steps = 0;
    for (int step = 0; step < 50; step++) {
        PlayerStepSupport support = game_player_step(&gs, GAME_FIXED_STEP);
        game_float_platforms_update(&gs, GAME_FIXED_STEP, support.float_platform);
        if (!fp->falling) continue;
        falling_steps++;
        CHECK(support.float_platform == 0);
        CHECK(gs.world.player.on_ground);
        CHECK(NEAR(gs.world.player.y + gs.world.player.h - PLAYER_FLOOR_SINK, fp->y));
    }
    CHECK(falling_steps > 30 && fp->y > 70.0f);
done:
    return failed;
}

/*
 * Drop the player onto a high bouncepad and return how far the launch
 * carries them above the floor, in pixels. Jump is down on the steps in
 * [jump_from, jump_to) and up otherwise; pass 0, 0 for no input.
 */
static float bounce_height(int jump_from, int jump_to)
{
    GameState gs = {0};
    stand_player_on_floor(&gs, 100.0f);
    float floor_y = gs.world.player.y, apex_y = gs.world.player.y;
    gs.world.bouncepad_high_count = 1;
    bouncepad_place(&gs.world.bouncepads_high[0], 100.0f, BOUNCEPAD_VY_HIGH,
                    BOUNCEPAD_RED);
    /* Just above the pad and falling, with the coyote window still open,
     * as it is for a player who walked off a ledge onto the pad. */
    gs.world.player.y -= 4.0f;
    gs.world.player.vy = 60.0f;
    gs.world.player.on_ground = 0;
    gs.world.player.coyote_timer = PLAYER_COYOTE_TIME;
    for (int step = 0; step < 240; step++) {
        gs.screen.replay_input_mask =
            step >= jump_from && step < jump_to ? PLAYER_INPUT_JUMP : 0;
        game_player_step(&gs, GAME_FIXED_STEP);
        if (gs.world.player.y < apex_y) apex_y = gs.world.player.y;
    }
    return floor_y - apex_y;
}

/* A bouncepad launch belongs to the pad: no jump press replaces it through
 * coyote time, and no jump release shortens it. */
static int jump_input_does_not_change_a_bounce(void)
{
    int failed = 0;
    float plain = bounce_height(0, 0);
    CHECK(plain > 200.0f);                       /* a high pad: about 238 px */
    CHECK(NEAR(bounce_height(4, 5), plain));     /* a tap just after the launch */
    CHECK(NEAR(bounce_height(8, 14), plain));    /* press and release while rising */
    CHECK(NEAR(bounce_height(0, 20), plain));    /* held through the landing, then released */
    CHECK(NEAR(bounce_height(4, 5) , bounce_height(4, 240)));  /* tap or hold: the same */
done:
    return failed;
}

/*
 * Gameplay and looks draw from separate random streams: any number of
 * cosmetic draws leaves the gameplay numbers of a seeded run unchanged
 * (and the other way round), both restart from the seed, and neither
 * stream ever gets stuck on zero.
 */
static int cosmetic_random_draws_leave_gameplay_alone(void)
{
    int failed = 0;
    uint32_t plain[8], mixed[8], looks[8];

    game_random_seed(7);
    for (int i = 0; i < 8; i++) plain[i] = game_random();
    game_random_seed(7);
    for (int i = 0; i < 8; i++) looks[i] = game_random_cosmetic();

    game_random_seed(7);
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j <= i; j++) (void)game_random_cosmetic_unit();
        mixed[i] = game_random();
    }
    CHECK(memcmp(plain, mixed, sizeof(plain)) == 0);

    game_random_seed(7);
    for (int i = 0; i < 8; i++) {
        (void)game_random_unit();
        CHECK(game_random_cosmetic() == looks[i]);
    }
    CHECK(plain[0] != looks[0]);                 /* two streams, not one twice */

    /* A unit draw stays inside [0, 1] on both. */
    for (int i = 0; i < 1000; i++) {
        float a = game_random_unit(), b = game_random_cosmetic_unit();
        CHECK(a >= 0.0f && a <= 1.0f && b >= 0.0f && b <= 1.0f);
    }
    /* Seed 0, and the one seed the cosmetic salt would cancel, still work. */
    game_random_seed(0);
    CHECK(game_random() != 0 && game_random_cosmetic() != 0);
    game_random_seed(UINT32_C(0x85ebca6b));
    CHECK(game_random() != 0 && game_random_cosmetic() != 0);
done:
    return failed;
}

/* A frame between two fixed steps draws the player and camera between
 * them, by the accumulator's share of a step, and never touches the run. */
static int render_view_interpolates_without_touching_the_run(void)
{
    int failed = 0;
    GameState gs = {0};
    gs.world.player.x = 100.0f; gs.world.player.y = 200.0f; gs.world.camera.x = 50.0f;
    GameRenderView view = game_timing_render_view(&gs, 50);
    CHECK(view.cam_x == 50 && view.player_x == 100.0f);   /* no earlier step yet */
    game_timing_note_step(&gs);
    gs.world.player.x = 104.0f; gs.world.player.y = 198.0f; gs.world.camera.x = 54.0f;
    gs.screen.loop.accumulator = GAME_FIXED_STEP * 0.5;
    view = game_timing_render_view(&gs, 54);
    CHECK(view.cam_x == 52 && NEAR(view.player_x, 102.0f) && NEAR(view.player_y, 199.0f));
    gs.screen.loop.accumulator = GAME_FIXED_STEP * 0.25;
    view = game_timing_render_view(&gs, 54);
    CHECK(view.cam_x == 51 && NEAR(view.player_x, 101.0f));
    gs.screen.loop.accumulator = GAME_FIXED_STEP * 3.0;    /* clamped to the latest step */
    view = game_timing_render_view(&gs, 54);
    CHECK(view.cam_x == 54 && view.player_x == 104.0f);

    /* A respawn-sized jump is shown where it lands, not swept across. */
    gs.screen.loop.accumulator = GAME_FIXED_STEP * 0.5;
    gs.world.player.x = 900.0f; gs.world.camera.x = 700.0f;
    view = game_timing_render_view(&gs, 700);
    CHECK(view.cam_x == 700 && view.player_x == 900.0f && view.player_y == 198.0f);
    gs.world.player.x = 104.0f; gs.world.camera.x = 54.0f;

    /* Debug, replay and smoke runs draw the latest step itself. */
    gs.screen.debug_mode = 1;
    view = game_timing_render_view(&gs, 54);
    CHECK(view.cam_x == 54 && view.player_x == 104.0f);
    gs.screen.debug_mode = 0;
    strcpy(gs.screen.replay_script_path, "x.replay");
    CHECK(game_timing_render_view(&gs, 54).player_x == 104.0f);
    gs.screen.replay_script_path[0] = '\0';
    gs.screen.smoke_test_frames = 3;
    CHECK(game_timing_render_view(&gs, 54).cam_x == 54);
    gs.screen.smoke_test_frames = 0;

    /* The ghost goes through the same blend: a ghost on the player's last
     * two steps is drawn exactly where the player is. A latest-step view
     * (t = 1) leaves it on its latest sample, and a jump snaps it there. */
    gs.screen.loop.accumulator = GAME_FIXED_STEP * 0.5;
    view = game_timing_render_view(&gs, 54);
    float gx = 104.0f, gy = 198.0f;
    game_timing_view_point(&view, 100.0f, 200.0f, &gx, &gy);
    CHECK(view.t == 0.5f && gx == view.player_x && gy == view.player_y);
    gx = 900.0f; gy = 198.0f;
    game_timing_view_point(&view, 100.0f, 200.0f, &gx, &gy);
    CHECK(gx == 900.0f && gy == 198.0f);
    gs.screen.debug_mode = 1;
    view = game_timing_render_view(&gs, 54);
    gx = 104.0f; gy = 198.0f;
    game_timing_view_point(&view, 100.0f, 200.0f, &gx, &gy);
    CHECK(view.t == 1.0f && gx == 104.0f && gy == 198.0f);
    gs.screen.debug_mode = 0;

    /* Movers recorded before the step are drawn between that position and
     * the one after it. A bird stores x only: its height is a sine of x. */
    gs.screen.loop.accumulator = GAME_FIXED_STEP * 0.5;
    gs.world.spider_count = 1;
    gs.world.spiders[0].x = 10.0f;
    gs.world.bird_count = 1;
    gs.world.birds[0].x = 40.0f;
    gs.world.fish_count = 1;
    gs.world.fish[0].x = 20.0f;
    gs.world.fish[0].y = 80.0f;
    game_timing_note_step(&gs);
    gs.world.spiders[0].x = 18.0f;
    gs.world.birds[0].x = 44.0f;
    gs.world.fish[0].x = 24.0f;
    gs.world.fish[0].y = 70.0f;
    view = game_timing_render_view(&gs, 54);
    CHECK(view.t == 0.5f);
    {
        Spider spiders[MAX_SPIDERS];
        Bird birds[MAX_BIRDS];
        Fish fish[MAX_FISH];
        int count = -1;
        const Spider *spider = game_movers_shown(&gs, MOVER_SPIDERS, view.t,
                                                 spiders, sizeof(spiders), &count);
        CHECK(count == 1 && spider == spiders && NEAR(spider[0].x, 14.0f));
        CHECK(gs.world.spiders[0].x == 18.0f);            /* the world is not written */
        const Bird *bird = game_movers_shown(&gs, MOVER_BIRDS, view.t, birds, sizeof(birds), &count);
        CHECK(count == 1 && NEAR(bird[0].x, 42.0f));
        const Fish *one = game_movers_shown(&gs, MOVER_FISH, view.t, fish, sizeof(fish), &count);
        CHECK(count == 1 && NEAR(one[0].x, 22.0f) && NEAR(one[0].y, 75.0f));
        /* A latest-step view draws the world's own array, uncopied. */
        CHECK(game_movers_shown(&gs, MOVER_SPIDERS, 1.0f, spiders, sizeof(spiders), &count) ==
              (const void *)gs.world.spiders);
        /* A step longer than the snap distance is shown where it landed. */
        gs.world.spiders[0].x = 10.0f + GAME_INTERP_MAX_JUMP + 1.0f;
        spider = game_movers_shown(&gs, MOVER_SPIDERS, view.t, spiders, sizeof(spiders), &count);
        CHECK(spider[0].x == gs.world.spiders[0].x);
        /* One axis jumping takes the other with it: both are "now". */
        gs.world.fish[0].x = 20.0f + GAME_INTERP_MAX_JUMP + 5.0f;
        one = game_movers_shown(&gs, MOVER_FISH, view.t, fish, sizeof(fish), &count);
        CHECK(one[0].x == gs.world.fish[0].x && one[0].y == 70.0f);
    }

    /* Things that do not travel are blended too: an axe's angle (the
     * shorter way round, so a spin from 356 to 4 degrees passes through 0
     * instead of swinging back), a flame's height, and each falling brick
     * of a bridge. They used to jump at 60 Hz beside smooth neighbours. */
    gs.world.axe_trap_count = 2;
    gs.world.axe_traps[0].angle = 10.0f;
    gs.world.axe_traps[1].angle = 356.0f;
    gs.world.blue_flame_count = 1;
    gs.world.blue_flames[0].y = 200.0f;
    gs.world.blue_flames[0].angle = 0.0f;
    gs.world.fire_flame_count = 1;
    gs.world.fire_flames[0].y = 150.0f;
    gs.world.bridge_count = 1;
    gs.world.bridges[0].brick_count = 3;
    gs.world.bridges[0].bricks[1].y_offset = 0.0f;
    gs.world.bridges[0].bricks[2].y_offset = 6.0f;
    gs.world.circular_saw_count = 1;
    gs.world.circular_saws[0].x = 30.0f;
    gs.world.circular_saws[0].y = 60.0f;
    gs.world.circular_saws[0].spin_angle = 350.0f;
    gs.world.spike_block_count = 1;
    gs.world.spike_blocks[0].x = 70.0f;
    gs.world.spike_blocks[0].y = 90.0f;
    gs.world.spike_blocks[0].spin_angle = 100.0f;
    game_timing_note_step(&gs);
    gs.world.circular_saws[0].x = 34.0f;
    gs.world.circular_saws[0].spin_angle = 10.0f;
    gs.world.spike_blocks[0].y = 94.0f;
    gs.world.spike_blocks[0].spin_angle = 110.0f;
    gs.world.axe_traps[0].angle = 20.0f;
    gs.world.axe_traps[1].angle = 4.0f;
    gs.world.blue_flames[0].y = 190.0f;
    gs.world.blue_flames[0].angle = 30.0f;
    gs.world.fire_flames[0].y = 144.0f;
    gs.world.bridges[0].bricks[1].y_offset = 4.0f;
    gs.world.bridges[0].bricks[2].y_offset = 12.0f;
    view = game_timing_render_view(&gs, 54);
    {
        AxeTrap axes[MAX_AXE_TRAPS];
        BlueFlame flames[MAX_BLUE_FLAMES];
        Bridge bridges[MAX_BRIDGES];
        int count = -1;
        const AxeTrap *axe = game_movers_shown(&gs, MOVER_AXE_TRAPS, view.t, axes, sizeof(axes), &count);
        CHECK(count == 2 && NEAR(axe[0].angle, 15.0f));
        /* Half way from 356 to 4 by the short way is 0 (or 360), not 180. */
        CHECK(NEAR(axe[1].angle, 0.0f) || NEAR(axe[1].angle, 360.0f));
        const BlueFlame *flame = game_movers_shown(&gs, MOVER_BLUE_FLAMES, view.t,
                                                   flames, sizeof(flames), &count);
        CHECK(count == 1 && NEAR(flame[0].y, 195.0f) && NEAR(flame[0].angle, 15.0f));
        flame = game_movers_shown(&gs, MOVER_FIRE_FLAMES, view.t, flames, sizeof(flames), &count);
        CHECK(count == 1 && NEAR(flame[0].y, 147.0f));
        const Bridge *bridge = game_movers_shown(&gs, MOVER_BRIDGES, view.t,
                                                 bridges, sizeof(bridges), &count);
        CHECK(count == 1 && bridge[0].brick_count == 3);
        CHECK(bridge[0].bricks[0].y_offset == 0.0f);
        CHECK(NEAR(bridge[0].bricks[1].y_offset, 2.0f) && NEAR(bridge[0].bricks[2].y_offset, 9.0f));
        /* A saw and a spike block spin as they travel: the spin is a third
         * blended value, and it too goes the short way across 360. */
        {
            CircularSaw saws[MAX_CIRCULAR_SAWS];
            SpikeBlock blocks[MAX_SPIKE_BLOCKS];
            const CircularSaw *saw = game_movers_shown(&gs, MOVER_CIRCULAR_SAWS, view.t,
                                                       saws, sizeof(saws), &count);
            CHECK(count == 1 && NEAR(saw[0].x, 32.0f) && NEAR(saw[0].y, 60.0f));
            /* Half way from 350 to 10 is 0 (or 360), not 180. */
            CHECK(NEAR(saw[0].spin_angle, 0.0f) || NEAR(saw[0].spin_angle, 360.0f));
            const SpikeBlock *block = game_movers_shown(&gs, MOVER_SPIKE_BLOCKS, view.t,
                                                        blocks, sizeof(blocks), &count);
            CHECK(count == 1 && NEAR(block[0].y, 92.0f) && NEAR(block[0].spin_angle, 105.0f));
        }
        /* A buffer too small for the array is refused, not overrun. */
        CHECK(game_movers_shown(&gs, MOVER_BRIDGES, view.t, bridges, 8, &count) ==
              (const void *)gs.world.bridges);
    }
    /* Every kind has a row, and a kind out of range has none. */
    for (int kind = 0; kind < MOVER_KIND_COUNT; kind++)
        CHECK(game_mover_name((GameMoverKind)kind) != NULL);
    CHECK(game_mover_name(MOVER_KIND_COUNT) == NULL);

    /* Restarting the clock (a level load does) forgets the noted positions,
     * so the next frame shows everything where it is now. */
    game_timing_restart_clock(&gs);
    view = game_timing_render_view(&gs, 54);
    CHECK(view.t == 1.0f && view.cam_x == 54);

    /* Noting each step changes nothing the simulation keeps. */
    GameState a = {0}, b = {0};
    stand_player_on_floor(&a, 100.0f);
    stand_player_on_floor(&b, 100.0f);
    for (int step = 0; step < 90; step++) {
        a.screen.replay_input_mask = b.screen.replay_input_mask =
            (step < 40 ? PLAYER_INPUT_RIGHT : 0) | (step % 30 < 3 ? PLAYER_INPUT_JUMP : 0);
        game_timing_note_step(&a);
        (void)game_timing_render_view(&a, (int)a.world.camera.x);
        game_player_step(&a, GAME_FIXED_STEP);
        game_player_step(&b, GAME_FIXED_STEP);
    }
    CHECK(memcmp(&a.world, &b.world, sizeof(a.world)) == 0);
    CHECK(a.world.player.x != 100.0f);   /* the run did move */
done:
    return failed;
}

int game_simulation_contract_test(void)
{
    int failures = inspection_and_replay();
    printf("simulation: inspection/export/replay %s\n", failures ? "FAIL" : "PASS");
    int scenario = moving_support_and_damage();
    printf("simulation: moving support/hazard/checkpoint %s\n", scenario ? "FAIL" : "PASS");
    int rect_rail = rect_rail_platform_carries_rider_down_and_catches_from_above();
    printf("simulation: RECT rail platform carries and catches the player %s\n", rect_rail ? "FAIL" : "PASS");
    failures += rect_rail;
    int jump = jump_height_ignores_frame_rate();
    printf("simulation: jump apex independent of frame rate %s\n", jump ? "FAIL" : "PASS");
    int tunnel = fast_fall_does_not_tunnel_through_platform();
    printf("simulation: fast fall lands on platform %s\n", tunnel ? "FAIL" : "PASS");
    int gap_walls = sinking_into_a_gap_cannot_steer_back_onto_the_floor();
    printf("simulation: floor gap sides hold a sinking player %s\n", gap_walls ? "FAIL" : "PASS");
    failures += gap_walls;
    int stars = health_stars_heal_one_heart_up_to_the_cap();
    printf("simulation: health stars heal up to the cap %s\n", stars ? "FAIL" : "PASS");
    failures += stars;
    int jumps = jump_buffer_coyote_time_and_short_hops();
    printf("simulation: jump buffer, coyote time and short hops %s\n", jumps ? "FAIL" : "PASS");
    failures += jumps;
    int streams = cosmetic_random_draws_leave_gameplay_alone();
    printf("simulation: cosmetic random draws leave gameplay alone %s\n", streams ? "FAIL" : "PASS");
    failures += streams;
    int trig = fixed_step_sine_is_the_same_everywhere();
    printf("simulation: fixed-step sine and cosine are the same everywhere %s\n", trig ? "FAIL" : "PASS");
    failures += trig;
    int outlines = debug_coin_and_bouncepad_outlines_match_the_game();
    printf("simulation: debug coin and bouncepad outlines match the game %s\n", outlines ? "FAIL" : "PASS");
    failures += outlines;
    int respawn_jump = held_jump_does_not_jump_again_after_a_lost_life();
    printf("simulation: held jump does not jump again after a lost life %s\n", respawn_jump ? "FAIL" : "PASS");
    failures += respawn_jump;
    int timer = level_timer_stays_exact_over_a_long_run();
    printf("simulation: level timer stays exact over a long run %s\n", timer ? "FAIL" : "PASS");
    failures += timer;
    int crumble = falling_crumble_platform_keeps_its_rider_standing();
    printf("simulation: falling crumble platform keeps its rider standing %s\n", crumble ? "FAIL" : "PASS");
    failures += crumble;
    int bounce = jump_input_does_not_change_a_bounce();
    printf("simulation: jump input leaves a bouncepad launch alone %s\n", bounce ? "FAIL" : "PASS");
    failures += bounce;
    int collide = collision_hurts_once_and_collects_coins();
    printf("simulation: hazard and coin collision %s\n", collide ? "FAIL" : "PASS");
    int streak = coin_streak_raises_the_chime();
    printf("simulation: coin streak pitch %s\n", streak ? "FAIL" : "PASS");
    int interp = render_view_interpolates_without_touching_the_run();
    printf("simulation: render view between steps %s\n", interp ? "FAIL" : "PASS");
    return failures + scenario + jump + tunnel + collide + streak + interp;
}
