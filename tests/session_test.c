#include <stdio.h>

#include "input/input_backend.h"

#include "collision/collision_damage.h"
#include "collision/game_collision.h"
#include "core/app_session.h"
#include "core/game_overlay.h"
#include "core/game_timing.h"
#include "core/game_update.h"
#include "input/game_input.h"
#include "input/game_web_input.h"
#include "levels/level.h"
#include "levels/level_session.h"
#include "levels/level_loader.h"
#include "levels/level_path.h"
#include "levels/level_resources.h"
#include "player/player_surfaces.h"
#include "player/player_internal.h"

extern float test_last_music_volume;

/* A distinctive GPU/software texture must survive screen swaps. This proves
 * context ownership on both real windows and raylib's Memory test backend. */
static Texture2D context_probe_open(void)
{
    Image image = GenImageColor(3, 3, (Color){23,117,211,255});
    Texture2D probe = LoadTextureFromImage(image);
    UnloadImage(image);
    return probe;
}

static int context_probe_alive(Texture2D probe)
{
    /* Memory supports framebuffer readback, not direct texture readback. */
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(probe, 0, 0, WHITE);
    rlDrawRenderBatchActive();
    Image image = LoadImageFromScreen();
    EndDrawing();
    int ok = IsImageValid(image) && image.width > 1 && image.height > 1;
    if (ok) {
#ifdef MANGO_MEMORY_TESTS
        /* raylib 6.0 Memory readback is bottom-origin/BGRA. Normalize only this
         * test probe; desktop rendering/readback still has the RGBA contract. */
        Color pixel = GetImageColor(image, 1, image.height-2);
        unsigned char red = pixel.r;
        pixel.r = pixel.b;
        pixel.b = red;
#else
        Color pixel = GetImageColor(image, 1, 1);
#endif
        ok = pixel.r == 23 && pixel.g == 117 && pixel.b == 211 && pixel.a == 255;
        if (!ok) fprintf(stderr, "context probe: texture %u, image %dx%d, pixel %u/%u/%u/%u\n",
                         probe.id, image.width, image.height, pixel.r, pixel.g, pixel.b, pixel.a);
    }
    UnloadImage(image);
    return ok;
}

static int expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        fprintf(stderr, "session_test: %s got %d expected %d\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int expect_float(const char *name, float actual, float expected)
{
    float diff = actual - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 0.001f) {
        fprintf(stderr, "session_test: %s got %.3f expected %.3f\n",
                name, actual, expected);
        return 1;
    }
    return 0;
}

static int push_confirm(void)
{
    InputEvent event = {.type=INPUT_KEY_DOWN,.key=KEY_ENTER};
    return input_push(&event) == 1 ? 0 : 1;
}

static int push_key(int key)
{
    InputEvent event = {.type=INPUT_KEY_DOWN,.key=key};
    return input_push(&event) == 1 ? 0 : 1;
}

static int push_controller_button(int button)
{
    InputEvent event = {.type=INPUT_PAD_DOWN,.button=button};
    return input_push(&event) == 1 ? 0 : 1;
}

static int physical_release_latch_blocks_transition_input(void)
{
    GameState gs = {0};
    Player player = {0};
    unsigned int sampled;

    game_input_test_set_physical_state(PLAYER_INPUT_JUMP, 0);
    game_input_arm_release_latch(&gs, NULL);
    sampled = game_input_sample(&gs);
    if (expect_int("held Space is gated", sampled, 0) != 0 ||
        expect_int("Space latch armed", gs.input_release_latched, 1) != 0)
        goto fail;
    player_handle_input(&player, NULL, 0, sampled,
                        NULL, 0, NULL, 0, NULL, 0);
    if (expect_int("held Space cannot jump", player.jump_held, 0) != 0)
        goto fail;

    game_input_test_set_physical_state(0, 0);
    if (expect_int("Space release clears gate", game_input_sample(&gs), 0) != 0 ||
        expect_int("Space latch cleared", gs.input_release_latched, 0) != 0)
        goto fail;

    game_input_test_set_physical_state(PLAYER_INPUT_LEFT, 0);
    game_input_arm_release_latch(&gs, NULL);
    if (expect_int("held A is gated", game_input_sample(&gs), 0) != 0)
        goto fail;
    game_input_test_set_physical_state(0, 0);
    (void)game_input_sample(&gs);

    game_input_test_set_physical_state(0, PLAYER_INPUT_JUMP | GAME_INPUT_CONFIRM);
    game_input_arm_release_latch(&gs, NULL);
    if (expect_int("held controller A is gated", game_input_sample(&gs), 0) != 0)
        goto fail;
    game_input_test_set_physical_state(0, 0);
    (void)game_input_sample(&gs);

    game_input_test_set_physical_state(0, PLAYER_INPUT_RIGHT | GAME_INPUT_CONFIRM);
    game_input_arm_release_latch(&gs, NULL);
    if (expect_int("held D-pad is gated", game_input_sample(&gs), 0) != 0)
        goto fail;
    game_input_test_set_physical_state(0, 0);
    if (expect_int("D-pad release clears gate", game_input_sample(&gs), 0) != 0)
        goto fail;

    game_input_test_clear_physical_state();
    return 0;

fail:
    game_input_test_clear_physical_state();
    return 1;
}

