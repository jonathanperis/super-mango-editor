#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L  /* chdir under -std=c11 */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>   /* _chdir */
#else
#include <unistd.h>   /* chdir */
#endif

#include "input/input_backend.h"
#include "shared/asset_root.h"
#include "shared/platform.h"  /* clock_wait */
#include "shared/serializer_io.h"
#include "test_paths.h"

#include "collision/collision_damage.h"
#include "collision/game_collision.h"
#include "core/app_session.h"
#include "core/game_completion.h"
#include "core/game_experiment.h"
#include "core/game_overlay.h"
#include "core/game_player_step.h"
#include "core/game_terminal.h"
#include "core/game_timing.h"
#include "core/game_update.h"
#include "input/game_input.h"
#include "input/game_web_input.h"
#include "levels/level.h"
#include "levels/level_session.h"
#include "levels/level_loader.h"
#include "levels/level_path.h"
#include "levels/level_resources.h"
#include "levels/level_start.h"  /* level_ground_top_at */
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
        expect_int("Space latch armed", gs.screen.input_release_latched, 1) != 0)
        goto fail;
    player_handle_input(&player, NULL, 0, sampled,
                        NULL, 0, NULL, 0, NULL, 0);
    if (expect_int("held Space cannot jump", player.jump_held, 0) != 0)
        goto fail;

    game_input_test_set_physical_state(0, 0);
    if (expect_int("Space release clears gate", game_input_sample(&gs), 0) != 0 ||
        expect_int("Space latch cleared", gs.screen.input_release_latched, 0) != 0)
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

/*
 * One missing or out-of-order level file used to fail the whole catalog, so
 * the game could not even open its menu. Now only that entry is unavailable:
 * it stays listed with a reason, the menu starts on a playable entry, shows
 * the broken one disabled and refuses to start it.
 */
static int campaign_broken_level_disables_only_its_entry(void)
{
    CampaignCatalog catalog = {0};
    StartMenu *menu = NULL;
    int failed = 1;

    if (campaign_catalog_load("tests/fixtures/campaign_manifest/partial_missing_level.toml",
                              &catalog) != 0) {
        fprintf(stderr, "session_test: partial campaign rejected as a whole\n");
        goto done;
    }
    if (expect_int("partial count", (int)catalog.count, 4) ||
        expect_int("missing entry unavailable", catalog.levels[0].available, 0) ||
        expect_int("missing entry reason",
                   strcmp(catalog.levels[0].problem, "level file not found"), 0) ||
        expect_int("missing entry named by file",
                   strcmp(catalog.levels[0].display_name, "zz_missing_campaign_level"), 0) ||
        expect_int("sandbox playable", catalog.levels[1].available, 1) ||
        expect_int("lugio 1 playable", catalog.levels[2].available, 1) ||
        expect_int("lugio 2 playable", catalog.levels[3].available, 1) ||
        expect_int("first playable", campaign_first_available(&catalog), 1))
        goto done;

    menu = start_menu_create(&catalog);
    if (!menu) goto done;
    menu->confirm_release_required = 0;
    if (expect_int("menu starts on a playable level", menu->selected_level, 1)) goto done;
    input_clear();
    if (push_key(KEY_LEFT) || push_confirm()) goto done;
    if (expect_int("disabled entry still renders", start_menu_frame(menu), 1) ||
        expect_int("broken entry selectable", menu->selected_level, 0) ||
        expect_int("broken entry cannot start", menu->route, MENU_ROUTE_NONE))
        goto done;
    if (push_key(KEY_RIGHT) || push_confirm()) goto done;
    start_menu_frame(menu);
    if (expect_int("playable entry starts", menu->route, MENU_ROUTE_PLAY)) goto done;
    start_menu_close(&menu);

    /* A level that does not lead to the next entry is unavailable too. */
    if (campaign_catalog_load("tests/fixtures/campaign_manifest/partial_broken_chain.toml",
                              &catalog) != 0 ||
        expect_int("chain breaker unavailable", catalog.levels[0].available, 0) ||
        expect_int("chain reason",
                   strcmp(catalog.levels[0].problem, "next_phase is out of campaign order"), 0) ||
        expect_int("final level playable", catalog.levels[1].available, 1))
        goto done;

    /* Nothing playable is still a failed catalog, and keeps the old one. */
    CampaignLevel *kept = catalog.levels;
    if (expect_int("no playable level fails",
                   campaign_catalog_load("tests/fixtures/campaign_manifest/no_playable_level.toml",
                                         &catalog), -1) ||
        expect_int("failed load keeps catalog", catalog.levels == kept, 1))
        goto done;
    failed = 0;
done:
    start_menu_close(&menu);
    campaign_catalog_cleanup(&catalog);
    return failed;
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
        expect_int("direct boot latch armed", session->game->screen.input_release_latched, 1) != 0 ||
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
                   session->game->screen.input_release_latched, 0) != 0)
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

/* Change the working folder for a test without going through the helper
 * under test. Windows spells chdir with a leading underscore. */
static int test_change_directory(const char *folder)
{
#ifdef _WIN32
    return _chdir(folder);
#else
    return chdir(folder);
#endif
}

/* 1 when path is prefix followed by rest, treating '/' and '\\' as the same
 * separator (Windows mixes them; POSIX paths only ever use '/'). */
static int same_path_spelling_prefix(const char *path, const char *prefix,
                                     const char *rest)
{
    size_t prefix_len = strlen(prefix);
    size_t rest_len = strlen(rest);

    if (strlen(path) != prefix_len + rest_len) return 0;
    for (size_t i = 0; i < prefix_len + rest_len; i++) {
        char want = i < prefix_len ? prefix[i] : rest[i - prefix_len];
        char got = path[i];
        if (want == '\\') want = '/';
        if (got == '\\') got = '/';
        if (want != got) return 0;
    }
    return 1;
}

/*
 * Started from another folder, the game and editor used to find no assets:
 * every path is relative and nothing moved the working folder. The helper
 * must move it to the folder that holds assets/ and levels/, and turn typed
 * paths into absolute ones first so they keep their meaning.
 */
