/* AppSession owns the process window/audio and the only native/web frame loop.
 * Screens own render targets and assets, so a failed candidate level can leave
 * the existing menu alive without recreating the global raylib context. */
#include "app_session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../input/game_web_input.h"
#include "../input/game_input.h"
#include "../levels/level_session.h"
#include "../levels/level_path.h"
#include "game_overlay.h"
#include "game_experiment.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(int, session_browser_store_replay_js, (const char *path), {
    try { sessionStorage.setItem('super-mango-replay-level', UTF8ToString(path)); return 1; }
    catch (e) { console.error('Super Mango: unable to persist replay level', e); return 0; }
});
EM_JS(void, session_browser_reload, (const char *path), { void path; window.location.reload(); });
EM_JS(void, session_browser_ended, (int fatal, int profile_error), {
    if (typeof Module.onGameEnded === 'function') Module.onGameEnded(fatal, profile_error);
});
#endif

static void copy_path(char *out, size_t size, const char *path)
{
    if (out && size) str_copy(out, path ? path : "", size);
}

static void session_emit(AppSession *session, AppSessionLifecycleEvent event, const char *path)
{
    if (session->hooks.lifecycle) session->hooks.lifecycle(event, path, session->hooks.userdata);
}

static int session_load_catalog(AppSession *session)
{
    if (session->catalog_loaded) return 0;
    if (campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &session->catalog)) {
        copy_path(session->status_message, sizeof(session->status_message), "Campaign manifest unavailable");
        return -1;
    }
    session->catalog_loaded = 1;
    return 0;
}

static void session_free_owned(AppSession *session)
{
    game_profile_close(&session->profile);
    campaign_catalog_cleanup(&session->catalog);
    free(session);
}

static void session_repair_web_input(AppSession *session)
{
    game_web_input_flush_stale_keys();
    session->web_input_repair_count++;
    session_emit(session, APP_SESSION_EVENT_WEB_INPUT_REPAIRED, NULL);
}

static void session_runtime_cleanup(AppSession *session)
{
    if (session->runtime_cleaned) return;
#ifndef __EMSCRIPTEN__
    if (session->profile.dirty && game_profile_save(&session->profile) < 0)
        TraceLog(LOG_WARNING, "%s", session->profile.status);
#endif
    session->runtime_cleaned = 1;
    session->runtime_cleanup_count++;
    game_web_input_clear_touch();
    input_close();
    audio_close();
    if (IsWindowReady()) CloseWindow();
    session_emit(session, APP_SESSION_EVENT_RUNTIME_CLEANED, NULL);
}

static void session_cancel_callback(AppSession *session)
{
    if (!session->callback_registered || session->callback_cancelled) return;
    session->callback_cancelled = 1;
    session->callback_cancellation_count++;
    session_emit(session, APP_SESSION_EVENT_CALLBACK_CANCELLED, NULL);
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
}

static void session_close_menu(AppSession *session)
{
    if (!session->menu) return;
    settings_menu_cleanup(&session->settings);
    start_menu_close(&session->menu);
    session->menu_close_count++;
}

static void session_close_game(AppSession *session)
{
    if (!session->game) return;
    settings_menu_cleanup(&session->settings);
    GameState *game = session->game;
    session->game = NULL;
    game_cleanup(game);
    free(game);
    session->game_close_count++;
    session_emit(session, APP_SESSION_EVENT_GAME_CLOSED, NULL);
}

static void session_profile_key(GameState *game, const char *source)
{
    game->profile_level_key[0] = 0;
    if (game_profile_key_valid(source)) {
        copy_path(game->profile_level_key, sizeof(game->profile_level_key), source);
        return;
    }
    const char *base = strrchr(game->level_path, '/');
    const char *back = strrchr(game->level_path, '\\');
    if (!base || (back && back > base)) base = back;
    char key[PROFILE_LEVEL_PATH], resolved[GAME_LEVEL_PATH_MAX];
    int size = snprintf(key, sizeof(key), "levels/%s", base ? base+1 : game->level_path);
    if (size > 0 && (size_t)size < sizeof(key) && game_profile_key_valid(key) &&
        !level_resolve_path(key, resolved, sizeof(resolved)) && !strcmp(resolved, game->level_path))
        copy_path(game->profile_level_key, sizeof(game->profile_level_key), key);
}