static int campaign_manifest_is_ordered_and_transactional(void)
{
    CampaignCatalog catalog = {0};
    CampaignLevel *original_levels;

    if (campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &catalog) != 0) {
        fprintf(stderr, "session_test: campaign manifest failed to load\n");
        return 1;
    }
    if (expect_int("campaign level count", (int)catalog.count, 3) != 0 ||
        expect_int("campaign first path",
                   strcmp(catalog.levels[0].path,
                          "levels/00_sandbox_01.toml") == 0, 1) != 0 ||
        expect_int("campaign first display name",
                   strcmp(catalog.levels[0].display_name,
                          "Creator's Playground") == 0, 1) != 0 ||
        expect_int("campaign sandbox successor chain",
                   strcmp(catalog.levels[0].level.next_phase,
                          "levels/01_lugio_01.toml") == 0, 1) != 0 ||
        expect_int("campaign Lugio successor chain",
                   strcmp(catalog.levels[1].level.next_phase,
                          "levels/02_lugio_02.toml") == 0, 1) != 0 ||
        expect_int("campaign terminal chain",
                   catalog.levels[2].level.next_phase[0] == '\0', 1) != 0)
        goto fail;

    original_levels = catalog.levels;
    if (campaign_catalog_load("levels/campaigns/missing.toml", &catalog) == 0 ||
        expect_int("failed campaign load preserves catalog",
                   catalog.levels == original_levels, 1) != 0 ||
        expect_int("failed campaign load preserves count", (int)catalog.count, 3) != 0)
        goto fail;

    campaign_catalog_cleanup(&catalog);
    return 0;

fail:
    campaign_catalog_cleanup(&catalog);
    return 1;
}

static int campaign_manifest_nul_fixtures_reject_transactionally(void)
{
    static const char *const fixtures[] = {
        "tests/fixtures/campaign_manifest/bad_format_version_key_embedded_nul.toml",
        "tests/fixtures/campaign_manifest/bad_levels_key_embedded_nul.toml",
        "tests/fixtures/campaign_manifest/bad_path_embedded_nul.toml",
    };
    CampaignCatalog catalog = {0};
    CampaignLevel *original_levels;
    size_t original_count;

    if (campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &catalog) != 0) {
        fprintf(stderr, "session_test: campaign NUL baseline failed to load\n");
        return 1;
    }
    original_levels = catalog.levels;
    original_count = catalog.count;

    for (size_t i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        if (campaign_catalog_load(fixtures[i], &catalog) == 0 ||
            expect_int("NUL campaign load preserves catalog",
                       catalog.levels == original_levels, 1) != 0 ||
            expect_int("NUL campaign load preserves count",
                       (int)catalog.count, (int)original_count) != 0) {
            campaign_catalog_cleanup(&catalog);
            return 1;
        }
    }

    campaign_catalog_cleanup(&catalog);
    return 0;
}

static int direct_game_boot_repairs_input_and_keeps_controller_runtime(void)
{
    AppSessionConfig config = {0};
    AppSession *session;

    game_input_test_set_physical_state(PLAYER_INPUT_LEFT, 0);
    config.level_path = "levels/00_sandbox_01.toml";
    config.smoke_test_frames = 0;
    session = session_create(&config);
    if (!session) {
        game_input_test_clear_physical_state();
        fprintf(stderr, "session_test: direct game session_create failed\n");
        return 1;
    }
    if (expect_int("direct boot opens game", session->screen, APP_SCREEN_GAME) != 0 ||
        expect_int("direct boot bypasses campaign", session->catalog_loaded, 0) != 0 ||
        expect_int("direct boot repairs web input", session->web_input_repair_count, 1) != 0 ||
        expect_int("direct boot owns window", IsWindowReady(), 1) != 0 ||
        expect_int("direct boot input ready", input_ready(), 1) != 0 ||
        expect_int("direct boot latch armed", session->game->input_release_latched, 1) != 0 ||
        expect_int("direct boot held input gated", game_input_sample(session->game), 0) != 0)
        goto fail;

    session_frame(session);
    if (expect_int("direct boot presents once", session->game_presented_count, 1) != 0 ||
        expect_int("direct boot remains in game",
                   session->screen, APP_SCREEN_GAME) != 0)
        goto fail;

    game_input_test_set_physical_state(0, 0);
    if (expect_int("direct boot release opens gate",
                   game_input_sample(session->game), 0) != 0 ||
        expect_int("direct boot latch cleared",
                   session->game->input_release_latched, 0) != 0)
        goto fail;

    session_destroy(&session);
    game_input_test_clear_physical_state();
    if (expect_int("direct boot window cleanup", IsWindowReady(), 0) != 0 ||
        expect_int("direct boot input cleanup", input_ready(), 0) != 0)
        return 1;
    return 0;

fail:
    game_input_test_clear_physical_state();
    session_destroy(&session);
    return 1;
}

static int failed_initial_level_does_not_create_session(void)
{
    AppSessionConfig config = {0};
    AppSession *session;

    config.level_path = "levels/does-not-exist.toml";
    session = session_create(&config);
    if (session) {
        session_destroy(&session);
        fprintf(stderr, "session_test: failed initial level created a session\n");
        return 1;
    }
    return 0;
}