static int asset_root_moves_a_foreign_working_folder(void)
{
    int failed = 1;
    char *home = asset_root_absolute(".");
    char *scratch = asset_root_absolute(TEST_OUT);
    char *typed = NULL;
    char start[4096], missing[4096];
    if (!home || !scratch) goto done;
    snprintf(start, sizeof(start), "%s/levels/campaigns/", home);
    snprintf(missing, sizeof(missing), "%sno/such/folder/", scratch);

    if (expect_int("repository root is an asset root", asset_root_contains(""), 1) ||
        expect_int("absolute path", asset_root_path_is_absolute("/tmp/x.toml"), 1) ||
        expect_int("drive path", asset_root_path_is_absolute("C:\\levels\\x.toml"), 1) ||
        expect_int("relative path", asset_root_path_is_absolute("levels/x.toml"), 0))
        goto done;

    /* Start the "game" from the scratch folder, as a shortcut would. */
    if (test_change_directory(scratch) != 0) goto done;
    typed = asset_root_absolute("mine.toml");
    /* Windows reports the working folder with backslashes while scratch was
     * joined with '/', so compare with one separator spelling. */
    if (expect_int("scratch folder holds no assets", asset_root_contains(""), 0) ||
        expect_int("typed path keeps the launch folder",
                   typed && same_path_spelling_prefix(typed, scratch, "mine.toml"), 1))
        goto done;
    /* Nothing within reach: report failure and stay put. */
    if (expect_int("no asset root found", asset_root_enter_from(missing), -1) ||
        expect_int("failed search keeps folder", asset_root_contains(""), 0))
        goto done;
    /* Two parents up from levels/campaigns/ is the checkout. */
    if (expect_int("asset root found", asset_root_enter_from(start), 0) ||
        expect_int("moved to the asset root", asset_root_contains(""), 1) ||
        expect_int("campaign readable after move",
                   serializer_file_exists_utf8(CAMPAIGN_MANIFEST_PATH), 1))
        goto done;
    failed = 0;
done:
    if (home && test_change_directory(home) != 0) failed = 1;
    free(typed);
    free(scratch);
    free(home);
    return failed;
}

/*
 * --start-x / --start-checkpoint (the editor's "Playtest from here"): the
 * first game starts at that point, which is also where a lost life comes
 * back to; a start the level does not have refuses the session, like a
 * level that does not load.
 */
static int start_points_place_the_first_game(void)
{
    const char *fixture = "tests/fixtures/runtime/start_points.toml";
    AppSessionConfig config = {0};
    AppSession *session;
    GameState *game;
    int failed = 0;

    /* x 616 is over the 2-tile pillar at 600: the player stands on top of
     * it, in the column centred on 616, with checkpoint 0 (x 304) behind. */
    config.level_path = fixture;
    config.start.kind = LEVEL_START_AT_X;
    config.start.x = 616.0f;
    session = session_create(&config);
    if (!session || !session->game) {
        fprintf(stderr, "session_test: start-x session_create failed\n");
        session_destroy(&session);
        return 1;
    }
    game = session->game;
    failed |= expect_float("start-x respawn x", game->world.respawn_x, 592.0f);
    failed |= expect_float("start-x stands on the pillar", game->world.respawn_y,
                           (float)(FLOOR_Y - 2 * TILE_SIZE + 16));
    failed |= expect_float("start-x player centred",
                           game->world.player.x + game->world.player.w / 2.0f, 616.0f);
    failed |= expect_int("start-x checkpoint behind", game->world.checkpoint_index, 0);
    failed |= expect_int("start used up", session->start.kind, LEVEL_START_DEFAULT);
    session_destroy(&session);
    if (failed) return 1;

    /* A checkpoint start is exactly that checkpoint's respawn. */
    memset(&config, 0, sizeof(config));
    config.level_path = fixture;
    config.start.kind = LEVEL_START_AT_CHECKPOINT;
    config.start.checkpoint = 1;
    session = session_create(&config);
    if (!session || !session->game) {
        fprintf(stderr, "session_test: start-checkpoint session_create failed\n");
        session_destroy(&session);
        return 1;
    }
    game = session->game;
    failed |= expect_float("checkpoint respawn x", game->world.respawn_x, 1000.0f);
    failed |= expect_float("checkpoint respawn y", game->world.respawn_y, 252.0f);
    failed |= expect_int("checkpoint index", game->world.checkpoint_index, 1);
    session_destroy(&session);
    if (failed) return 1;

    /* Over the floor gap at 400 there is nothing to stand on; checkpoint 5
     * does not exist.  Neither creates a session. */
    memset(&config, 0, sizeof(config));
    config.level_path = fixture;
    config.start.kind = LEVEL_START_AT_X;
    config.start.x = 416.0f;
    session = session_create(&config);
    if (session) {
        fprintf(stderr, "session_test: start over a gap created a session\n");
        session_destroy(&session);
        return 1;
    }
    config.start.kind = LEVEL_START_AT_CHECKPOINT;
    config.start.checkpoint = 5;
    session = session_create(&config);
    if (session) {
        fprintf(stderr, "session_test: missing checkpoint created a session\n");
        session_destroy(&session);
        return 1;
    }
    return 0;
}

/*
 * A level without authored checkpoints saves one at each new screen,
 * moving respawn_x only. After --start-x on the 3-tile pillar at x 256,
 * the next screen's respawn kept the pillar's height, in mid-air over the
 * floor. On a start-point run the height now follows the ground under the
 * new respawn column; a normal run keeps the level start's height, so
 * recorded runs do not change.
 */
