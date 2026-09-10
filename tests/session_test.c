#include <stdio.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include "collision/collision_damage.h"
#include "collision/game_collision.h"
#include "core/app_session.h"
#include "core/game_overlay.h"
#include "core/game_timing.h"
#include "core/game_update.h"
#include "input/game_input.h"
#include "levels/level.h"
#include "levels/level_session.h"
#include "levels/level_loader.h"
#include "levels/level_path.h"
#include "levels/level_resources.h"
#include "player/player_surfaces.h"
#include "player/player_internal.h"

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
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RETURN;
    event.key.repeat = 0;
    return SDL_PushEvent(&event) == 1 ? 0 : 1;
}

static int push_key(SDL_Keycode key)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    event.key.repeat = 0;
    return SDL_PushEvent(&event) == 1 ? 0 : 1;
}

static int push_controller_button(Uint8 button)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.button = button;
    return SDL_PushEvent(&event) == 1 ? 0 : 1;
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
    player_handle_input(&player, NULL, NULL, 0, sampled,
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

    gs.controller_init_pending = 1;
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
    if (expect_int("campaign level count", (int)catalog.count, 4) != 0 ||
        expect_int("campaign first path",
                   strcmp(catalog.levels[0].path,
                          "levels/00_onboarding_01.toml") == 0, 1) != 0 ||
        expect_int("campaign first display name",
                   strcmp(catalog.levels[0].display_name,
                          "Forest First Steps") == 0, 1) != 0 ||
        expect_int("campaign onboarding successor chain",
                   strcmp(catalog.levels[0].level.next_phase,
                          "levels/00_sandbox_01.toml") == 0, 1) != 0 ||
        expect_int("campaign sandbox successor chain",
                   strcmp(catalog.levels[1].level.next_phase,
                          "levels/01_lugio_01.toml") == 0, 1) != 0 ||
        expect_int("campaign Lugio successor chain",
                   strcmp(catalog.levels[2].level.next_phase,
                          "levels/02_lugio_02.toml") == 0, 1) != 0 ||
        expect_int("campaign terminal chain",
                   catalog.levels[3].level.next_phase[0] == '\0', 1) != 0)
        goto fail;

    original_levels = catalog.levels;
    if (campaign_catalog_load("levels/campaigns/missing.toml", &catalog) == 0 ||
        expect_int("failed campaign load preserves catalog",
                   catalog.levels == original_levels, 1) != 0 ||
        expect_int("failed campaign load preserves count", (int)catalog.count, 4) != 0)
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
        expect_int("direct boot controller pending",
                   session->controller_init_state, APP_CONTROLLER_INIT_PENDING) != 0 ||
        expect_int("direct boot has no sync controller init",
                   session->controller_subsystem_ready_count, 0) != 0 ||
        expect_int("direct boot controller inactive",
                   SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0, 0) != 0 ||
        expect_int("direct boot latch armed", session->game->input_release_latched, 1) != 0 ||
        expect_int("direct boot held input gated", game_input_sample(session->game), 0) != 0)
        goto fail;

    session_frame(session);
    if (expect_int("direct boot schedules once", session->controller_init_schedule_count, 1) != 0 ||
        expect_int("direct boot still renders before readiness",
                   session->screen, APP_SCREEN_GAME) != 0)
        goto fail;

    game_input_test_set_physical_state(0, 0);
    if (expect_int("direct boot release opens gate",
                   game_input_sample(session->game), 0) != 0 ||
        expect_int("direct boot latch cleared",
                   session->game->input_release_latched, 0) != 0)
        goto fail;

    for (int i = 0; i < 100 &&
                    session->controller_init_state == APP_CONTROLLER_INIT_RUNNING; i++) {
        session_frame(session);
    }
    if (expect_int("direct boot worker joined once", session->controller_init_join_count, 1) != 0)
        goto fail;
    session_destroy(&session);
    game_input_test_clear_physical_state();
    if (expect_int("direct boot controller cleanup",
                   SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0, 0) != 0)
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

typedef struct {
    AppSession *session;
    SDL_atomic_t worker_entered;
    SDL_atomic_t worker_release;
    int worker_calls;
    int game_open_overlap;
} DeferredControllerProbe;