static int immediate_play_preserves_window_and_input_latch(void)
{
    AppSessionConfig config = {0};
    AppSession *session = session_create(&config);
    if (!session) return 1;
    Texture2D window = context_probe_open();
    if (!context_probe_alive(window)) { fprintf(stderr, "initial context probe failed\n"); goto fail; }
    session->menu->confirm_release_required = 0;
    game_input_test_set_physical_state(GAME_INPUT_CONFIRM, 0);
    if (push_confirm()) goto fail;
    session_frame(session);
    if (expect_int("immediate Play opens game", session->screen, APP_SCREEN_GAME) ||
        expect_int("immediate Play keeps context", context_probe_alive(window), 1) ||
        expect_int("immediate Play has no menu present", session->menu_presented_count, 0) ||
        expect_int("immediate Play constructs one game", session->game_open_count, 1) ||
        expect_int("held confirmation stays gated", session->game->input_release_latched, 1)) goto fail;
    session_frame(session);
    if (expect_int("game first frame presents", session->game_presented_count, 1)) goto fail;
    UnloadTexture(window);
    session_destroy(&session);
    game_input_test_clear_physical_state();
    return 0;
fail:
    UnloadTexture(window);
    session_destroy(&session);
    game_input_test_clear_physical_state();
    return 1;
}

static int repeated_menu_game_ownership(void)
{
    AppSessionConfig config = {0};
    config.smoke_test_frames = 0;
    AppSession *session = session_create(&config);

    if (!session) {
        fprintf(stderr, "session_test: session_create menu failed\n");
        return 1;
    }
    Texture2D window = context_probe_open();
    if (expect_int("starts in menu", session->screen, APP_SCREEN_MENU) != 0 ||
        expect_int("default menu owns campaign", session->catalog_loaded, 1) != 0 ||
        expect_int("menu consumes campaign", session->menu->catalog == &session->catalog, 1) != 0 ||
        expect_int("menu campaign count", (int)session->catalog.count, 3) != 0 ||
        expect_int("menu window ready", IsWindowReady(), 1) != 0)
        return 1;
    session_frame(session);
    if (expect_int("menu presents once", session->menu_presented_count, 1) != 0)
        return 1;
    if (push_controller_button(PAD_RIGHT) != 0) return 1;
    session_frame(session);
    if (expect_int("menu D-pad selects next level",
                   strcmp(session->menu->selected_level_path,
                            session->catalog.levels[1].path) == 0, 1) != 0)
        return 1;
    if (push_controller_button(PAD_A) != 0) return 1;
    session_frame(session);
    if (expect_int("menu opens game", session->screen, APP_SCREEN_GAME) != 0) return 1;
    if (expect_int("menu closes once", session->menu_close_count, 1) != 0) return 1;
    if (expect_int("menu-to-game retains context", context_probe_alive(window), 1) != 0 ||
        expect_int("menu-to-game repairs web input", session->web_input_repair_count, 1) != 0)
        return 1;

    session->game->completion.complete = 1;
    session->game->completion.pending_next_phase = 0;
    session->game->terminal_action_index = 0;
    if (!game_web_input_touch(GAME_TOUCH_RIGHT, 1)) return 1;
    if (push_key(KEY_DOWN) != 0 || push_confirm() != 0) return 1;
    session_frame(session);
    if (expect_int("level select opens menu", session->screen, APP_SCREEN_MENU) != 0) return 1;
    if (expect_int("level select clears touch holds", game_web_input_take_touch_mask(), 0) != 0) return 1;
    if (expect_int("game closes once", session->game_close_count, 1) != 0) return 1;
    if (expect_int("level-select retains context", context_probe_alive(window), 1) != 0)
        return 1;

    {
        StartMenu *failed_menu = session->menu;
        int selected = failed_menu->selected_level;
        strncpy(failed_menu->selected_level_path, "levels/missing-menu-level.toml",
                sizeof(failed_menu->selected_level_path) - 1);
        failed_menu->selected_level_path[sizeof(failed_menu->selected_level_path) - 1] = '\0';
        failed_menu->route = MENU_ROUTE_PLAY;
        session_frame(session);
        if (expect_int("menu load failure keeps menu", session->screen, APP_SCREEN_MENU) != 0 ||
            expect_int("menu load failure keeps selection",
                       session->menu->selected_level, selected) != 0 ||
            expect_int("menu load failure keeps menu ownership",
                       session->menu == failed_menu, 1) != 0 ||
            expect_int("menu load failure reports error",
                       session->menu->error_message[0] != '\0', 1) != 0)
            return 1;
        strncpy(failed_menu->selected_level_path,
                session->catalog.levels[selected].path,
                sizeof(failed_menu->selected_level_path) - 1);
        failed_menu->selected_level_path[sizeof(failed_menu->selected_level_path) - 1] = '\0';
        start_menu_set_error(failed_menu, NULL);
    }

    game_input_test_set_physical_state(PLAYER_INPUT_LEFT, 0);
    session->menu->route = MENU_ROUTE_PLAY;
    session_frame(session);
    if (expect_int("second menu opens game", session->screen, APP_SCREEN_GAME) != 0) return 1;
    if (expect_int("menu closes twice", session->menu_close_count, 2) != 0) return 1;
    if (expect_int("second game repairs web input", session->web_input_repair_count, 2) != 0 ||
        expect_int("second game retains context", context_probe_alive(window), 1) != 0)
        return 1;
    if (expect_int("held menu direction gated", game_input_sample(session->game), 0) != 0)
        return 1;
    game_input_test_set_physical_state(0, 0);
    (void)game_input_sample(session->game);

    session->game->completion.complete = 1;
    session->game->completion.pending_next_phase = 0;
    session->game->terminal_action_index = 0;
    if (push_confirm() != 0) return 1;
    session_frame(session);
    if (expect_int("replay stays in game", session->screen, APP_SCREEN_GAME) != 0) return 1;
    if (expect_int("replay closes old game", session->game_close_count, 2) != 0) return 1;
    if (expect_int("replay repairs web input", session->web_input_repair_count, 3) != 0 ||
        expect_int("replay retains context", context_probe_alive(window), 1) != 0)
        return 1;

    session->game->completion.complete = 1;
    session->game->completion.pending_next_phase = 1;
    session->game->loop.prev_ticks = 0;
    session->game->terminal_action_index = 0;
    if (push_confirm() != 0) return 1;
    session_frame(session);
    if (expect_int("next level success keeps game", session->screen, APP_SCREEN_GAME) != 0) return 1;
    if (session->game->completion.complete != 0) {
        fprintf(stderr, "session_test: successful next level kept completion\n");
        return 1;
    }
    if (expect_int("next level timing reset", session->game->loop.prev_ticks != 0, 1) != 0)
        return 1;

    {
        LevelDef *def = (LevelDef *)session->game->runtime.current_level;
        strncpy(def->next_phase, "levels/does-not-exist.toml",
                sizeof(def->next_phase) - 1);
        def->next_phase[sizeof(def->next_phase) - 1] = '\0';
    }
    session->game->completion.complete = 1;
    session->game->completion.pending_next_phase = 1;
    session->game->terminal_action_index = 0;
    if (push_confirm() != 0) return 1;
    session_frame(session);
    if (expect_int("next failure keeps game", session->screen, APP_SCREEN_GAME) != 0) return 1;
    if (expect_int("next failure keeps overlay", session->game->completion.complete, 1) != 0) return 1;
    if (expect_int("next failure clears request", session->game->route, GAME_ROUTE_NONE) != 0) return 1;

    UnloadTexture(window);
    if (push_key(KEY_ESCAPE) != 0) return 1;
    session_frame(session);
    if (expect_int("exit ends session", session->screen, APP_SCREEN_ENDED) != 0) return 1;
    if (expect_int("runtime cleanup once", session->runtime_cleanup_count, 1) != 0) return 1;
    if (expect_int("window closed on exit", IsWindowReady(), 0) != 0 ||
        expect_int("input closed on exit", input_ready(), 0) != 0 ||
        expect_int("audio closed on exit", IsAudioDeviceReady(), 0) != 0)
        return 1;
    session_destroy(&session);
    if (session != NULL) {
        fprintf(stderr, "session_test: destroy did not null owner\n");
        return 1;
    }
    return 0;
}

