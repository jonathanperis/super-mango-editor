/* Multi-frame behavior proofs using stable fixtures and production update paths. */
#include "core/game_experiment.h"
#include "core/game_inspector.h"
#include "core/game_overlay.h"
#include "core/game_resources.h"
#include "core/game_timing.h"
#include "core/game_player_step.h"
#include "collision/game_collision.h"
#include "core/game_update.h"
#include "collision/collision_damage.h"
#include "screens/settings_menu.h"
#include "input/game_events.h"
#include "player/player_internal.h"
#include "shared/platform.h"   /* clock_millis */
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { fprintf(stderr, "simulation_test:%d: %s\n", __LINE__, #test); failed = 1; goto done; } } while (0)
#define NEAR(a,b) (fabsf((a)-(b)) < 0.001f)

static void key(GameState *gs, int keycode)
{
    InputEvent event = {.type=INPUT_KEY_DOWN,.key=keycode};
    (void)game_inspector_event(gs, &event);
}

static int inspection_and_replay(void)
{
    int failed = 0;
    GameState gs = {0};
    gs.debug_mode = 1;
    gs.random_seed = 7;
    strcpy(gs.level_path, "tests/fixtures/runtime/transition.toml");
    CHECK(game_init(&gs) == 0);
    input_clear();
    /* F5 opens and closes the key help; F6 keeps the tuning line visible for
     * a few seconds instead of a permanent panel row. */
    CHECK(!gs.inspector.show_keys);
    key(&gs, KEY_F5);
    CHECK(gs.inspector.show_keys);
    key(&gs, KEY_F5);
    CHECK(!gs.inspector.show_keys);
    {
        /* The deadline is set from the clock at the key press, so it is at
         * least 3 s after a reading taken before the press, however long
         * the test process pauses afterwards. */
        uint64_t before_f6 = clock_millis();
        key(&gs, KEY_F6);
        CHECK(gs.inspector.tuning_visible_until >= before_f6 + 3000);
    }
    gs.inspector.physics_field = 0;
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
    CHECK(game_inspector_steps(&gs, 0.04f) == 0 && !gs.inspector.step_requested);
    game_overlay_set_pause_reason(&gs, GAME_PAUSE_REASON_FOCUS, 0);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    /* Slow mode 0.25x: 40 ms of real time is 10 ms of game time, so steps
     * arrive every other frame instead of becoming shorter. */
    key(&gs, KEY_F2); key(&gs, KEY_F4);
    CHECK(game_inspector_steps(&gs, 0.04f) == 1);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    CHECK(game_inspector_steps(&gs, 0.04f) == 1);
    SettingsMenu settings = {.open = 1};
    gs.settings_menu = &settings;
    key(&gs, KEY_F3);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    settings.capture = 1;
    InputEvent reserved = {.type=INPUT_KEY_DOWN,.key=KEY_F3};
    input_push(&reserved); game_handle_events(&gs);
    CHECK(settings.capture == 1 && strstr(settings.message, "Reserved for debug") != NULL);
    gs.settings_menu = NULL;
    float speed = gs.player.walk_max_speed;
    key(&gs, KEY_EQUAL);
    CHECK(gs.player.walk_max_speed == speed + 25);
    key(&gs, KEY_F7);
    CHECK(gs.player.walk_max_speed == speed);
    CHECK(game_experiment_begin(&gs) == 0);
    for (int i = 0; i < 180; i++) {
        gs.replay_input_mask = i < 120 ? PLAYER_INPUT_RIGHT : 0;
        if (i == 30) gs.replay_input_mask |= PLAYER_INPUT_JUMP;
        if (i == 60) gs.player.walk_max_speed = 150;
        game_update_active(&gs, GAME_FIXED_STEP, (int)gs.camera.x);
    }
    CHECK(gs.experiment->count == 180 && gs.player.x > 150);
    Player recorded = gs.player;
    float elapsed = gs.completion.level_elapsed;
    int score = gs.score, checkpoint = gs.checkpoint_index;
    remove("out/school-experiment.toml");
    CHECK(game_experiment_save(&gs, "out/school-experiment.toml") == 0);
    CHECK(game_experiment_save(&gs, "out/school-experiment.toml") == -1);
    CHECK(game_experiment_load(&gs, "out/school-experiment.toml") == 0);
    /* Replay through the same frame loop shape as game_frame: real frames of
     * 70 ms run several fixed steps each, one recorded row per step. */
    int replayed = 0;
    for (int frame = 0; frame < 400 && replayed < 180; frame++) {
        /* Opposite live input must not perturb replay. */
        gs.replay_input_mask = PLAYER_INPUT_LEFT;
        int steps = game_inspector_steps(&gs, 0.07f);
        for (int s = 0; s < steps; s++) {
            float dt = game_experiment_dt(&gs, GAME_FIXED_STEP);
            if (dt <= 0) break;
            game_update_active(&gs, dt, (int)gs.camera.x);
            replayed++;
        }
    }
    CHECK(replayed == 180);
    CHECK(NEAR(gs.player.x, recorded.x) && NEAR(gs.player.y, recorded.y));
    CHECK(NEAR(gs.player.vx, recorded.vx) && NEAR(gs.player.vy, recorded.vy));
    CHECK(NEAR(gs.completion.level_elapsed, elapsed) && gs.score == score && gs.checkpoint_index == checkpoint);
    CHECK(game_inspector_steps(&gs, 0.04f) == 0);
    FILE *bad = fopen("out/school-experiment-invalid.toml", "w");
    CHECK(bad != NULL);
    fputs("format_version = 9\n", bad); fclose(bad);
    GameExperiment *before = gs.experiment;
    CHECK(game_experiment_load(&gs, "out/school-experiment-invalid.toml") == -1 && gs.experiment == before);
    /* Rows are [input, 9 physics values]. Format 1 rows (with a leading
     * frame duration) came from the variable-timestep engine: refused. */
    const char *bad_rows[] = {
        "[0, nan, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[64, 100, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[0, 1e100, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[0.016, 0, 100, 250, 750, 600, 550, 100, 350, 180, 80]"
    };
    const int bad_versions[] = {2, 2, 2, 1};
    for (size_t i = 0; i < sizeof(bad_rows) / sizeof(bad_rows[0]); i++) {
        bad = fopen("out/school-experiment-invalid.toml", "w");
        CHECK(bad != NULL);
        fprintf(bad, "format_version = %d\nlevel_path = \"fixture\"\nseed = 7\nlevel_hash = \"%016llx\"\nframes = [%s]\n",
                bad_versions[i], (unsigned long long)gs.source_level_hash, bad_rows[i]);
        fclose(bad);
        CHECK(game_experiment_load(&gs, "out/school-experiment-invalid.toml") == -1 && gs.experiment == before);
    }
    gs.source_level_hash ^= 1; /* Loaded bytes no longer match the file. */
    CHECK(game_experiment_begin(&gs) == -1 && gs.experiment == before);
    gs.source_level_hash ^= 1;
done:
    remove("out/school-experiment.toml"); remove("out/school-experiment-invalid.toml");
    game_cleanup(&gs);
    return failed;
}

static int moving_support_and_damage(void)
{
    int failed = 0;
    GameState gs = {0};
    strcpy(gs.level_path, "tests/fixtures/runtime/moving_support.toml");
    CHECK(game_init(&gs) == 0);
    gs.player.x = gs.float_platforms[0].x;
    gs.player.y = gs.float_platforms[0].y - gs.player.h + PLAYER_FLOOR_SINK;
    gs.player.on_ground = 1;
    gs.loop.fp_prev_riding = 0;
    float offset = gs.player.x - gs.float_platforms[0].x;
    for (int i = 0; i < 180; i++) game_update_active(&gs, 1.0f / 60, (int)gs.camera.x);
    CHECK(gs.loop.fp_prev_riding == 0 && NEAR(gs.player.x - gs.float_platforms[0].x, offset));

    /* A saw starts one pixel outside the player and enters during this step. */
    gs.player.x = 48; gs.player.y = FLOOR_Y - gs.player.h + PLAYER_FLOOR_SINK;
    gs.player.vx = gs.player.vy = gs.player.hurt_timer = 0;
    IntRect hit = player_get_hitbox(&gs.player);
    gs.circular_saw_count = 1;
    gs.circular_saws[0] = (CircularSaw){.x = hit.x + hit.w - 3, .y = 220,
        .w = 32, .h = 32, .active = 1, .direction = -1, .patrol_x0 = 0, .patrol_x1 = 300};
    int hearts = gs.hearts;
    game_update_active(&gs, 1.0f / 60, 0);
    CHECK(gs.hearts == hearts - 1);
    LevelDef *def = gs.level_def;
    def->circular_saw_count = 1;
    Texture2D *texture = gs.textures.circular_saw;
    gs.textures.circular_saw = NULL;
    int missing = game_resources_require_level_textures(&gs, def);
    gs.textures.circular_saw = texture;
    CHECK(missing == -1);

    /* Authored checkpoints persist through an actual lethal-damage reset. */
    def->checkpoint_count = 1; def->checkpoints[0] = (CheckpointPlacement){304, 252};
    gs.player.x = 320;
    gs.player.hurt_timer = 0;
    game_update_active(&gs, 1.0f / 60, 0);
    CHECK(gs.checkpoint_index == 0);
    gs.hearts = 1; gs.player.hurt_timer = 0;
    apply_damage(&gs, 1, 0, gs.player.x, gs.player.y);
    CHECK(gs.checkpoint_index == 0 && NEAR(gs.player.x, 304));
done:
    game_cleanup(&gs);
    return failed;
}

/* A bare GameState with one player standing on the ground floor. */
static void stand_player_on_floor(GameState *gs, float x)
{
    gs->runtime.world_w = 1600;
    gs->player.w = gs->player.h = 48;
    player_apply_default_physics(&gs->player);
    gs->player.spawn_x = x;
    gs->player.spawn_y = FLOOR_Y;
    player_reset(&gs->player);
    gs->loop.fp_prev_riding = -1;
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
    float floor_y = gs.player.y, apex_y = gs.player.y;
    int step_index = 0;
    game_timing_restart_clock(&gs);
    for (int frame = 0; frame < (int)(render_hz * 2.0f); frame++) {
        float seconds = 1.0f / render_hz + (frame % 2 ? jitter : -jitter);
        int steps = game_timing_take_steps(&gs, seconds);
        for (int s = 0; s < steps; s++, step_index++) {
            gs.replay_input_mask = step_index < 30 ? PLAYER_INPUT_JUMP : 0;
            game_player_step(&gs, GAME_FIXED_STEP);
            if (gs.player.y < apex_y) apex_y = gs.player.y;
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
    int bounce = -1, support = -1;
    for (int step = 0; step < 60 && !player.on_ground; step++)
        player_update(&player, GAME_FIXED_STEP, NULL, &pillar, 1, NULL, 0,
                      NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0,
                      NULL, 0, NULL, 0, &bounce, &support, -1, 400);
    CHECK(player.on_ground);
    CHECK(NEAR(player.y + player.h - PLAYER_FLOOR_SINK, pillar.y));
    CHECK(player.vy == 0.0f);
done:
    return failed;
}

static int collision_hurts_once_and_collects_coins(void)
{
    int failed = 0;
    GameState gs = {0};
    stand_player_on_floor(&gs, 100.0f);
    gs.hearts = 3;
    gs.lives = 3;
    gs.rules.coin_score = 100;
    gs.rules.score_per_life = 1000;
    gs.score_life_next = 1000;
    gs.coin_count = 1;
    gs.coins[0] = (Coin){.x = 300, .y = 230, .active = 1};
    gs.spike_row_count = 1;
    gs.spike_rows[0] = (SpikeRow){.x = 110, .y = FLOOR_Y - SPIKE_TILE_H, .count = 2, .active = 1};

    /* Standing in the spikes: one heart lost, knockback, invincibility. */
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.hearts == 2 && gs.player.hurt_timer > 0.0f && gs.player.vx != 0.0f);
    CHECK(gs.coins[0].active == 1 && gs.score == 0);
    /* Still overlapping during invincibility: no second hit. */
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.hearts == 2);

    /* Overlapping the coin collects it exactly once. */
    gs.player.x = 290.0f;
    game_collide(&gs, GAME_FIXED_STEP);
    game_collide(&gs, GAME_FIXED_STEP);
    CHECK(gs.coins[0].active == 0 && gs.score == 100 && gs.hearts == 2);
done:
    return failed;
}

int game_simulation_contract_test(void)
{
    int failures = inspection_and_replay();
    printf("simulation: inspection/export/replay %s\n", failures ? "FAIL" : "PASS");
    int scenario = moving_support_and_damage();
    printf("simulation: moving support/hazard/checkpoint %s\n", scenario ? "FAIL" : "PASS");
    int jump = jump_height_ignores_frame_rate();
    printf("simulation: jump apex independent of frame rate %s\n", jump ? "FAIL" : "PASS");
    int tunnel = fast_fall_does_not_tunnel_through_platform();
    printf("simulation: fast fall lands on platform %s\n", tunnel ? "FAIL" : "PASS");
    int collide = collision_hurts_once_and_collects_coins();
    printf("simulation: hazard and coin collision %s\n", collide ? "FAIL" : "PASS");
    return failures + scenario + jump + tunnel + collide;
}