static int blocked_controller_init(void *userdata)
{
    DeferredControllerProbe *probe = userdata;

    probe->worker_calls++;
    SDL_AtomicSet(&probe->worker_entered, 1);
    while (!SDL_AtomicGet(&probe->worker_release)) {
        if (probe->session &&
            SDL_AtomicGet(&probe->session->game_open_in_progress)) {
            probe->game_open_overlap = 1;
        }
        SDL_Delay(1);
    }
    return 0;
}

static void release_blocked_controller(AppSession *session,
                                       DeferredControllerProbe *probe)
{
    SDL_AtomicSet(&probe->worker_release, 1);
    for (int i = 0; session && i < 100 && session->controller_init_thread; i++) {
        session_frame(session);
    }
}

static int immediate_play_waits_for_present_before_controller_worker(void)
{
    DeferredControllerProbe probe = {0};
    AppSessionHooks hooks = {0};
    AppSessionConfig config = {0};
    AppSession *session;

    hooks.controller_init = blocked_controller_init;
    hooks.userdata = &probe;
    config.hooks = &hooks;
    session = session_create(&config);
    if (!session) {
        fprintf(stderr, "session_test: immediate Play session_create failed\n");
        return 1;
    }
    probe.session = session;
    SDL_AtomicSet(&probe.worker_entered, 0);
    SDL_AtomicSet(&probe.worker_release, 0);

    /* Simulate a held confirm arriving with the first menu route. */
    session->menu->confirm_release_required = 0;
    game_input_test_set_physical_state(GAME_INPUT_CONFIRM, 0);
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    if (expect_int("immediate Play opens game", session->screen, APP_SCREEN_GAME) != 0 ||
        expect_int("immediate Play has no menu present", session->menu_presented_count, 0) != 0 ||
        expect_int("immediate Play has no game present yet", session->game_presented_count, 0) != 0 ||
        expect_int("immediate Play has no worker schedule", session->controller_init_schedule_count, 0) != 0 ||
        expect_int("immediate Play constructs one game", session->game_open_count, 1) != 0 ||
        expect_int("immediate Play worker not entered", SDL_AtomicGet(&probe.worker_entered), 0) != 0)
        goto fail;

    session_frame(session);
    if (expect_int("game first frame presents", session->game_presented_count, 1) != 0 ||
        expect_int("worker schedules after game present", session->controller_init_schedule_count, 1) != 0)
        goto fail;

    for (int i = 0; i < 100 && !SDL_AtomicGet(&probe.worker_entered); i++)
        SDL_Delay(1);
    if (expect_int("blocked worker entered after present",
                   SDL_AtomicGet(&probe.worker_entered), 1) != 0 ||
        expect_int("blocked worker never overlaps game construction",
                   probe.game_open_overlap, 0) != 0)
        goto fail;

    release_blocked_controller(session, &probe);
    if (expect_int("blocked worker joined", session->controller_init_join_count, 1) != 0 ||
        expect_int("controller readiness published", session->controller_init_state,
                   APP_CONTROLLER_INIT_READY) != 0)
        goto fail;

    session_destroy(&session);
    game_input_test_clear_physical_state();
    return 0;

fail:
    release_blocked_controller(session, &probe);
    session_destroy(&session);
    game_input_test_clear_physical_state();
    return 1;
}