static void disable_integration_dynamic_collisions(GameState *game)
{
    game->platform_count = 0;
    game->spider_count = 0;
    game->jumping_spider_count = 0;
    game->bird_count = 0;
    game->faster_bird_count = 0;
    game->fish_count = 0;
    game->faster_fish_count = 0;
    game->coin_count = 0;
    game->star_yellow_count = 0;
    game->star_green_count = 0;
    game->star_red_count = 0;
    game->axe_trap_count = 0;
    game->circular_saw_count = 0;
    game->spike_row_count = 0;
    game->spike_platform_count = 0;
    game->spike_block_count = 0;
    game->blue_flame_count = 0;
    game->fire_flame_count = 0;
    game->float_platform_count = 0;
    game->bridge_count = 0;
    game->bouncepad_small_count = 0;
    game->bouncepad_medium_count = 0;
    game->bouncepad_high_count = 0;
    game->floor_gap_count = 0;
    game->last_star.active = 0;
}

static int checkpoint_transitions_use_production_paths(void)
{
    AppSessionConfig config = {0};
    AppSession *session = NULL;
    GameState *game;
    LevelDef *def;
    float initial_x;
    float initial_y;
    float expected_next_x;
    float expected_next_y;
    char expected_next_path[256] = {0};

    config.level_path = "levels/00_sandbox_01.toml";
    session = session_create(&config);
    if (!session || !session->game) {
        fprintf(stderr, "session_test: checkpoint integration session_create failed\n");
        session_destroy(&session);
        return 1;
    }

    game = session->game;
    def = (LevelDef *)game->runtime.current_level;
    initial_x = game->respawn_x;
    initial_y = game->respawn_y;

    /* Real active update: movement crosses CP, then real gap damage kills. */
    disable_integration_dynamic_collisions(game);
    def->checkpoint_count = 2;
    def->checkpoints[0].x = 120.0f;
    def->checkpoints[0].y = 96.0f;
    def->checkpoints[1].x = 125.0f;
    def->checkpoints[1].y = 88.0f;
    game->floor_gap_count = 1;
    game->floor_gaps[0] = 130;
    game->player.x = 130.0f;
    game->player.y = 270.0f;
    game->player.vx = 0.0f;
    game->player.vy = 0.0f;
    game->player.on_ground = 0;
    game_update_active(game, 0.0f, 0);
    if (expect_int("same-frame life loss preserves checkpoint",
                   game->checkpoint_index, 1) != 0 ||
        expect_float("same-frame checkpoint x", game->respawn_x, 125.0f) != 0 ||
        expect_float("same-frame checkpoint y", game->respawn_y, 88.0f) != 0 ||
        expect_int("same-frame life loss decrements lives", game->lives, 2) != 0 ||
        expect_float("same-frame player respawn x", game->player.spawn_x, 125.0f) != 0 ||
        expect_float("same-frame player respawn y", game->player.spawn_y, 88.0f) != 0)
        goto fail;

    /* Real damage path reaches game-over; real session event path retries. */
    game->floor_gap_count = 0;
    game->hearts = 1;
    game->lives = 0;
    apply_damage(game, 1, 0, 0.0f, 0.0f);
    if (expect_int("game-over damage sets overlay", game->game_over, 1) != 0 ||
        expect_int("game-over retains checkpoint", game->checkpoint_index, 1) != 0)
        goto fail;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    game = session->game;
    if (!game || expect_int("retry clears game-over", game->game_over, 0) != 0 ||
        expect_int("retry resets checkpoint", game->checkpoint_index, -1) != 0 ||
        expect_float("retry resets initial x", game->respawn_x, initial_x) != 0 ||
        expect_float("retry resets initial y", game->respawn_y, initial_y) != 0)
        goto fail;

    /* Native Replay closes/reopens through AppSession, so TOML start wins. */
    def = (LevelDef *)game->runtime.current_level;
    def->checkpoint_count = 1;
    def->checkpoints[0].x = 220.0f;
    def->checkpoints[0].y = 100.0f;
    game->checkpoint_index = 0;
    game->respawn_x = 220.0f;
    game->respawn_y = 100.0f;
    game->completion.complete = 1;
    game->completion.pending_next_phase = 0;
    game->terminal_action_index = 0;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    game = session->game;
    if (!game || expect_int("replay keeps game screen", session->screen,
                            APP_SCREEN_GAME) != 0 ||
        expect_int("replay resets checkpoint", game->checkpoint_index, -1) != 0 ||
        expect_float("replay resets initial x", game->respawn_x, initial_x) != 0 ||
        expect_float("replay resets initial y", game->respawn_y, initial_y) != 0)
        goto fail;

    /* Next Level loads real phase data and must not carry old CP progress. */
    def = (LevelDef *)game->runtime.current_level;
    strncpy(expected_next_path, def->next_phase, sizeof(expected_next_path) - 1);
    def->checkpoint_count = 1;
    def->checkpoints[0].x = 220.0f;
    def->checkpoints[0].y = 100.0f;
    game->checkpoint_index = 0;
    game->respawn_x = 220.0f;
    game->respawn_y = 100.0f;
    game->completion.complete = 1;
    game->completion.pending_next_phase = 1;
    game->terminal_action_index = 0;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    game = session->game;
    if (!game || expect_int("next phase keeps game screen", session->screen,
                            APP_SCREEN_GAME) != 0 ||
         expect_int("next phase resets checkpoint", game->checkpoint_index, -1) != 0 ||
         expect_int("next phase completion clears", game->completion.complete, 0) != 0 ||
         expect_int("next phase path advances",
                    strcmp(game->level_path, expected_next_path) == 0, 1) != 0)
        goto fail;

    def = (LevelDef *)game->runtime.current_level;
    level_effective_spawn(def, &expected_next_x, &expected_next_y);
    if (expect_float("next phase start x", game->respawn_x, expected_next_x) != 0 ||
        expect_float("next phase start y", game->respawn_y, expected_next_y) != 0)
        goto fail;

    /* A failed load must leave active phase and resolved checkpoint untouched. */
    def = (LevelDef *)game->runtime.current_level;
    def->checkpoint_count = 1;
    def->checkpoints[0].x = 400.0f;
    def->checkpoints[0].y = 112.0f;
    strncpy(def->next_phase, "levels/does-not-exist.toml",
            sizeof(def->next_phase) - 1);
    def->next_phase[sizeof(def->next_phase) - 1] = '\0';
    game->checkpoint_index = 0;
    game->respawn_x = 400.0f;
    game->respawn_y = 112.0f;
    game->completion.complete = 1;
    game->completion.pending_next_phase = 1;
    game->terminal_action_index = 0;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    game = session->game;
    if (!game || expect_int("failed phase keeps game screen", session->screen,
                            APP_SCREEN_GAME) != 0 ||
        expect_int("failed phase keeps completion", game->completion.complete, 1) != 0 ||
        expect_int("failed phase keeps checkpoint", game->checkpoint_index, 0) != 0 ||
        expect_float("failed phase keeps checkpoint x", game->respawn_x, 400.0f) != 0 ||
        expect_float("failed phase keeps checkpoint y", game->respawn_y, 112.0f) != 0)
        goto fail;

    session_destroy(&session);
    return 0;

fail:
    session_destroy(&session);
    return 1;
}

