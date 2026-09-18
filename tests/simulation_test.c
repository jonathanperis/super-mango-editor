/* Multi-frame behavior proofs using stable fixtures and production update paths. */
#include "core/game_experiment.h"
#include "core/game_inspector.h"
#include "core/game_overlay.h"
#include "core/game_resources.h"
#include "core/game_update.h"
#include "collision/collision_damage.h"
#include "screens/settings_menu.h"
#include "input/game_events.h"
#include "player/player_internal.h"
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
    key(&gs, KEY_F2);
    CHECK(game_inspector_step(&gs, 0.04f) == 0);
    key(&gs, KEY_F3);
    CHECK(NEAR(game_inspector_step(&gs, 0.04f), 1.0f / 60));
    CHECK(game_inspector_step(&gs, 0.04f) == 0);
    key(&gs, KEY_F3);
    game_overlay_set_pause_reason(&gs, GAME_PAUSE_REASON_FOCUS, 1);
    CHECK(game_inspector_step(&gs, 0.04f) == 0 && !gs.inspector.step_requested);
    game_overlay_set_pause_reason(&gs, GAME_PAUSE_REASON_FOCUS, 0);
    CHECK(game_inspector_step(&gs, 0.04f) == 0);
    key(&gs, KEY_F2); key(&gs, KEY_F4);
    CHECK(NEAR(game_inspector_step(&gs, 0.04f), 0.01f));
    SettingsMenu settings = {.open = 1};
    gs.settings_menu = &settings;
    key(&gs, KEY_F3);
    CHECK(game_inspector_step(&gs, 0.04f) == 0);
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
        float dt = i < 90 ? 1.0f / 60 : 1.0f / 120;
        game_update_active(&gs, dt, (int)gs.camera.x);
    }
    CHECK(gs.experiment->count == 180 && gs.player.x > 150);
    Player recorded = gs.player;
    float elapsed = gs.completion.level_elapsed;
    int score = gs.score, checkpoint = gs.checkpoint_index;
    remove("out/school-experiment.toml");
    CHECK(game_experiment_save(&gs, "out/school-experiment.toml") == 0);
    CHECK(game_experiment_save(&gs, "out/school-experiment.toml") == -1);
    CHECK(game_experiment_load(&gs, "out/school-experiment.toml") == 0);
    for (int i = 0; i < 180; i++) {
        /* Opposite live input must not perturb replay. */
        gs.replay_input_mask = PLAYER_INPUT_LEFT;
        float dt = game_inspector_step(&gs, 0.07f);
        CHECK(dt > 0);
        game_update_active(&gs, dt, (int)gs.camera.x);
    }
    CHECK(NEAR(gs.player.x, recorded.x) && NEAR(gs.player.y, recorded.y));
    CHECK(NEAR(gs.player.vx, recorded.vx) && NEAR(gs.player.vy, recorded.vy));
    CHECK(NEAR(gs.completion.level_elapsed, elapsed) && gs.score == score && gs.checkpoint_index == checkpoint);
    CHECK(game_inspector_step(&gs, 0.04f) == 0);
    FILE *bad = fopen("out/school-experiment-invalid.toml", "w");
    CHECK(bad != NULL);
    fputs("format_version = 9\n", bad); fclose(bad);
    GameExperiment *before = gs.experiment;
    CHECK(game_experiment_load(&gs, "out/school-experiment-invalid.toml") == -1 && gs.experiment == before);
    const char *bad_rows[] = {
        "[nan, 0, 100, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[0.016, 64, 100, 250, 750, 600, 550, 100, 350, 180, 80]",
        "[0.016, 0, 1e100, 250, 750, 600, 550, 100, 350, 180, 80]"
    };
    for (size_t i = 0; i < sizeof(bad_rows) / sizeof(bad_rows[0]); i++) {
        bad = fopen("out/school-experiment-invalid.toml", "w");
        CHECK(bad != NULL);
        fprintf(bad, "format_version = 1\nlevel_path = \"fixture\"\nseed = 7\nlevel_hash = \"%016llx\"\nframes = [%s]\n",
                (unsigned long long)gs.source_level_hash, bad_rows[i]);
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

int game_simulation_contract_test(void)
{
    int failures = inspection_and_replay();
    printf("simulation: inspection/export/replay %s\n", failures ? "FAIL" : "PASS");
    int scenario = moving_support_and_damage();
    printf("simulation: moving support/hazard/checkpoint %s\n", scenario ? "FAIL" : "PASS");
    return failures + scenario;
}