static int blocked_menu_route_waits_without_teardown(void)
{
    DeferredControllerProbe probe = {0};
    AppSessionHooks hooks = {0};
    AppSessionConfig config = {0};
    AppSession *session;
    StartMenu *menu;
    int menu_frames;

    hooks.controller_init = blocked_controller_init;
    hooks.userdata = &probe;
    config.hooks = &hooks;
    session = session_create(&config);
    if (!session) {
        fprintf(stderr, "session_test: blocked menu session_create failed\n");
        return 1;
    }
    probe.session = session;
    session_frame(session);
    if (expect_int("blocked menu schedules worker",
                   session->controller_init_schedule_count, 1) != 0)
        goto fail;
    for (int i = 0; i < 100 && !SDL_AtomicGet(&probe.worker_entered); i++)
        SDL_Delay(1);
    if (expect_int("blocked menu worker entered",
                   SDL_AtomicGet(&probe.worker_entered), 1) != 0)
        goto fail;

    menu = session->menu;
    menu_frames = session->menu_presented_count;
    menu->confirm_release_required = 0;
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    if (expect_int("blocked menu retains screen", session->screen, APP_SCREEN_MENU) != 0 ||
        expect_int("blocked menu keeps menu alive", session->menu == menu, 1) != 0 ||
        expect_int("blocked menu does not close menu", session->menu_close_count, 0) != 0 ||
        expect_int("blocked menu does not construct game", session->game_open_count, 0) != 0 ||
        expect_int("blocked menu still presents", session->menu_presented_count,
                   menu_frames + 1) != 0 ||
        expect_int("blocked menu has not joined worker", session->controller_init_join_count, 0) != 0)
        goto fail;

    release_blocked_controller(session, &probe);
    if (expect_int("blocked menu joins worker", session->controller_init_join_count, 1) != 0 ||
        expect_int("blocked menu publishes readiness", session->controller_init_state,
                   APP_CONTROLLER_INIT_READY) != 0 ||
        expect_int("blocked menu transitions once", session->screen, APP_SCREEN_GAME) != 0 ||
        expect_int("blocked menu closes once", session->menu_close_count, 1) != 0 ||
        expect_int("blocked menu constructs once", session->game_open_count, 1) != 0)
        goto fail;

    session_frame(session);
    if (expect_int("blocked menu remains in game", session->screen, APP_SCREEN_GAME) != 0 ||
        expect_int("blocked menu construction stays once", session->game_open_count, 1) != 0 ||
        expect_int("blocked menu close stays once", session->menu_close_count, 1) != 0)
        goto fail;

    session_destroy(&session);
    game_input_test_clear_physical_state();
    return 0;

fail:
    release_blocked_controller(session, &probe);
    session_destroy(&session);
    game_input_test_clear_physical_state();
    return 1;
}