typedef struct {
    int store_result;
    int store_calls;
    int reload_calls;
    AppSessionLifecycleEvent events[16];
    int event_count;
    char reload_path[256];
} LifecycleProbe;

static int probe_store_replay(const char *path, void *userdata)
{
    LifecycleProbe *probe = userdata;
    (void)path;
    probe->store_calls++;
    return probe->store_result;
}

static void probe_reload(const char *path, void *userdata)
{
    LifecycleProbe *probe = userdata;
    probe->reload_calls++;
    strncpy(probe->reload_path, path, sizeof(probe->reload_path) - 1);
    probe->reload_path[sizeof(probe->reload_path) - 1] = '\0';
    if (probe->event_count < (int)(sizeof(probe->events) / sizeof(probe->events[0])))
        probe->events[probe->event_count++] = APP_SESSION_EVENT_RELOAD_REQUESTED;
}

static void probe_lifecycle(AppSessionLifecycleEvent event, const char *path,
                            void *userdata)
{
    LifecycleProbe *probe = userdata;
    (void)path;
    if (probe->event_count < (int)(sizeof(probe->events) / sizeof(probe->events[0])))
        probe->events[probe->event_count++] = event;
}

static int replay_storage_failure_retains_session_and_success_orders_cleanup(void)
{
    LifecycleProbe probe = {0};
    AppSessionHooks hooks = {
        probe_store_replay,
        probe_reload,
        probe_lifecycle,
        &probe,
        1
    };
    AppSessionConfig config = {0};
    AppSession *session;
    static const AppSessionLifecycleEvent expected[] = {
        APP_SESSION_EVENT_WEB_INPUT_REPAIRED,
        APP_SESSION_EVENT_CALLBACK_REGISTERED,
        APP_SESSION_EVENT_CALLBACK_CANCELLED,
        APP_SESSION_EVENT_GAME_CLOSED,
        APP_SESSION_EVENT_RUNTIME_CLEANED,
        APP_SESSION_EVENT_SESSION_FREED,
        APP_SESSION_EVENT_RELOAD_REQUESTED
    };

    config.level_path = "levels/00_sandbox_01.toml";
    config.smoke_test_frames = 0;
    config.hooks = &hooks;
    session = session_create(&config);
    if (!session) return 1;

    if (expect_int("callback registers once", session_run(session), EXIT_SUCCESS) != 0 ||
        expect_int("callback second registration is ignored",
                   session_run(session), EXIT_SUCCESS) != 0 ||
        expect_int("callback registration count", session->callback_registration_count, 1) != 0)
        goto fail;

    probe.store_result = 0;
    session->game->completion.complete = 1;
    session->game->route = GAME_ROUTE_REPLAY;
    session_frame(session);
    if (expect_int("storage failure keeps game screen", session->screen, APP_SCREEN_GAME) != 0 ||
        expect_int("storage failure keeps completion", session->game->completion.complete, 1) != 0 ||
        expect_int("storage failure leaves callback", session->callback_cancelled, 0) != 0 ||
        expect_int("storage failure leaves cleanup", session->runtime_cleanup_count, 0) != 0 ||
        expect_int("storage failure counted", session->replay_storage_attempts, 1) != 0 ||
        expect_int("storage failure not successful", session->replay_storage_successes, 0) != 0 ||
        expect_int("storage failure status", strcmp(session->status_message,
                                                    "Replay unavailable: storage failed") == 0, 1) != 0 ||
        expect_int("storage failure emits no teardown", probe.event_count, 2) != 0)
        goto fail;

    probe.store_result = 1;
    session->game->route = GAME_ROUTE_REPLAY;
    session_frame(session);
    if (expect_int("replay storage called twice", probe.store_calls, 2) != 0 ||
        expect_int("reload called once after free", probe.reload_calls, 1) != 0 ||
        expect_int("lifecycle ordering length", probe.event_count,
                   (int)(sizeof(expected) / sizeof(expected[0]))) != 0)
        return 1;
    for (int i = 0; i < (int)(sizeof(expected) / sizeof(expected[0])); i++) {
        if (expect_int("lifecycle ordering", probe.events[i], expected[i]) != 0)
            return 1;
    }
    if (expect_int("reload path", strcmp(probe.reload_path,
                                         "levels/00_sandbox_01.toml") == 0, 1) != 0)
        return 1;
    return 0;

fail:
    session_destroy(&session);
    return 1;
}