static void session_apply_preferences(AppSession *session)
{
    if (game_profile_poll(&session->profile) < 0) TraceLog(LOG_WARNING, "%s", session->profile.status);
    GameSettings *s = &session->profile.data.settings;
    if (!session->settings.open && (!session->preferences_applied ||
        session->applied_settings_revision != session->profile.revision)) {
        int volume = s->muted ? 0 : s->effects_volume;
        if (session->game) {
            GameState *game = session->game;
#define VOLUME(member) sound_set_volume(game->audio.member, volume)
            VOLUME(coin); VOLUME(jump); VOLUME(hit); VOLUME(spring);
            VOLUME(axe); VOLUME(flap); VOLUME(spider_attack); VOLUME(dive);
#undef VOLUME
            const LevelDef *level = game->runtime.current_level;
            music_set_volume(s->muted ? 0 : level->music_volume*s->music_volume/128);
        }
        if (session->menu) sound_set_volume(session->menu->snd_confirm, volume);
#ifndef __EMSCRIPTEN__
        if (IsWindowReady() && (GetScreenWidth() != GAME_W*s->window_scale || GetScreenHeight() != GAME_H*s->window_scale))
            SetWindowSize(GAME_W*s->window_scale, GAME_H*s->window_scale);
#endif
        session->preferences_applied = 1;
        session->applied_settings_revision = session->profile.revision;
    }
    if (session->settings.open != session->settings_were_open) {
        if (session->settings.open) music_pause();
        else if (!session->game || !session->game->paused) music_resume();
        session->settings_were_open = session->settings.open;
    }
    if (!session->settings.open && !session->profile.pending_text && session->profile.dirty &&
        session->attempted_save_revision != session->profile.revision) {
        session->attempted_save_revision = session->profile.revision;
        if (game_profile_save(&session->profile) < 0) TraceLog(LOG_WARNING, "%s", session->profile.status);
    }
}

static int session_profile_ready_to_leave(AppSession *session)
{
    int result = game_profile_poll(&session->profile);
    if (result == PROFILE_SAVE_PENDING) return 0;
    if (session->profile.dirty && session->attempted_save_revision != session->profile.revision) {
        session->attempted_save_revision = session->profile.revision;
        result = game_profile_save(&session->profile);
        if (result < 0) TraceLog(LOG_WARNING, "%s", session->profile.status);
    }
    return result != PROFILE_SAVE_PENDING;
}

static GameState *session_make_game(AppSession *session, const char *path, const GameInputPhysicalState *inherited)
{
    GameState *game = calloc(1, sizeof(*game));
    if (!game) return NULL;
    game->debug_mode = session->debug_mode;
    game->random_seed = session->random_seed;
    game->smoke_test_frames = session->smoke_test_frames;
    copy_path(game->level_path, sizeof(game->level_path), path);
    copy_path(game->replay_script_path, sizeof(game->replay_script_path), session->replay_script_path);
    if (game_init(game)) { free(game); return NULL; }
    game->loop.prev_ticks = clock_millis();
    game->profile = &session->profile;
    game->settings_menu = &session->settings;
    session_profile_key(game, path);
    game_profile_select(&session->profile, game->profile_level_key);
    session->preferences_applied = 0;
    game->loop.fp_prev_riding = -1;
    session_repair_web_input(session);
    game_input_arm_release_latch(game, inherited);
    session->game_open_count++;
    return game;
}

static int session_open_game(AppSession *session, const char *path, const GameInputPhysicalState *inherited)
{
    session->game = session_make_game(session, path, inherited);
    if (!session->game) return -1;
    session->screen = APP_SCREEN_GAME;
    return 0;
}