static int blocked_terminal_exit_waits_with_overlay_visible(void)
{
    DeferredControllerProbe probe = {0};
    AppSessionHooks hooks = {0};
    AppSessionConfig config = {0};
    AppSession *session;
    GameState *game;
    int game_frames;

    hooks.controller_init = blocked_controller_init;
    hooks.userdata = &probe;
    config.level_path = "levels/00_sandbox_01.toml";
    config.hooks = &hooks;
    session = session_create(&config);
    if (!session) {
        fprintf(stderr, "session_test: blocked terminal session_create failed\n");
        return 1;
    }
    probe.session = session;
    session_frame(session);
    if (expect_int("blocked terminal schedules worker",
                   session->controller_init_schedule_count, 1) != 0)
        goto fail;
    for (int i = 0; i < 100 && !SDL_AtomicGet(&probe.worker_entered); i++)
        SDL_Delay(1);
    if (expect_int("blocked terminal worker entered",
                   SDL_AtomicGet(&probe.worker_entered), 1) != 0)
        goto fail;

    game = session->game;
    game_frames = session->game_presented_count;
    game->completion.complete = 1;
    game->completion.pending_next_phase = 0;
    game->terminal_action_index = 2; /* Replay, Level Select, Exit. */
    if (push_confirm() != 0) goto fail;
    session_frame(session);
    if (expect_int("blocked terminal retains game", session->screen, APP_SCREEN_GAME) != 0 ||
        expect_int("blocked terminal keeps game alive", session->game == game, 1) != 0 ||
        expect_int("blocked terminal does not close game", session->game_close_count, 0) != 0 ||
        expect_int("blocked terminal presents overlay", session->game_presented_count,
                   game_frames + 1) != 0 ||
        expect_int("blocked terminal retains exit route", game->route, GAME_ROUTE_EXIT) != 0 ||
        expect_int("blocked terminal has not joined worker", session->controller_init_join_count, 0) != 0)
        goto fail;

    release_blocked_controller(session, &probe);
    if (expect_int("blocked terminal joins worker", session->controller_init_join_count, 1) != 0 ||
        expect_int("blocked terminal publishes readiness", session->controller_init_state,
                   APP_CONTROLLER_INIT_READY) != 0 ||
        expect_int("blocked terminal ends once", session->screen, APP_SCREEN_ENDED) != 0 ||
        expect_int("blocked terminal closes game once", session->game_close_count, 1) != 0 ||
        expect_int("blocked terminal cleans runtime once", session->runtime_cleanup_count, 1) != 0)
        goto fail;

    session_destroy(&session);
    return 0;

fail:
    release_blocked_controller(session, &probe);
    session_destroy(&session);
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
    if (expect_int("starts in menu", session->screen, APP_SCREEN_MENU) != 0 ||
        expect_int("default menu owns campaign", session->catalog_loaded, 1) != 0 ||
        expect_int("menu consumes campaign", session->menu->catalog == &session->catalog, 1) != 0 ||
        expect_int("menu campaign count", (int)session->catalog.count, 4) != 0 ||
        expect_int("menu controller pending",
                   session->controller_init_state, APP_CONTROLLER_INIT_PENDING) != 0 ||
        expect_int("menu controller inactive",
                   SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0, 0) != 0 ||
        expect_int("menu init has no sync worker", session->controller_init_schedule_count, 0) != 0)
        return 1;
    session_frame(session);
    if (expect_int("menu schedules controller once", session->controller_init_schedule_count, 1) != 0)
        return 1;
    if (push_controller_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0) return 1;
    session_frame(session);
    if (expect_int("menu D-pad selects next level",
                   strcmp(session->menu->selected_level_path,
                           "levels/00_sandbox_01.toml") == 0, 1) != 0)
        return 1;
    for (int i = 0; i < 100 &&
                    session->controller_init_state == APP_CONTROLLER_INIT_RUNNING; i++)
        session_frame(session);
    if (push_controller_button(SDL_CONTROLLER_BUTTON_A) != 0) return 1;
    session_frame(session);
    if (expect_int("menu opens game", session->screen, APP_SCREEN_GAME) != 0) return 1;
    if (expect_int("menu closes once", session->menu_close_count, 1) != 0) return 1;
    if (expect_int("menu-to-game controller stays active",
                   SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0, 1) != 0 ||
        expect_int("menu-to-game repairs web input", session->web_input_repair_count, 1) != 0)
        return 1;

    session->game->completion.complete = 1;
    session->game->completion.pending_next_phase = 0;
    session->game->terminal_action_index = 0;
    if (push_key(SDLK_DOWN) != 0 || push_confirm() != 0) return 1;
    session_frame(session);
    if (expect_int("level select opens menu", session->screen, APP_SCREEN_MENU) != 0) return 1;
    if (expect_int("game closes once", session->game_close_count, 1) != 0) return 1;
    if (expect_int("level-select controller stays active",
                   SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0, 1) != 0)
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
        expect_int("second game controller stays active",
                   SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0, 1) != 0)
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
        expect_int("replay controller stays active",
                   SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0, 1) != 0)
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

    if (push_key(SDLK_ESCAPE) != 0) return 1;
    session_frame(session);
    if (expect_int("exit ends session", session->screen, APP_SCREEN_ENDED) != 0) return 1;
    if (expect_int("runtime cleanup once", session->runtime_cleanup_count, 1) != 0) return 1;
    if (expect_int("controller cleanup once", session->controller_subsystem_closed_count, 1) != 0 ||
        expect_int("controller worker joined once", session->controller_init_join_count, 1) != 0 ||
        expect_int("controller worker scheduled once", session->controller_init_schedule_count, 1) != 0 ||
        expect_int("controller inactive after exit",
                   SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0, 0) != 0)
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