static int start_point_respawns_follow_the_ground(void)
{
    const char *level = "levels/00_sandbox_01.toml";
    AppSessionConfig config = {.level_path = level};
    AppSession *session;
    GameState *game;
    float top = 0.0f;
    int failed = 0;

    config.start.kind = LEVEL_START_AT_X;
    config.start.x = 280.0f;
    session = session_create(&config);
    if (!session || !session->game) {
        fprintf(stderr, "session_test: sandbox start-x session_create failed\n");
        session_destroy(&session);
        return 1;
    }
    game = session->game;
    failed |= expect_float("start on the 3-tile pillar", game->world.respawn_y,
                           level_platform_top_y(3));
    game->world.player.x = 2.0f * GAME_W + 50.0f;
    game_update_active(game, GAME_FIXED_STEP, (int)game->world.camera.x);
    failed |= expect_int("screen checkpoint saved", game->world.legacy_checkpoint_screen, 2);
    failed |= expect_int("ground under the new respawn",
                         level_ground_top_at(game->world.level_def, game->world.respawn_x, &top), 1);
    failed |= expect_float("respawn on that ground", game->world.respawn_y, top);
    failed |= expect_int("not the pillar's height", game->world.respawn_y != level_platform_top_y(3), 1);
    session_destroy(&session);

    /* A normal run: the same checkpoint keeps the level start's height. */
    config.start.kind = LEVEL_START_DEFAULT;
    session = session_create(&config);
    if (!session || !session->game) {
        fprintf(stderr, "session_test: sandbox session_create failed\n");
        session_destroy(&session);
        return 1;
    }
    game = session->game;
    float start_y = game->world.respawn_y;
    game->world.player.x = 2.0f * GAME_W + 50.0f;
    game_update_active(game, GAME_FIXED_STEP, (int)game->world.camera.x);
    failed |= expect_int("normal run checkpoint saved", game->world.legacy_checkpoint_screen, 2);
    failed |= expect_float("normal run keeps the start height", game->world.respawn_y, start_y);
    session_destroy(&session);
    return failed;
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
        expect_int("held confirmation stays gated", session->game->screen.input_release_latched, 1)) goto fail;
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

    session->game->screen.completion.complete = 1;
    session->game->screen.completion.pending_next_phase = 0;
    session->game->screen.terminal_action_index = 0;
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

    session->game->screen.completion.complete = 1;
    session->game->screen.completion.pending_next_phase = 0;
    session->game->screen.terminal_action_index = 0;
    if (push_confirm() != 0) return 1;
    session_frame(session);
    if (expect_int("replay stays in game", session->screen, APP_SCREEN_GAME) != 0) return 1;
    if (expect_int("replay closes old game", session->game_close_count, 2) != 0) return 1;
    if (expect_int("replay repairs web input", session->web_input_repair_count, 3) != 0 ||
        expect_int("replay retains context", context_probe_alive(window), 1) != 0)
        return 1;

    session->game->screen.completion.complete = 1;
    session->game->screen.completion.pending_next_phase = 1;
    session->game->screen.loop.clock_started = 1;
    session->game->screen.loop.accumulator = 0.2; /* time that must not be caught up */
    session->game->screen.terminal_action_index = 0;
    if (push_confirm() != 0) return 1;
    {
        /* Bracket the frame with the real clock instead of bounding how long
         * it took: loading the next level under a sanitizer on a busy CI
         * runner can take longer than any fixed limit. */
        double frame_start = GetTime();
        session_frame(session);
        if (expect_int("next level success keeps game", session->screen, APP_SCREEN_GAME) != 0) return 1;
        if (session->game->screen.completion.complete != 0) {
            fprintf(stderr, "session_test: successful next level kept completion\n");
            return 1;
        }
        /* The clock restarts at "now" (during this frame, not before it), so
         * the next frame measures normal time (no hitch) and the 0.2 s
         * pending above is not caught up. */
        if (expect_int("next level timing reset", session->game->screen.loop.clock_started == 1 &&
                       session->game->screen.loop.prev_time >= frame_start &&
                       session->game->screen.loop.prev_time <= GetTime() &&
                       session->game->screen.loop.accumulator < GAME_FIXED_STEP, 1) != 0)
            return 1;
    }

    {
        LevelDef *def = (LevelDef *)session->game->world.runtime.current_level;
        strncpy(def->next_phase, "levels/does-not-exist.toml",
                sizeof(def->next_phase) - 1);
        def->next_phase[sizeof(def->next_phase) - 1] = '\0';
    }
    session->game->screen.completion.complete = 1;
    session->game->screen.completion.pending_next_phase = 1;
    session->game->screen.terminal_action_index = 0;
    if (push_confirm() != 0) return 1;
    session_frame(session);
    if (expect_int("next failure keeps game", session->screen, APP_SCREEN_GAME) != 0) return 1;
    if (expect_int("next failure keeps overlay", session->game->screen.completion.complete, 1) != 0) return 1;
    if (expect_int("next failure clears request", session->game->screen.route, GAME_ROUTE_NONE) != 0) return 1;
    {
        /* The failure is visible and the dead Next Level row is gone. */
        GameTerminalActionList actions;
        game_terminal_actions(session->game, &actions);
        if (expect_int("next failure flagged", session->game->screen.completion.next_phase_failed, 1) != 0 ||
            expect_int("next failure leaves three actions", actions.count, 3) != 0 ||
            expect_int("next failure focuses replay", game_terminal_focused_action(session->game),
                       GAME_TERMINAL_ACTION_REPLAY) != 0 ||
            expect_int("next failure message", strcmp(session->status_message, "Next level failed to load"), 0) != 0)
            return 1;
    }

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
    game->world.platform_count = 0;
    game->world.spider_count = 0;
    game->world.jumping_spider_count = 0;
    game->world.bird_count = 0;
    game->world.faster_bird_count = 0;
    game->world.fish_count = 0;
    game->world.faster_fish_count = 0;
    game->world.coin_count = 0;
    game->world.star_yellow_count = 0;
    game->world.star_green_count = 0;
    game->world.star_red_count = 0;
    game->world.axe_trap_count = 0;
    game->world.circular_saw_count = 0;
    game->world.spike_row_count = 0;
    game->world.spike_platform_count = 0;
    game->world.spike_block_count = 0;
    game->world.blue_flame_count = 0;
    game->world.fire_flame_count = 0;
    game->world.float_platform_count = 0;
    game->world.bridge_count = 0;
    game->world.bouncepad_small_count = 0;
    game->world.bouncepad_medium_count = 0;
    game->world.bouncepad_high_count = 0;
    game->world.floor_gap_count = 0;
    game->world.last_star.active = 0;
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
    def = (LevelDef *)game->world.runtime.current_level;
    initial_x = game->world.respawn_x;
    initial_y = game->world.respawn_y;

    /* Real active update: movement crosses CP, then real gap damage kills. */
    disable_integration_dynamic_collisions(game);
    def->checkpoint_count = 2;
    def->checkpoints[0].x = 120.0f;
    def->checkpoints[0].y = 96.0f;
    def->checkpoints[1].x = 125.0f;
    def->checkpoints[1].y = 88.0f;
    game->world.floor_gap_count = 1;
    game->world.floor_gaps[0] = 130;
    game->world.player.x = 130.0f;
    game->world.player.y = 270.0f;
    game->world.player.vx = 0.0f;
    game->world.player.vy = 0.0f;
    game->world.player.on_ground = 0;
    game_update_active(game, 0.0f, 0);
    if (expect_int("same-frame life loss preserves checkpoint",
                   game->world.checkpoint_index, 1) != 0 ||
        expect_float("same-frame checkpoint x", game->world.respawn_x, 125.0f) != 0 ||
        expect_float("same-frame checkpoint y", game->world.respawn_y, 88.0f) != 0 ||
        expect_int("same-frame life loss decrements lives", game->world.lives, 2) != 0 ||
        expect_float("same-frame player respawn x", game->world.player.spawn_x, 125.0f) != 0 ||
        expect_float("same-frame player respawn y", game->world.player.spawn_y, 88.0f) != 0)
        goto fail;

    /* Real damage path reaches game-over; real session event path retries. */
    game->world.floor_gap_count = 0;
    game->world.hearts = 1;
    game->world.lives = 0;
    apply_damage(game, 1, 0, 0.0f, 0.0f);
    if (expect_int("game-over damage sets overlay", game->screen.game_over, 1) != 0 ||
        expect_int("game-over retains checkpoint", game->world.checkpoint_index, 1) != 0)
        goto fail;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    game = session->game;
    if (!game || expect_int("retry clears game-over", game->screen.game_over, 0) != 0 ||
        expect_int("retry resets checkpoint", game->world.checkpoint_index, -1) != 0 ||
        expect_float("retry resets initial x", game->world.respawn_x, initial_x) != 0 ||
        expect_float("retry resets initial y", game->world.respawn_y, initial_y) != 0)
        goto fail;

    /* Native Replay closes/reopens through AppSession, so TOML start wins. */
    def = (LevelDef *)game->world.runtime.current_level;
    def->checkpoint_count = 1;
    def->checkpoints[0].x = 220.0f;
    def->checkpoints[0].y = 100.0f;
    game->world.checkpoint_index = 0;
    game->world.respawn_x = 220.0f;
    game->world.respawn_y = 100.0f;
    game->screen.completion.complete = 1;
    game->screen.completion.pending_next_phase = 0;
    game->screen.terminal_action_index = 0;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    game = session->game;
    if (!game || expect_int("replay keeps game screen", session->screen,
                            APP_SCREEN_GAME) != 0 ||
        expect_int("replay resets checkpoint", game->world.checkpoint_index, -1) != 0 ||
        expect_float("replay resets initial x", game->world.respawn_x, initial_x) != 0 ||
        expect_float("replay resets initial y", game->world.respawn_y, initial_y) != 0)
        goto fail;

    /* Next Level loads real phase data and must not carry old CP progress. */
    def = (LevelDef *)game->world.runtime.current_level;
    _Static_assert(sizeof(expected_next_path) == sizeof(def->next_phase),
                   "next phase copy uses the same buffer size");
    memcpy(expected_next_path, def->next_phase, sizeof(expected_next_path));
    def->checkpoint_count = 1;
    def->checkpoints[0].x = 220.0f;
    def->checkpoints[0].y = 100.0f;
    game->world.checkpoint_index = 0;
    game->world.respawn_x = 220.0f;
    game->world.respawn_y = 100.0f;
    game->screen.completion.complete = 1;
    game->screen.completion.pending_next_phase = 1;
    game->screen.terminal_action_index = 0;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    game = session->game;
    if (!game || expect_int("next phase keeps game screen", session->screen,
                            APP_SCREEN_GAME) != 0 ||
         expect_int("next phase resets checkpoint", game->world.checkpoint_index, -1) != 0 ||
         expect_int("next phase completion clears", game->screen.completion.complete, 0) != 0 ||
         expect_int("next phase path advances",
                    strcmp(game->world.level_path, expected_next_path) == 0, 1) != 0)
        goto fail;

    def = (LevelDef *)game->world.runtime.current_level;
    level_effective_spawn(def, &expected_next_x, &expected_next_y);
    if (expect_float("next phase start x", game->world.respawn_x, expected_next_x) != 0 ||
        expect_float("next phase start y", game->world.respawn_y, expected_next_y) != 0)
        goto fail;

    /* A failed load must leave active phase and resolved checkpoint untouched. */
    def = (LevelDef *)game->world.runtime.current_level;
    def->checkpoint_count = 1;
    def->checkpoints[0].x = 400.0f;
    def->checkpoints[0].y = 112.0f;
    strncpy(def->next_phase, "levels/does-not-exist.toml",
            sizeof(def->next_phase) - 1);
    def->next_phase[sizeof(def->next_phase) - 1] = '\0';
    game->world.checkpoint_index = 0;
    game->world.respawn_x = 400.0f;
    game->world.respawn_y = 112.0f;
    game->screen.completion.complete = 1;
    game->screen.completion.pending_next_phase = 1;
    game->screen.terminal_action_index = 0;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    game = session->game;
    if (!game || expect_int("failed phase keeps game screen", session->screen,
                            APP_SCREEN_GAME) != 0 ||
        expect_int("failed phase keeps completion", game->screen.completion.complete, 1) != 0 ||
        expect_int("failed phase keeps checkpoint", game->world.checkpoint_index, 0) != 0 ||
        expect_float("failed phase keeps checkpoint x", game->world.respawn_x, 400.0f) != 0 ||
        expect_float("failed phase keeps checkpoint y", game->world.respawn_y, 112.0f) != 0)
        goto fail;

    session_destroy(&session);
    return 0;