static int session_open_menu(AppSession *session)
{
    if (session_load_catalog(session)) return -1;
    game_web_input_clear_touch();
    input_clear();
    session->menu = start_menu_create(&session->catalog);
    if (!session->menu) return -1;
    session->menu->profile = &session->profile;
    session->menu->settings_menu = &session->settings;
    for (size_t i = 0; i < session->catalog.count; i++) {
        if (!strcmp(session->catalog.levels[i].path, session->profile.data.last_level)) {
            session->menu->selected_level = (int)i;
            copy_path(session->menu->selected_level_path, sizeof(session->menu->selected_level_path), session->profile.data.last_level);
            break;
        }
    }
    session->preferences_applied = 0;
    session->menu->confirm_release_required = 1;
    session->screen = APP_SCREEN_MENU;
    return 0;
}

static void session_end(AppSession *session, int fatal)
{
    if (session->ended) return;
    session->fatal |= fatal;
    session_cancel_callback(session);
    session_close_menu(session);
    session_close_game(session);
    session->screen = APP_SCREEN_ENDED;
    if (fatal) session->route = APP_ROUTE_FATAL;
    session->ended = 1;
    session_runtime_cleanup(session);
#ifdef __EMSCRIPTEN__
    session_browser_ended(fatal, session->profile.error);
#endif
}

static void session_apply_menu_route(AppSession *session)
{
    if (!session->menu || session->menu->route == MENU_ROUTE_NONE) return;
    MenuRoute route = session->menu->route;
    if (route == MENU_ROUTE_EXIT && !session_profile_ready_to_leave(session)) return;
    if (route == MENU_ROUTE_PLAY) {
        GameInputPhysicalState inherited;
        session->route = APP_ROUTE_MENU_PLAY;
        start_menu_get_input_state(session->menu, &inherited);
        GameState *candidate = session_make_game(session, session->menu->selected_level_path, &inherited);
        if (!candidate) {
            copy_path(session->status_message, sizeof(session->status_message), "Selected level could not be loaded");
            start_menu_set_error(session->menu, session->status_message);
            session->menu->route = MENU_ROUTE_NONE;
        } else {
            session_close_menu(session);
            session->game = candidate;
            session->screen = APP_SCREEN_GAME;
        }
        session->route = APP_ROUTE_NONE;
    } else {
        session->route = route == MENU_ROUTE_EXIT ? APP_ROUTE_MENU_EXIT : APP_ROUTE_FATAL;
        session_end(session, route != MENU_ROUTE_EXIT);
    }
}

static int session_browser_replay(AppSession *session, const char *path)
{
    int stored = 0;
    if (session->hooks.store_replay) stored = session->hooks.store_replay(path, session->hooks.userdata);
#ifdef __EMSCRIPTEN__
    else stored = session_browser_store_replay_js(path);
#endif
    session->replay_storage_attempts++;
    if (!stored) {
        copy_path(session->status_message, sizeof(session->status_message), "Replay unavailable: storage failed");
        session->game->route = GAME_ROUTE_NONE;
        session->route = APP_ROUTE_NONE;
        TraceLog(LOG_WARNING, "%s", session->status_message);
        return 0;
    }
    session->replay_storage_successes++;
    AppSessionHooks hooks = session->hooks;
    session_cancel_callback(session);
    session_close_game(session);
    session->screen = APP_SCREEN_ENDED;
    session->ended = session->browser_reload_requested = 1;
    session_runtime_cleanup(session);
    session_emit(session, APP_SESSION_EVENT_SESSION_FREED, path);
    session_free_owned(session);
    if (hooks.reload) hooks.reload(path, hooks.userdata);
#ifdef __EMSCRIPTEN__
    else session_browser_reload(path);
#endif
    return 1;
}