static void wait_for_controller_ready(AppSession *session)
{
    for (int i = 0; session && i < 100; i++) {
        if (session->controller_init_state == APP_CONTROLLER_INIT_READY ||
            session->controller_init_state == APP_CONTROLLER_INIT_FAILED)
            return;
        session_frame(session);
    }
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
    wait_for_controller_ready(session);
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
        1,
        NULL
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
    for (int i = 0; i < 100 &&
                    session->controller_init_state == APP_CONTROLLER_INIT_RUNNING; i++) {
        session_frame(session);
    }
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

static int native_replay_keeps_session_ownership(void)
{
    LifecycleProbe probe = {.store_result = 1};
    AppSessionHooks hooks = {.store_replay = probe_store_replay,
                            .reload = probe_reload, .userdata = &probe};
    AppSessionConfig config = {.level_path = "levels/00_onboarding_01.toml",
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
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_Event event = {0};
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.windowID = SDL_GetWindowID(menu->window);
    event.button.button = SDL_BUTTON_LEFT;
    event.button.state = SDL_PRESSED;
    event.button.x = 400;
    event.button.y = 368;
    SDL_PushEvent(&event);
    start_menu_frame(menu);
    int result = expect_int("physical Play click", menu->route, MENU_ROUTE_PLAY);
    start_menu_close(&menu);
    campaign_catalog_cleanup(&catalog);
    char short_path[8];
    if (expect_int("reject resolved path truncation",
                   level_resolve_path("levels/00_onboarding_01.toml", short_path, sizeof(short_path)), -1)) result = 1;
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
    timing.loop.prev_ticks = SDL_GetTicks64();
    float first = game_timing_step(&timing, NULL);
    SDL_Delay(20);
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
    gs.controller_init_pending = 1;
    strcpy(gs.level_path, "levels/00_onboarding_01.toml");
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
    if (gs.audio.music && expect_int("zero volume stays muted", Mix_VolumeMusic(-1), 0)) result = 1;
    game_cleanup(&gs);
    return result;
}

int game_profile_contract_test(void);

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "session_test: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) || TTF_Init() != 0) {
        fprintf(stderr, "session_test: SDL image/font init failed\n");
        SDL_Quit();
        return 1;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "session_test: Mix_OpenAudio failed: %s\n", Mix_GetError());
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    if (game_profile_contract_test()) return 1;
    if (native_replay_keeps_session_ownership()) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0 ||
        !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) || TTF_Init() != 0 ||
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) return 1;
    if (menu_mouse_and_path_boundaries()) return 1;
    if (collision_lifetime_and_pickups() || nearest_surface_is_order_independent() ||
        phase_resets_transient_state()) return 1;
    if (campaign_manifest_is_ordered_and_transactional() != 0) return 1;
    if (campaign_manifest_nul_fixtures_reject_transactionally() != 0) return 1;
    if (physical_release_latch_blocks_transition_input() != 0) return 1;
    if (failed_initial_level_does_not_create_session() != 0) return 1;
    if (direct_game_boot_repairs_input_and_keeps_controller_runtime() != 0) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0 ||
        !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) || TTF_Init() != 0 ||
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "session_test: failed to reinitialize after direct boot\n");
        return 1;
    }
    if (immediate_play_waits_for_present_before_controller_worker() != 0) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0 ||
        !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) || TTF_Init() != 0 ||
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "session_test: failed to reinitialize after deferred controller test\n");
        return 1;
    }
    if (blocked_menu_route_waits_without_teardown() != 0) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0 ||
        !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) || TTF_Init() != 0 ||
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "session_test: failed to reinitialize after blocked menu test\n");
        return 1;
    }
    if (blocked_terminal_exit_waits_with_overlay_visible() != 0) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0 ||
        !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) || TTF_Init() != 0 ||
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "session_test: failed to reinitialize after blocked terminal test\n");
        return 1;
    }
    if (repeated_menu_game_ownership() != 0) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0 ||
        !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) || TTF_Init() != 0 ||
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "session_test: failed to reinitialize after menu ownership test\n");
        return 1;
    }
    if (checkpoint_transitions_use_production_paths() != 0) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0 ||
        !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) || TTF_Init() != 0 ||
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "session_test: failed to reinitialize after checkpoint integration\n");
        return 1;
    }
    if (replay_storage_failure_retains_session_and_success_orders_cleanup() != 0)
        return 1;
    puts("session_test: ok");
    return 0;
}