fail:
    session_destroy(&session);
    return 1;
}

typedef struct {
    AppSessionLifecycleEvent events[32];
    int event_count;
} LifecycleProbe;

static void probe_lifecycle(AppSessionLifecycleEvent event, const char *path,
                            void *userdata)
{
    LifecycleProbe *probe = userdata;
    (void)path;
    if (probe->event_count < (int)(sizeof(probe->events) / sizeof(probe->events[0])))
        probe->events[probe->event_count++] = event;
}

/*
 * Browser Replay used to persist the level path, free the whole session and
 * reload the page. The browser frame callback now replaces the game in
 * place, exactly as the native loop does: the callback stays registered,
 * the window, audio and profile stay alive, and only the GameState is
 * swapped. force_callback_mode runs the browser's callback-owned path.
 */
static int browser_replay_replaces_the_game_in_place(void)
{
    LifecycleProbe probe = {0};
    AppSessionHooks hooks = {.lifecycle = probe_lifecycle, .userdata = &probe,
                             .force_callback_mode = 1};
    AppSessionConfig config = {.level_path = "levels/00_sandbox_01.toml", .hooks = &hooks};
    AppSession *session = session_create(&config);
    if (!session) return 1;
    Texture2D probe_texture = context_probe_open();
    int failed = 1;

    if (expect_int("callback registers once", session_run(session), EXIT_SUCCESS) ||
        expect_int("callback second registration is ignored", session_run(session), EXIT_SUCCESS) ||
        expect_int("callback registration count", session->callback_registration_count, 1))
        goto done;

    /* Unsaved profile changes no longer block Replay: the profile stays in
     * memory, so nothing is lost by replaying. (This run has saving off, so
     * the changes simply stay pending.) */
    session->profile.error = session->profile.dirty = 1;
    for (int round = 1; round <= 3; round++) {
        session->game->screen.completion.complete = 1;
        session->game->world.score = 500;
        session->game->screen.route = GAME_ROUTE_REPLAY;
        session_frame(session);
        if (expect_int("replay keeps the session", session->ended, 0) ||
            expect_int("replay keeps the game screen", session->screen, APP_SCREEN_GAME) ||
            expect_int("replay has a game", session->game != NULL, 1) ||
            expect_int("replay restarts the level", session->game->screen.completion.complete, 0) ||
            expect_int("replay resets the score", session->game->world.score, 0) ||
            expect_int("replay opens count", session->game_open_count, round + 1) ||
            expect_int("replay closes count", session->game_close_count, round) ||
            expect_int("replay keeps the callback", session->callback_cancelled, 0) ||
            expect_int("replay keeps the runtime", session->runtime_cleanup_count, 0) ||
            expect_int("replay level path",
                       strcmp(session->game->world.level_path, "levels/00_sandbox_01.toml"), 0))
            goto done;
    }
    if (expect_int("profile kept in memory", session->profile.dirty, 1) ||
        expect_int("shared context survives replays", context_probe_alive(probe_texture), 1))
        goto done;
    for (int i = 0; i < probe.event_count; i++) {
        if (probe.events[i] == APP_SESSION_EVENT_SESSION_FREED ||
            probe.events[i] == APP_SESSION_EVENT_CALLBACK_CANCELLED ||
            probe.events[i] == APP_SESSION_EVENT_RUNTIME_CLEANED) {
            fprintf(stderr, "session_test: replay emitted teardown event %d\n", probe.events[i]);
            goto done;
        }
    }
    failed = 0;
done:
    UnloadTexture(probe_texture);
    session_destroy(&session);
    return failed;
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
    session->game->screen.route = GAME_ROUTE_EXIT;
    session->game->screen.loop.clock_started = 1;
    session->game->screen.loop.prev_time = GetTime() - 0.1; /* 100 ms pending */
    float elapsed = session->game->screen.completion.level_elapsed;
    session_frame(session);
    if (expect_int("pending save retains session", session->ended, 0) ||
        expect_int("pending save retains game", session->game_close_count, 0) ||
        expect_float("pending exit freezes gameplay", session->game->screen.completion.level_elapsed, elapsed)) {
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

/* The shared sprites and sounds are the session's: a Replay hands the new
 * game the very same textures and samples instead of loading them again,
 * and neither game owns them. */
static int replay_reuses_the_session_assets(void)
{
    AppSessionConfig config = {.level_path = "tests/fixtures/runtime/transition.toml"};
    AppSession *session = session_create(&config);
    if (!session) return 1;
    Texture2D *spider = session->assets.textures.spider;
    SoundEffect *jump = session->assets.audio.jump;
    int failed = expect_int("session loaded its assets", session->assets_loaded, 1) ||
                 expect_int("first game borrows the spider sprite",
                            session->game->assets.textures.spider == spider && spider != NULL, 1) ||
                 expect_int("first game does not own its assets", session->game->owns_assets, 0);
    if (!failed) {
        session->game->screen.route = GAME_ROUTE_REPLAY;
        session_frame(session);
        failed = expect_int("replay opened a new game", session->game_open_count, 2) ||
                 expect_int("replay keeps the spider sprite", session->assets.textures.spider == spider, 1) ||
                 expect_int("new game borrows the same sprite",
                            session->game->assets.textures.spider == spider, 1) ||
                 expect_int("new game borrows the same sound",
                            session->game->assets.audio.jump == jump, 1) ||
                 expect_int("new game does not own its assets", session->game->owns_assets, 0);
    }
    session_destroy(&session);
    return failed;
}

static int native_replay_keeps_session_ownership(void)
{
    AppSessionConfig config = {.level_path = "tests/fixtures/runtime/transition.toml",
                               .smoke_test_frames = 1};
    AppSession *session = session_create(&config);
    if (!session) return 1;
    session->game->screen.route = GAME_ROUTE_REPLAY;
    int result = session_run(session) != EXIT_SUCCESS ||
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
    /* The Settings button opens the panel exactly like F1: first page and
     * no message left over from an earlier visit. */
    static SettingsMenu settings;
    static GameProfile profile;
    game_profile_init(&profile);
    memset(&settings, 0, sizeof(settings));
    snprintf(settings.message, sizeof(settings.message), "stale");
    settings.page = 1;
    menu->settings_menu = &settings;
    menu->profile = &profile;
    menu->route = MENU_ROUTE_NONE;
    InputEvent click = {.type=INPUT_MOUSE_DOWN,.button=MOUSE_BUTTON_LEFT,.x=200,.y=282};
    input_push(&click);
    start_menu_frame(menu);
    if (expect_int("Settings click opens", settings.open, 1) ||
        expect_int("Settings click starts on main page", settings.page, 0) ||
        expect_int("Settings click clears message", settings.message[0], '\0'))
        result = 1;
    settings_menu_cleanup(&settings);
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
        def.spiders[0] = (SpiderPlacement){100, SPIDER_SPEED, 100, 200, 0};
        def.coin_count = 1;
        def.coins[0] = (CoinPlacement){115, 239};
        def.last_star = (LastStarPlacement){110, 236};
        gs.world.player.w = gs.world.player.h = 48;
        if (level_load(&gs, &def)) return 1;
        gs.world.player.x = 100;
        gs.world.player.y = 220;
        gs.world.player.hurt_timer = mode == 0 ? 1.0f : 0.0f;
        gs.world.hearts = mode == 0 ? 3 : 1;
        gs.world.lives = mode == 2 ? 0 : 1;
        game_collide(&gs, 1.0f / TARGET_FPS);
        if (expect_int("immunity pickups/death no stale goal", gs.screen.completion.complete, mode == 0) ||
            expect_int("coin remains after death", gs.world.coins[0].active, mode != 0) ||
            expect_int("game over only on final life", gs.screen.game_over, mode == 2)) return 1;
        if (mode == 1 && expect_float("respawn before next pass", gs.world.player.x, 20)) return 1;
    }
    return 0;
}

/* Regaining window focus must not resume music under the settings panel. */
static int settings_keep_music_paused_after_refocus(void)
{
    AppSessionConfig config = {.level_path = "levels/00_sandbox_01.toml"};
    AppSession *session = session_create(&config);
    int failed = 1;
    if (!session || !session->game || !session->game->world.music) goto done;
    Music stream = session->game->world.music->stream;
    session_frame(session);
    if (expect_int("music plays in game", IsMusicStreamPlaying(stream), 1)) goto done;
    if (push_key(KEY_F1)) goto done;
    session_frame(session);
    if (expect_int("settings open", session->settings.open, 1) ||
        expect_int("settings pause music", IsMusicStreamPlaying(stream), 0)) goto done;
    InputEvent lost = {.type=INPUT_FOCUS,.focused=0};
    InputEvent gained = {.type=INPUT_FOCUS,.focused=1};
    if (input_push(&lost) != 1 || input_push(&gained) != 1) goto done;
    session_frame(session);
    if (expect_int("refocus keeps settings music paused", IsMusicStreamPlaying(stream), 0)) goto done;
    if (push_key(KEY_ESCAPE)) goto done;
    session_frame(session);
    if (expect_int("settings closed", session->settings.open, 0) ||
        expect_int("closing settings resumes music", IsMusicStreamPlaying(stream), 1)) goto done;
    failed = 0;
done:
    session_destroy(&session);
    return failed;
}

/* Dying must not bring coins back (score farming); Retry starts over. */
static int coins_stay_collected_across_life_loss(void)
{
    GameState gs = {0};
    LevelDef def;
    level_def_init_defaults(&def);
    def.player_start_x = 20;
    def.player_start_y = FLOOR_Y;
    def.coin_count = 2;
    def.coins[0] = (CoinPlacement){30, 239};
    def.coins[1] = (CoinPlacement){300, 239};
    def.star_yellow_count = 1;
    def.star_yellows[0] = (StarYellowPlacement){30, 230};
    def.last_star = (LastStarPlacement){380, 100};
    gs.world.player.w = gs.world.player.h = 48;
    if (level_load(&gs, &def)) return 1;
    game_completion_reset_summary(&gs); /* as level_session does after a load */

    /* The player spawns over coin 0 and the star: one pass collects both. */
    gs.world.hearts = 2;
    game_collide(&gs, 1.0f / TARGET_FPS);
    int score = gs.world.score;
    if (expect_int("coin collected", gs.world.coins[0].active, 0) ||
        expect_int("star collected", gs.world.star_yellows[0].active, 0) ||
        expect_int("coin scored", score, gs.world.rules.coin_score)) return 1;

    /* Lethal damage spends a life and respawns at the same spot. */
    gs.world.player.hurt_timer = 0;
    apply_damage(&gs, gs.world.hearts, 0, 0, 0);
    if (expect_int("life spent", gs.world.lives, DEFAULT_LIVES - 1) ||
        expect_int("coin stays gone after death", gs.world.coins[0].active, 0) ||
        expect_int("uncollected coin remains", gs.world.coins[1].active, 1) ||
        expect_int("star respawns for next life", gs.world.star_yellows[0].active, 1)) return 1;
    game_collide(&gs, 1.0f / TARGET_FPS);
    if (expect_int("no second award at respawn", gs.world.score, score)) return 1;

    /* Completion counts every coin collected during the attempt. */
    game_complete_level(&gs);
    if (expect_int("summary counts coins across lives", gs.screen.completion.coins_collected, 1) ||
        expect_int("summary coin total", gs.screen.completion.coin_total, 2)) return 1;

    /* Game over, then Retry: a fresh attempt brings every coin back. */
    gs.screen.completion.complete = 0;
    gs.world.lives = 0;
    gs.world.player.hurt_timer = 0;
    apply_damage(&gs, gs.world.hearts, 0, 0, 0);
    if (expect_int("game over", gs.screen.game_over, 1)) return 1;
    game_restart_after_game_over(&gs);
    if (expect_int("retry restores coins", gs.world.coins[0].active, 1) ||
        expect_int("retry clears score", gs.world.score, 0)) return 1;
    return 0;
}

/* Pads are passed as views of the three GameState arrays; the flat index the
 * player reports must still select the right pad in the right array. */
static int bouncepad_lists_select_the_landed_pad(void)
{
    GameState gs = {0};
    gs.world.runtime.world_w = 1600;
    gs.world.player.w = gs.world.player.h = 48;
    player_apply_default_physics(&gs.world.player);
    bouncepad_place(&gs.world.bouncepads_medium[0], 0.0f, BOUNCEPAD_VY_MEDIUM, BOUNCEPAD_WOOD);
    bouncepad_place(&gs.world.bouncepads_small[0], 100.0f, BOUNCEPAD_VY_SMALL, BOUNCEPAD_GREEN);
    bouncepad_place(&gs.world.bouncepads_small[1], 150.0f, BOUNCEPAD_VY_SMALL, BOUNCEPAD_GREEN);
    bouncepad_place(&gs.world.bouncepads_high[0], 300.0f, BOUNCEPAD_VY_HIGH, BOUNCEPAD_RED);
    gs.world.bouncepad_medium_count = 1;
    gs.world.bouncepad_small_count = 2;
    gs.world.bouncepad_high_count = 1;

    /* Drop the player onto the red (high) pad: flat index 3 of 4. */
    gs.world.player.x = 300.0f;
    gs.world.player.y = (float)(FLOOR_Y - gs.world.player.h + PLAYER_FLOOR_SINK) - 1.0f;
    gs.world.player.vy = 200.0f;
    gs.screen.loop.fp_prev_riding = -1;
    game_player_step(&gs, GAME_FIXED_STEP);
    if (expect_int("high pad animates", gs.world.bouncepads_high[0].state, BOUNCE_ACTIVE) ||
        expect_int("small pads untouched", gs.world.bouncepads_small[1].state, BOUNCE_IDLE) ||
        expect_int("medium pad untouched", gs.world.bouncepads_medium[0].state, BOUNCE_IDLE) ||
        expect_float("high pad launch", gs.world.player.vy, BOUNCEPAD_VY_HIGH)) return 1;
    return 0;
}

static int fixed_step_accumulator_contract(void)
{
    /* Smoke and scripted replays: one fixed step per frame, whatever the
     * wall clock says, so their results match on every machine. */
    GameState smoke = {0};
    smoke.screen.smoke_test_frames = 5;
    game_timing_restart_clock(&smoke);
    float first = game_timing_frame_seconds(&smoke);
    clock_wait(20);
    float delayed = game_timing_frame_seconds(&smoke);
    if (expect_float("smoke fixed step", first, GAME_FIXED_STEP) ||
        expect_float("wall time does not change replay physics", delayed, first)) return 1;
    for (int frame = 0; frame < 120; frame++)
        if (expect_int("smoke runs one step per frame", game_timing_take_steps(&smoke, first), 1)) return 1;

    /* A 144 Hz display still simulates 60 steps per second; most of its
     * frames run zero steps and only redraw. */
    GameState fast = {0};
    game_timing_restart_clock(&fast);
    int steps = 0, idle_frames = 0;
    for (int frame = 0; frame < 144; frame++) {
        int frame_steps = game_timing_take_steps(&fast, 1.0f / 144);
        steps += frame_steps;
        idle_frames += frame_steps == 0;
    }
    if (expect_int("144 Hz second simulates 60 steps", steps, 60) ||
        expect_int("144 Hz frames without a step", idle_frames, 84)) return 1;

    /* Measured 60 Hz frames jitter by a fraction of a millisecond; the
     * half-step slack keeps that at exactly one step per frame. */
    GameState jitter = {0};
    game_timing_restart_clock(&jitter);
    for (int frame = 0; frame < 600; frame++) {
        float seconds = GAME_FIXED_STEP + (frame % 2 ? 0.0006f : -0.0006f);
        if (expect_int("jittery 60 Hz runs one step", game_timing_take_steps(&jitter, seconds), 1)) return 1;
    }

    /* A stall is clamped, capped and then forgotten (no spiral of death). */
    GameState stall = {0};
    game_timing_restart_clock(&stall);
    stall.screen.loop.clock_started = 1;
    stall.screen.loop.prev_time = GetTime() - 5.0;
    float stalled = game_timing_frame_seconds(&stall);
    if (expect_float("stall clamped", stalled, (float)GAME_MAX_FRAME_SECONDS) ||
        expect_int("stall capped", game_timing_take_steps(&stall, stalled), GAME_MAX_STEPS_PER_FRAME) ||
        expect_int("excess dropped", game_timing_take_steps(&stall, GAME_FIXED_STEP), 1)) return 1;
    return 0;
}

static int nearest_surface_is_order_independent(void)
{
    for (int order = 0; order < 2; order++) {
        Platform platforms[2] = {{.x=0,.y=order ? 100 : 120,.w=100},
                                 {.x=0,.y=order ? 120 : 100,.w=100}};
        FloatPlatform floating = {.x=0,.y=95,.w=100,.active=1,.prev_y=95};
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
    FloatPlatform floating = {.x=0,.y=200,.w=100,.active=1,.prev_y=200};
    Bridge bridge = {.x=0,.base_y=160,.brick_count=8};
    for (int i = 0; i < 8; i++) bridge.bricks[i].active = 1;
    SpikePlatform spike = {.x=0,.y=150,.w=100,.active=1};
    Player falling = {.x=10,.y=100,.w=48,.h=48,.vy=1500};
    player_apply_default_physics(&falling);
    int bounce, support, on_bridge;
    player_update(&falling, 0.1f, NULL, &pillar, 1, &floating, 1,
                  NULL, 0, NULL, 0, NULL, 0, NULL, 0, &bridge, 1,
                  &spike, 1, NULL, 0, &bounce, &support, &on_bridge, -1, 400);
    if (expect_float("mixed surfaces beat lower floor", falling.y + falling.h - PLAYER_FLOOR_SINK, 150) ||
        expect_int("discarded float is not ridden", support, -1) ||
        expect_int("discarded bridge is not crumbled", on_bridge, -1)) return 1;
    return 0;
}

/*
 * A Next Level load must not give up the current level until nothing can
 * fail any more. A next_phase that cannot load leaves the active LevelDef,
 * its path, its hash and a running experiment recording exactly as they
 * were; a successful one swaps in the new definition in one step.
 */
/*
 * Level Select lists every campaign level with the player's best time and
 * coins from the profile, and marks the ones already cleared. A broken
 * entry stays listed, greyed out, and shows no result.
 */
static int level_select_lists_best_results(void)
{
    CampaignCatalog catalog = {0};
    static GameProfile profile;
    StartMenu *menu = NULL;
    StartMenuLevelRow row;
    char time[16];
    int failed = 1;

    if (campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &catalog) || catalog.count < 3) goto done;
    menu = start_menu_create(&catalog);
    if (!menu) goto done;
    game_profile_init(&profile);
    menu->profile = &profile;
    if (game_profile_record(&profile, catalog.levels[1].path, 150, 3, 65.25f) ||
        game_profile_record(&profile, catalog.levels[2].path, 90, 1, 30.0f))
        goto done;
    catalog.levels[2].available = 0;  /* as if its file had broken since */

    start_menu_level_row(menu, 0, &row);
    if (expect_int("uncleared level", row.cleared, 0) ||
        expect_int("uncleared time", strcmp(row.time, "--"), 0) ||
        expect_int("uncleared coins", strcmp(row.coins, "--"), 0) ||
        expect_int("uncleared name", strcmp(row.name, catalog.levels[0].display_name), 0))
        goto done;
    start_menu_level_row(menu, 1, &row);
    char coins[16];
    snprintf(coins, sizeof(coins), "3/%d", catalog.levels[1].level.coin_count);
    if (expect_int("cleared level", row.cleared, 1) ||
        expect_int("cleared available", row.available, 1) ||
        expect_int("cleared time", strcmp(row.time, "1:05.25"), 0) ||
        expect_int("cleared coins", strcmp(row.coins, coins), 0))
        goto done;
    start_menu_level_row(menu, 2, &row);
    if (expect_int("broken entry greyed", row.available, 0) ||
        expect_int("broken entry shows no result", row.cleared, 0))
        goto done;

    start_menu_format_time(59.996f, time, sizeof(time));
    if (expect_int("rounding carries into minutes", strcmp(time, "1:00.00"), 0)) goto done;
    start_menu_format_time(0.0f, time, sizeof(time));
    if (expect_int("zero time", strcmp(time, "0:00.00"), 0)) goto done;
    start_menu_format_time(-1.0f, time, sizeof(time));
    if (expect_int("no time", strcmp(time, "--"), 0)) goto done;

    /* A click on the second list row selects that level (Play starts it),
     * and the frame with results and a broken row draws without trouble. */
    input_clear();
    InputEvent click = {.type=INPUT_MOUSE_DOWN,.button=MOUSE_BUTTON_LEFT,.x=200,.y=84+15+7};
    input_push(&click);
    if (expect_int("menu frame presents", start_menu_frame(menu), 1) ||
        expect_int("row click selects", menu->selected_level, 1) ||
        expect_int("row click does not play", menu->route, MENU_ROUTE_NONE) ||
        expect_int("row click path", strcmp(menu->selected_level_path, catalog.levels[1].path), 0))
        goto done;
    failed = 0;
done:
    start_menu_close(&menu);
    campaign_catalog_cleanup(&catalog);
    game_profile_close(&profile);
    return failed;
}

static int failed_next_phase_keeps_the_current_level(void)
{
    GameState gs = {0};
    int failed = 1;
    strcpy(gs.world.level_path, "tests/fixtures/runtime/transition.toml");
    gs.screen.debug_mode = 1;
    if (game_init(&gs)) return 1;
    if (game_experiment_begin(&gs) != 0) goto done;

    LevelDef *active = gs.world.level_def;
    uint64_t hash = gs.world.source_level_hash;
    struct GameExperiment *tape = gs.screen.experiment;
    strcpy(active->next_phase, "levels/zz_removed_phase.toml");
    if (expect_int("missing next phase fails", game_load_next_phase(&gs), -1) ||
        expect_int("active level kept", gs.world.level_def == active, 1) ||
        expect_int("runtime level kept", gs.world.runtime.current_level == active, 1) ||
        expect_int("level path kept",
                   strcmp(gs.world.level_path, "tests/fixtures/runtime/transition.toml"), 0) ||
        expect_int("level hash kept", gs.world.source_level_hash == hash, 1) ||
        expect_int("recording kept", gs.screen.experiment == tape, 1))
        goto done;

    strcpy(active->next_phase, "levels/00_sandbox_01.toml");
    if (expect_int("next phase loads", game_load_next_phase(&gs), 0) ||
        expect_int("runtime follows the new level", gs.world.runtime.current_level == gs.world.level_def, 1) ||
        expect_int("path follows the new level",
                   strcmp(gs.world.level_path, "levels/00_sandbox_01.toml"), 0) ||
        expect_int("recording ends with its level", gs.screen.experiment == NULL, 1))
        goto done;
    failed = 0;
done:
    game_cleanup(&gs);
    return failed;
}

static int phase_resets_transient_state(void)
{
    GameState gs = {0};
    strcpy(gs.world.level_path, "tests/fixtures/runtime/transition.toml");
    if (game_init(&gs)) return 1;
    gs.world.player.vx = 123;
    gs.world.player.vy = -222;
    gs.world.player.on_vine = 1;
    gs.world.player.vine_index = 7;
    gs.screen.loop.fp_prev_riding = 3;
    gs.world.score = 1200;
    gs.world.score_life_next = 2000;
    gs.world.lives = 4;
    game_complete_level(&gs);
    int result = game_load_next_phase(&gs) != 0 ||
        expect_float("phase vx cleared", gs.world.player.vx, 0) ||
        expect_float("phase vy cleared", gs.world.player.vy, 0) ||
        expect_int("phase climb cleared", gs.world.player.on_vine, 0) ||
        expect_int("phase support cleared", gs.screen.loop.fp_prev_riding, -1) ||
        expect_int("campaign score retained", gs.world.score, 1200) ||
        expect_int("campaign lives retained", gs.world.lives, 4);
    LevelDef *active = gs.world.level_def;
    active->music_volume = 0;
    level_resources_apply(&gs, active);
    if (gs.world.music && expect_float("zero volume stays muted", test_last_music_volume, 0)) result = 1;
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
        CASE(replay_reuses_the_session_assets),
        CASE(menu_mouse_and_path_boundaries), CASE(level_select_lists_best_results),
        CASE(collision_lifetime_and_pickups),
        CASE(coins_stay_collected_across_life_loss), CASE(settings_keep_music_paused_after_refocus),
        CASE(fixed_step_accumulator_contract), CASE(bouncepad_lists_select_the_landed_pad),
        CASE(nearest_surface_is_order_independent), CASE(phase_resets_transient_state),
        CASE(failed_next_phase_keeps_the_current_level),
        CASE(campaign_manifest_is_ordered_and_transactional),
        CASE(campaign_manifest_nul_fixtures_reject_transactionally),
        CASE(campaign_broken_level_disables_only_its_entry),
        CASE(physical_release_latch_blocks_transition_input),
        CASE(failed_initial_level_does_not_create_session),
        CASE(start_points_place_the_first_game),
        CASE(start_point_respawns_follow_the_ground),
        CASE(asset_root_moves_a_foreign_working_folder),
        CASE(direct_game_boot_repairs_input_and_keeps_controller_runtime),
        CASE(immediate_play_preserves_window_and_input_latch),
        CASE(repeated_menu_game_ownership), CASE(checkpoint_transitions_use_production_paths),
        CASE(browser_replay_replaces_the_game_in_place)
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