static int session_apply_game_route(AppSession *session, int callback_owned)
{
    GameState *game = session->game;
    if (!game) return 0;
    GameRoute route = game->route;
    char path[GAME_LEVEL_PATH_MAX];
    if (route == GAME_ROUTE_NONE && !game->running) route = GAME_ROUTE_EXIT;
    if (route == GAME_ROUTE_NONE) return 0;
    if ((route == GAME_ROUTE_EXIT || route == GAME_ROUTE_REPLAY) && !session_profile_ready_to_leave(session)) return 0;
    if (route == GAME_ROUTE_REPLAY && callback_owned && session->profile.enabled &&
        session->profile.writable && session->profile.error && session->profile.dirty) {
        copy_path(session->profile.status, sizeof(session->profile.status), "Replay blocked: profile not saved. Use Level Select or Exit.");
        game->route = GAME_ROUTE_NONE;
        session->route = APP_ROUTE_NONE;
        return 0;
    }
    game->route = GAME_ROUTE_NONE;
    switch (route) {
    case GAME_ROUTE_NEXT_LEVEL:
        session->route = APP_ROUTE_GAME_NEXT_LEVEL;
        copy_path(path, sizeof(path), game->completion.next_phase);
        if (!game_load_next_phase(game)) {
            session_profile_key(game, path);
            game_profile_select(&session->profile, game->profile_level_key);
            session->preferences_applied = 0;
            game->loop.prev_ticks = clock_millis();
            game_input_arm_release_latch(game, NULL);
        }
        game->route = GAME_ROUTE_NONE;
        session->route = APP_ROUTE_NONE;
        break;
    case GAME_ROUTE_REPLAY:
        session->route = APP_ROUTE_GAME_REPLAY;
        copy_path(path, sizeof(path), game->level_path);
        if (callback_owned) return session_browser_replay(session, path);
        {
            GameInputPhysicalState inherited;
            game_input_read_physical(game->controller, &inherited);
            session_close_game(session);
            if (session_open_game(session, path, &inherited)) session_end(session, 1);
            else session->route = APP_ROUTE_NONE;
        }
        break;
    case GAME_ROUTE_LEVEL_SELECT:
        session->route = APP_ROUTE_GAME_LEVEL_SELECT;
        if (session_load_catalog(session)) {
            copy_path(session->status_message, sizeof(session->status_message), "Level Select unavailable: campaign manifest failed");
            session->route = APP_ROUTE_NONE;
            return 0;
        }
        session_close_game(session);
        if (session_open_menu(session)) session_end(session, 1);
        else session->route = APP_ROUTE_NONE;
        break;
    default:
        session->route = route == GAME_ROUTE_SMOKE_EXIT ? APP_ROUTE_SMOKE_EXIT :
            route == GAME_ROUTE_FATAL ? APP_ROUTE_FATAL : APP_ROUTE_GAME_EXIT;
        session_end(session, route == GAME_ROUTE_FATAL);
        break;
    }
    return 0;
}