static int pending_profile_keeps_exit_alive(void)
{
    AppSessionConfig config = {.level_path = "tests/fixtures/runtime/transition.toml"};
    AppSession *session = session_create(&config);
    if (!session) return 1;
    session->profile.pending_text = malloc(PROFILE_TEXT_MAX);
    if (!session->profile.pending_text ||
        game_profile_encode(&session->profile.data, session->profile.pending_text, PROFILE_TEXT_MAX)) {
        session_destroy(&session);
        return 1;
    }
    session->profile.pending_revision = session->profile.revision;
    session->attempted_save_revision = session->profile.revision;
    session->game->route = GAME_ROUTE_EXIT;
    session->game->loop.prev_ticks = clock_millis() - 100;
    float elapsed = session->game->completion.level_elapsed;
    session_frame(session);
    if (expect_int("pending save retains session", session->ended, 0) ||
        expect_int("pending save retains game", session->game_close_count, 0) ||
        expect_float("pending exit freezes gameplay", session->game->completion.level_elapsed, elapsed)) {
        session_destroy(&session);
        return 1;
    }
    game_profile_finish_save(&session->profile, PROFILE_SAVE_OK);
    session_frame(session);
    int result = expect_int("committed exit completes", session->ended, 1) ||
                 expect_int("committed exit closes once", session->game_close_count, 1);
    session_destroy(&session);
    return result;
}