AppSession *session_create(const AppSessionConfig *config)
{
    AppSession *session = calloc(1, sizeof(*session));
    const char *level = config ? config->level_path : NULL;
    if (!session) return NULL;
    game_profile_init(&session->profile);
    if ((level && strlen(level) >= sizeof(session->boot_level_path)) ||
        (config && config->replay_script_path && strlen(config->replay_script_path) >= sizeof(session->replay_script_path))) {
        free(session); return NULL;
    }
    session->debug_mode = config && (config->debug_mode || config->experiment_path);
    session->random_seed = config ? config->random_seed : 1;
    session->smoke_test_frames = config ? config->smoke_test_frames : 0;
    if (config && config->hooks) session->hooks = *config->hooks;
    copy_path(session->replay_script_path, sizeof(session->replay_script_path), config ? config->replay_script_path : NULL);
    if (!IsWindowReady() && display_open(WINDOW_W, WINDOW_H, WINDOW_TITLE,
        session->smoke_test_frames > 0 || getenv("MANGO_TEST_WINDOW") != NULL)) goto fail;
    input_open(GAME_W, GAME_H);
    if (!IsAudioDeviceReady() && audio_open()) goto fail;
    if (config && config->profile_enabled && !session->debug_mode && !config->experiment_path &&
        !session->smoke_test_frames && !session->replay_script_path[0]) {
        if (game_profile_open(&session->profile, config->profile_path)) TraceLog(LOG_WARNING, "%s", session->profile.status);
    } else copy_path(session->profile.status, sizeof(session->profile.status), "Saving disabled; settings apply to this run.");
    if (!level && config && config->continue_last && session->profile.data.last_level[0]) level = session->profile.data.last_level;
    if (level && level[0]) {
        copy_path(session->boot_level_path, sizeof(session->boot_level_path), level);
        if (session_open_game(session, session->boot_level_path, NULL)) goto fail;
    } else if (session_open_menu(session)) goto fail;
    if (config && config->experiment_path && (!session->game || game_experiment_load(session->game, config->experiment_path))) goto fail;
    return session;
fail:
    session_close_game(session);
    session_close_menu(session);
    session_runtime_cleanup(session);
    session_free_owned(session);
    return NULL;
}

static void session_step(AppSession *session, int callback_owned)
{
    if (!session || session->ended) return;
    session_apply_preferences(session);
    input_collect();
    music_update();
    if (session->screen == APP_SCREEN_MENU) {
        session->menu->route_waiting_render = session->profile.pending_text != NULL;
        if (start_menu_frame(session->menu)) session->menu_presented_count++;
        session->menu->route_waiting_render = 0;
        session_apply_menu_route(session);
    } else if (session->screen == APP_SCREEN_GAME) {
        GameState *game = session->game;
        if (game_frame(game)) session->game_presented_count++;
        if (game->completion.complete && !game->profile_completion_recorded) {
            game->profile_completion_recorded = 1;
            game_profile_record(&session->profile, game->profile_level_key, game->score-game->level_score_start,
                                game->completion.coins_collected, game->completion.elapsed);
        }
        if (session_apply_game_route(session, callback_owned)) return;
    } else session_end(session, 1);
    if (!session->ended) session_apply_preferences(session);
    if (session->ended && callback_owned) {
        session_emit(session, APP_SESSION_EVENT_SESSION_FREED, NULL);
        session_free_owned(session);
    }
}

void session_frame(void *arg)
{
    AppSession *session = arg;
#ifdef __EMSCRIPTEN__
    session_step(session, 1);
#else
    session_step(session, session && session->hooks.force_callback_mode);
#endif
}

int session_run(AppSession *session)
{
    if (!session) return EXIT_FAILURE;
#ifdef __EMSCRIPTEN__
    if (!session->callback_registered) {
        session->callback_registered = 1;
        session->callback_registration_count++;
        session_emit(session, APP_SESSION_EVENT_CALLBACK_REGISTERED, NULL);
        emscripten_set_main_loop_arg(session_frame, session, 0, 0);
    }
    return EXIT_SUCCESS;
#else
    if (session->hooks.force_callback_mode) {
        if (!session->callback_registered) {
            session->callback_registered = 1;
            session->callback_registration_count++;
            session_emit(session, APP_SESSION_EVENT_CALLBACK_REGISTERED, NULL);
        }
        return EXIT_SUCCESS;
    }
    while (!session->ended) session_step(session, 0);
    return session->fatal ? EXIT_FAILURE : EXIT_SUCCESS;
#endif
}

void session_destroy(AppSession **session)
{
    if (!session || !*session) return;
    AppSession *owned = *session;
    *session = NULL;
    session_cancel_callback(owned);
    session_close_menu(owned);
    session_close_game(owned);
    session_runtime_cleanup(owned);
    session_emit(owned, APP_SESSION_EVENT_SESSION_FREED, NULL);
    session_free_owned(owned);
}