static int native_replay_keeps_session_ownership(void)
{
    LifecycleProbe probe = {.store_result = 1};
    AppSessionHooks hooks = {.store_replay = probe_store_replay,
                            .reload = probe_reload, .userdata = &probe};
    AppSessionConfig config = {.level_path = "tests/fixtures/runtime/transition.toml",
                               .smoke_test_frames = 1, .hooks = &hooks};
    AppSession *session = session_create(&config);
    if (!session) return 1;
    session->game->route = GAME_ROUTE_REPLAY;
    int result = session_run(session) != EXIT_SUCCESS ||
                 expect_int("native replay does not call browser hook", probe.store_calls, 0) ||
                 expect_int("native replay reopens game", session->game_open_count, 2);
    session_destroy(&session);
    return result;
}

static int menu_mouse_and_path_boundaries(void)
{
    CampaignCatalog catalog = {0};
    if (campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &catalog)) return 1;
    StartMenu *menu = start_menu_create(&catalog);
    if (!menu) { campaign_catalog_cleanup(&catalog); return 1; }
    input_clear();
    Vector2 logical = input_pointer_to_logical((Vector2){400,368});
    if (expect_float("physical pointer maps x once", logical.x, 200) ||
        expect_float("physical pointer maps y once", logical.y, 184)) return 1;
    InputEvent event = {.type=INPUT_MOUSE_DOWN,.button=MOUSE_BUTTON_LEFT,
                        .x=(int)logical.x,.y=(int)logical.y};
    input_push(&event);
    start_menu_frame(menu);
    int result = expect_int("physical Play click", menu->route, MENU_ROUTE_PLAY);
    start_menu_close(&menu);
    campaign_catalog_cleanup(&catalog);
    char short_path[8];
    if (expect_int("reject resolved path truncation",
                   level_resolve_path("tests/fixtures/runtime/transition.toml", short_path, sizeof(short_path)), -1)) result = 1;
    return result;
}

static int collision_lifetime_and_pickups(void)
{
    for (int mode = 0; mode < 3; mode++) {
        GameState gs = {0};
        LevelDef def;
        level_def_init_defaults(&def);
        def.player_start_x = 20;
        def.player_start_y = FLOOR_Y;
        def.spider_count = 1;
        def.spiders[0] = (SpiderPlacement){100, 0, 100, 200, 0};
        def.coin_count = 1;
        def.coins[0] = (CoinPlacement){115, 239};
        def.last_star = (LastStarPlacement){110, 236};
        gs.player.w = gs.player.h = 48;
        if (level_load(&gs, &def)) return 1;
        gs.player.x = 100;
        gs.player.y = 220;
        gs.player.hurt_timer = mode == 0 ? 1.0f : 0.0f;
        gs.hearts = mode == 0 ? 3 : 1;
        gs.lives = mode == 2 ? 0 : 1;
        game_collide(&gs, 1.0f / TARGET_FPS);
        if (expect_int("immunity pickups/death no stale goal", gs.completion.complete, mode == 0) ||
            expect_int("coin remains after death", gs.coins[0].active, mode != 0) ||
            expect_int("game over only on final life", gs.game_over, mode == 2)) return 1;
        if (mode == 1 && expect_float("respawn before next pass", gs.player.x, 20)) return 1;
    }
    return 0;
}

static int nearest_surface_is_order_independent(void)
{
    GameState timing = {0};
    timing.smoke_test_frames = 5;
    timing.loop.prev_ticks = clock_millis();
    float first = game_timing_step(&timing, NULL);
    clock_wait(20);
    float delayed = game_timing_step(&timing, NULL);
    if (expect_float("smoke fixed step", first, 1.0f / TARGET_FPS) ||
        expect_float("wall time does not change replay physics", delayed, first)) return 1;
    for (int order = 0; order < 2; order++) {
        Platform platforms[2] = {{.x=0,.y=order ? 100 : 120,.w=100},
                                 {.x=0,.y=order ? 120 : 100,.w=100}};
        FloatPlatform floating = {.x=0,.y=95,.w=100,.active=1};
        Player player = {.x=10,.y=98,.w=48,.h=48,.vy=100};
        int landed;
        player_resolve_platform_collisions(&player, platforms, 2, NULL, 0, 90, &landed, -1);
        if (expect_float("nearest static", player.y+player.h-16, 100)) return 1;
        player_resolve_platform_collisions(&player, NULL, 0, &floating, 1, 90, &landed, -1);
        if (expect_float("nearer float replaces static", player.y+player.h-16, 95) ||
            expect_int("float support", landed, 0)) return 1;
        SpikePlatform ceilings[2] = {{.x=0,.y=order ? 100 : 120,.w=100,.active=1},
                                     {.x=0,.y=order ? 120 : 100,.w=100,.active=1}};
        player.y = 70;
        player.vy = -100;
        player.on_ground = 0;
        player_resolve_spike_platform_ceiling_collision(&player, ceilings, 2, 160);
        if (expect_float("nearest ceiling", player.y + PLAYER_PHYS_PAD_TOP, 120 + SPIKE_PLAT_SRC_H)) return 1;
    }
    Platform pillar = {.x=0,.y=172,.w=100};
    FloatPlatform floating = {.x=0,.y=200,.w=100,.active=1};
    Bridge bridge = {.x=0,.base_y=160,.brick_count=8};
    for (int i = 0; i < 8; i++) bridge.bricks[i].active = 1;
    SpikePlatform spike = {.x=0,.y=150,.w=100,.active=1};
    Player falling = {.x=10,.y=100,.w=48,.h=48,.vy=1500};
    player_apply_default_physics(&falling);
    int bounce, support;
    player_update(&falling, 0.1f, NULL, &pillar, 1, &floating, 1,
                  NULL, 0, NULL, 0, NULL, 0, NULL, 0, &bridge, 1,
                  &spike, 1, NULL, 0, &bounce, &support, -1, 400);
    if (expect_float("mixed surfaces beat lower floor", falling.y + falling.h - PLAYER_FLOOR_SINK, 150) ||
        expect_int("discarded float is not ridden", support, -1)) return 1;
    return 0;
}

static int phase_resets_transient_state(void)
{
    GameState gs = {0};
    strcpy(gs.level_path, "tests/fixtures/runtime/transition.toml");
    if (game_init(&gs)) return 1;
    gs.player.vx = 123;
    gs.player.vy = -222;
    gs.player.on_vine = 1;
    gs.player.vine_index = 7;
    gs.loop.fp_prev_riding = 3;
    gs.score = 1200;
    gs.score_life_next = 2000;
    gs.lives = 4;
    game_complete_level(&gs);
    int result = game_load_next_phase(&gs) != 0 ||
        expect_float("phase vx cleared", gs.player.vx, 0) ||
        expect_float("phase vy cleared", gs.player.vy, 0) ||
        expect_int("phase climb cleared", gs.player.on_vine, 0) ||
        expect_int("phase support cleared", gs.loop.fp_prev_riding, -1) ||
        expect_int("campaign score retained", gs.score, 1200) ||
        expect_int("campaign lives retained", gs.lives, 4);
    LevelDef *active = gs.level_def;
    active->music_volume = 0;
    level_resources_apply(&gs, active);
    if (gs.audio.music && expect_float("zero volume stays muted", test_last_music_volume, 0)) result = 1;
    game_cleanup(&gs);
    return result;
}

int game_profile_contract_test(void);
int game_simulation_contract_test(void);
int audio_contract_test(void);
int web_frame_pacing_contract_test(void);

static int setup_raylib(void)
{
    if (display_open(WINDOW_W, WINDOW_H, "session regression", 1)) return 1;
    input_open(GAME_W, GAME_H);
    game_input_test_set_physical_state(0, 0);
    return audio_open() != 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const struct { const char *name; int (*run)(void); } cases[] = {
#define CASE(fn) {#fn, fn}
        CASE(audio_contract_test),
        CASE(web_frame_pacing_contract_test),
        CASE(game_simulation_contract_test), CASE(game_profile_contract_test),
        CASE(pending_profile_keeps_exit_alive), CASE(native_replay_keeps_session_ownership),
        CASE(menu_mouse_and_path_boundaries), CASE(collision_lifetime_and_pickups),
        CASE(nearest_surface_is_order_independent), CASE(phase_resets_transient_state),
        CASE(campaign_manifest_is_ordered_and_transactional),
        CASE(campaign_manifest_nul_fixtures_reject_transactionally),
        CASE(physical_release_latch_blocks_transition_input),
        CASE(failed_initial_level_does_not_create_session),
        CASE(direct_game_boot_repairs_input_and_keeps_controller_runtime),
        CASE(immediate_play_preserves_window_and_input_latch),
        CASE(repeated_menu_game_ownership), CASE(checkpoint_transitions_use_production_paths),
        CASE(replay_storage_failure_retains_session_and_success_orders_cleanup)
#undef CASE
    };
    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (setup_raylib()) return 1;
        int result = cases[i].run();
        printf("session: %s %s\n", cases[i].name, result ? "FAIL" : "PASS");
        failures += result != 0;
        game_input_test_clear_physical_state();
        input_close(); audio_close();
        if (IsWindowReady()) CloseWindow();
    }
    printf("session_test: %d failing cases\n", failures);
    return failures ? 1 : 0;
}
