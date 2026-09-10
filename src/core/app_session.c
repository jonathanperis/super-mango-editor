/* Heap-owned application state machine. Only this module owns the app loop. */

#include "app_session.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../input/game_web_input.h"
#include "../input/game_input.h"
#include "../levels/level_session.h"
#include "../levels/level_path.h"
#include "game_overlay.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(int, session_browser_store_replay_js, (const char *level_path), {
    var level = UTF8ToString(level_path);
    try {
        sessionStorage.setItem("super-mango-replay-level", level);
        return 1;
    } catch (e) {
        console.error("Super Mango: unable to persist replay level", e);
        return 0;
    }
});

EM_JS(void, session_browser_reload, (const char *level_path), {
    void level_path;
    window.location.reload();
});

EM_JS(void, session_browser_ended, (int fatal), {
    if (typeof Module.onGameEnded === "function") Module.onGameEnded(fatal);
});
#endif

static void copy_path(char *out, size_t out_size, const char *path)
{
    if (!out || out_size == 0) return;
    if (!path) path = "";
    strncpy(out, path, out_size - 1);
    out[out_size - 1] = '\0';
}

static int session_load_catalog(AppSession *session)
{
    if (!session) return -1;
    if (session->catalog_loaded) return 0;
    if (campaign_catalog_load(CAMPAIGN_MANIFEST_PATH, &session->catalog) != 0) {
        copy_path(session->status_message, sizeof(session->status_message),
                  "Campaign manifest unavailable");
        return -1;
    }
    session->catalog_loaded = 1;
    return 0;
}

static void session_free_owned(AppSession *session)
{
    if (!session) return;
    game_profile_close(&session->profile);
    campaign_catalog_cleanup(&session->catalog);
    session->catalog_loaded = 0;
    free(session);
}

static void session_emit(AppSession *session, AppSessionLifecycleEvent event,
                         const char *path)
{
    if (session && session->hooks.lifecycle) {
        session->hooks.lifecycle(event, path, session->hooks.userdata);
    }
}

static void session_flush_input(void)
{
    SDL_FlushEvents(SDL_KEYDOWN, SDL_KEYUP);
    SDL_FlushEvents(SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP);
    SDL_FlushEvents(SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERAXISMOTION);
}

static void session_repair_web_input(AppSession *session)
{
    game_web_input_flush_stale_keys();
    if (session) {
        session->web_input_repair_count++;
        session_emit(session, APP_SESSION_EVENT_WEB_INPUT_REPAIRED, NULL);
    }
}

static int session_controller_is_pending(const AppSession *session)
{
    return session && (session->controller_init_state == APP_CONTROLLER_INIT_PENDING ||
                       session->controller_init_state == APP_CONTROLLER_INIT_RUNNING);
}

static void session_controller_sync_screens(AppSession *session)
{
    if (!session) return;
    if (session->game) {
        game_input_set_controller_init_pending(
            session->game, session_controller_is_pending(session));
    }
    if (session->menu) {
        start_menu_set_controller_ready(
            session->menu, session->controller_init_state == APP_CONTROLLER_INIT_READY);
    }
}

static void session_controller_publish(AppSession *session, int result)
{
    if (!session || session->controller_init_state != APP_CONTROLLER_INIT_RUNNING)
        return;

    if (result == 0) {
        session->controller_subsystem_owned = 1;
        session->controller_init_state = APP_CONTROLLER_INIT_READY;
        session->controller_subsystem_ready_count++;
        session_controller_sync_screens(session);
        session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_READY, NULL);
    } else {
        session->controller_init_state = APP_CONTROLLER_INIT_FAILED;
        session->controller_init_failure_count++;
        session_controller_sync_screens(session);
        fprintf(stderr, "Warning: SDL_INIT_GAMECONTROLLER failed — "
                        "gamepad support unavailable\n");
        session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_FAILED, NULL);
    }
}

#ifndef __EMSCRIPTEN__
static int session_controller_init_worker(void *data)
{
    AppSession *session = (AppSession *)data;
    int result = session->hooks.controller_init(session->hooks.userdata);

    SDL_AtomicSet(&session->controller_init_result, result);
    SDL_AtomicSet(&session->controller_init_done, 1);
    return 0;
}
#endif

static void session_controller_join(AppSession *session)
{
    int result;

    if (!session || !session->controller_init_thread) return;
    SDL_WaitThread(session->controller_init_thread, NULL);
    session->controller_init_thread = NULL;
    session->controller_init_join_count++;
    result = SDL_AtomicGet(&session->controller_init_result);
    session_controller_publish(session, result);
}

static int session_controller_worker_running(const AppSession *session)
{
    return session && session->controller_init_state == APP_CONTROLLER_INIT_RUNNING &&
           session->controller_init_thread != NULL;
}

static void session_controller_poll(AppSession *session)
{
#ifndef __EMSCRIPTEN__
    if (!session || session->controller_init_state != APP_CONTROLLER_INIT_RUNNING ||
        !session->controller_init_thread ||
        !SDL_AtomicGet(&session->controller_init_done))
        return;
    session_controller_join(session);
#else
    (void)session;
#endif
}

static void session_controller_schedule(AppSession *session)
{
#ifndef __EMSCRIPTEN__
    if (!session || session->controller_init_state != APP_CONTROLLER_INIT_PENDING)
        return;

    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_AtomicSet(&session->controller_init_result, 0);
        SDL_AtomicSet(&session->controller_init_done, 1);
        session->controller_init_state = APP_CONTROLLER_INIT_READY;
        session->controller_subsystem_ready_count++;
        session_controller_sync_screens(session);
        session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_READY, NULL);
        return;
    }

    session->controller_init_state = APP_CONTROLLER_INIT_RUNNING;
    session->controller_init_schedule_count++;
    SDL_AtomicSet(&session->controller_init_done, 0);
    SDL_AtomicSet(&session->controller_init_result, -1);
    if (!session->hooks.controller_init) {
        /* SDL2-compat/SDL3 HID backends retain the initializing thread's run
         * loop. Keep real init and teardown on the app thread, but only after
         * the first screen has been presented. Test hooks can still model a
         * deferred operation without making SDL calls from their worker. */
        int result = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
        SDL_AtomicSet(&session->controller_init_result, result);
        SDL_AtomicSet(&session->controller_init_done, 1);
        session_controller_publish(session, result);
        if (session->game) session->game->loop.prev_ticks = SDL_GetTicks64();
        return;
    }
    session->controller_init_thread = SDL_CreateThread(
        session_controller_init_worker, "controller_init", session);
    if (!session->controller_init_thread) {
        session->controller_init_state = APP_CONTROLLER_INIT_FAILED;
        SDL_AtomicSet(&session->controller_init_result, -1);
        SDL_AtomicSet(&session->controller_init_done, 1);
        session->controller_init_failure_count++;
        session_controller_sync_screens(session);
        fprintf(stderr, "Warning: could not start gamepad init thread: %s — "
                        "gamepad support unavailable\n", SDL_GetError());
        session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_FAILED, NULL);
    }
#else
    (void)session;
#endif
}

static int session_runtime_init(AppSession *session)
{
    if (!session) return -1;

#ifdef __EMSCRIPTEN__
    /* Web builds have no native worker path; synchronous init is permitted. */
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
            SDL_AtomicSet(&session->controller_init_result, -1);
            SDL_AtomicSet(&session->controller_init_done, 1);
            session->controller_init_state = APP_CONTROLLER_INIT_FAILED;
            session->controller_init_failure_count++;
            fprintf(stderr, "Warning: SDL_INIT_GAMECONTROLLER failed: %s — "
                            "gamepad support unavailable\n", SDL_GetError());
            session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_FAILED, NULL);
        } else {
            SDL_AtomicSet(&session->controller_init_result, 0);
            SDL_AtomicSet(&session->controller_init_done, 1);
            session->controller_subsystem_owned = 1;
            session->controller_init_state = APP_CONTROLLER_INIT_READY;
            session->controller_subsystem_ready_count++;
            session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_READY, NULL);
        }
    } else {
        SDL_AtomicSet(&session->controller_init_result, 0);
        SDL_AtomicSet(&session->controller_init_done, 1);
        session->controller_init_state = APP_CONTROLLER_INIT_READY;
        session->controller_subsystem_ready_count++;
        session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_READY, NULL);
    }
#else
    /* Native creation defers real subsystem init until after first present. */
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_AtomicSet(&session->controller_init_result, 0);
        SDL_AtomicSet(&session->controller_init_done, 1);
        session->controller_init_state = APP_CONTROLLER_INIT_READY;
        session->controller_subsystem_ready_count++;
        session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_READY, NULL);
    } else {
        session->controller_init_state = APP_CONTROLLER_INIT_PENDING;
    }
#endif
    return 0;
}

static void session_controller_cleanup(AppSession *session)
{
    if (!session) return;
    session_controller_join(session);
    if (session->controller_subsystem_owned) {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
        session->controller_subsystem_owned = 0;
        session->controller_subsystem_closed_count++;
        session_emit(session, APP_SESSION_EVENT_CONTROLLER_SUBSYSTEM_CLOSED, NULL);
    }
}

static void session_runtime_cleanup(AppSession *session)
{
    if (!session || session->runtime_cleaned) return;
    if (session->profile.dirty && game_profile_save(&session->profile)) SDL_Log("%s", session->profile.status);
    session->runtime_cleaned = 1;
    session->runtime_cleanup_count++;
    session_controller_cleanup(session);
    Mix_HaltChannel(-1);
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    session_emit(session, APP_SESSION_EVENT_RUNTIME_CLEANED, NULL);
}

static void session_cancel_callback(AppSession *session)
{
    if (!session || !session->callback_registered || session->callback_cancelled)
        return;
    session->callback_cancelled = 1;
    session->callback_cancellation_count++;
    session_emit(session, APP_SESSION_EVENT_CALLBACK_CANCELLED, NULL);
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
}

static void session_close_menu(AppSession *session)
{
    if (!session || !session->menu) return;
    settings_menu_cleanup(&session->settings);
    start_menu_close(&session->menu);
    session->menu_close_count++;
}

static void session_close_game(AppSession *session)
{
    GameState *game;

    if (!session || !session->game) return;
    settings_menu_cleanup(&session->settings);
    game = session->game;
    session->game = NULL;
    game_cleanup(game);
    free(game);
    session->game_close_count++;
    session_emit(session, APP_SESSION_EVENT_GAME_CLOSED, NULL);
}

static void session_profile_key(GameState *game, const char *source)
{
    game->profile_level_key[0] = '\0';
    if (game_profile_key_valid(source)) {
        copy_path(game->profile_level_key, sizeof(game->profile_level_key), source);
        return;
    }
    const char *base = strrchr(game->level_path, '/');
    const char *back = strrchr(game->level_path, '\\');
    if (!base || (back && back > base)) base = back;
    char key[PROFILE_LEVEL_PATH], resolved[GAME_LEVEL_PATH_MAX];
    int length = snprintf(key, sizeof(key), "levels/%s", base ? base + 1 : game->level_path);
    if (length > 0 && (size_t)length < sizeof(key) && game_profile_key_valid(key) &&
        level_resolve_path(key, resolved, sizeof(resolved)) == 0 && !strcmp(resolved, game->level_path))
        copy_path(game->profile_level_key, sizeof(game->profile_level_key), key);
}

static void session_apply_preferences(AppSession *session)
{
    GameSettings *s = &session->profile.data.settings;
    SDL_Window *window = session->game ? session->game->window : session->menu ? session->menu->window : NULL;
    if (!session->settings.open &&
        (!session->preferences_applied || session->applied_settings_revision != session->profile.revision)) {
        int volume = s->muted ? 0 : s->effects_volume;
        if (session->game) {
            GameState *game = session->game;
#define VOLUME(member) if (game->audio.member) Mix_VolumeChunk(game->audio.member, volume)
            VOLUME(coin); VOLUME(jump); VOLUME(hit); VOLUME(spring);
            VOLUME(axe); VOLUME(flap); VOLUME(spider_attack); VOLUME(dive);
#undef VOLUME
            const LevelDef *level = game->runtime.current_level;
            Mix_VolumeMusic(s->muted ? 0 : level->music_volume * s->music_volume / 128);
        }
        if (session->menu && session->menu->snd_confirm) Mix_VolumeChunk(session->menu->snd_confirm, volume);
#ifndef __EMSCRIPTEN__
        if (window) {
            int w, h; SDL_GetWindowSize(window, &w, &h);
            if (w != GAME_W*s->window_scale || h != GAME_H*s->window_scale)
                SDL_SetWindowSize(window, GAME_W*s->window_scale, GAME_H*s->window_scale);
        }
#else
        (void)window;
#endif
        session->preferences_applied = 1;
        session->applied_settings_revision = session->profile.revision;
    }
    if (session->settings.open != session->settings_were_open) {
        if (session->settings.open) Mix_PauseMusic();
        else if (!session->game || !session->game->paused) Mix_ResumeMusic();
        session->settings_were_open = session->settings.open;
    }
    if (!session->settings.open && session->profile.dirty &&
        session->attempted_save_revision != session->profile.revision) {
        session->attempted_save_revision = session->profile.revision;
        if (game_profile_save(&session->profile)) SDL_Log("%s", session->profile.status);
    }
}

static GameState *session_make_game(AppSession *session, const char *path,
                                    const GameInputPhysicalState *inherited)
{
    GameState *game;

    SDL_AtomicSet(&session->game_open_in_progress, 1);
    game = calloc(1, sizeof(*game));

    if (!game) {
        SDL_AtomicSet(&session->game_open_in_progress, 0);
        return NULL;
    }
    game->debug_mode = session->debug_mode;
    game->smoke_test_frames = session->smoke_test_frames;
    game_input_set_controller_init_pending(
        game, session_controller_is_pending(session));
    copy_path(game->level_path, sizeof(game->level_path), path);
    copy_path(game->replay_script_path, sizeof(game->replay_script_path),
              session->replay_script_path);

    if (game_init(game) != 0) {
        free(game); /* game_init already cleaned partial resources */
        SDL_AtomicSet(&session->game_open_in_progress, 0);
        return NULL;
    }
    game->loop.prev_ticks = SDL_GetTicks64();
    game->profile = &session->profile;
    game->settings_menu = &session->settings;
    session_profile_key(game, path);
    game_profile_select(&session->profile, game->profile_level_key);
    session->preferences_applied = 0;
    game->loop.fp_prev_riding = -1;
    session_repair_web_input(session);
    game_input_set_controller_init_pending(game,
                                           session_controller_is_pending(session));
    game_input_arm_release_latch(game, inherited);
    session->game_open_count++;
    SDL_AtomicSet(&session->game_open_in_progress, 0);
    return game;
}

static int session_open_game(AppSession *session, const char *path,
                             const GameInputPhysicalState *inherited)
{
    if (!session || !path || !path[0]) return -1;
    session->game = session_make_game(session, path, inherited);
    if (!session->game) return -1;
    session->screen = APP_SCREEN_GAME;
    return 0;
}

static int session_open_menu(AppSession *session)
{
    if (!session) return -1;
    if (session_load_catalog(session) != 0) return -1;
    session_flush_input();
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
    session_controller_sync_screens(session);
    return 0;
}

static void session_end(AppSession *session, int fatal)
{
    if (!session || session->ended) return;
    session->fatal |= fatal;
    session_cancel_callback(session);
    /* Never tear down a screen while its controller-init worker can touch SDL. */
    session_controller_join(session);
    session_close_menu(session);
    session_close_game(session);
    session->screen = APP_SCREEN_ENDED;
    if (fatal) session->route = APP_ROUTE_FATAL;
    session->ended = 1;
    session_runtime_cleanup(session);
#ifdef __EMSCRIPTEN__
    session_browser_ended(fatal);
#endif
}

static void session_apply_menu_route(AppSession *session)
{
    MenuRoute route;
    char selected_level[sizeof(session->boot_level_path)];

    if (!session || !session->menu || session_controller_worker_running(session)) return;
    route = session->menu->route;
    if (route == MENU_ROUTE_NONE) return;

    if (route == MENU_ROUTE_PLAY) {
        GameInputPhysicalState inherited;
        GameState *candidate;

        session->route = APP_ROUTE_MENU_PLAY;
        copy_path(selected_level, sizeof(selected_level),
                  session->menu->selected_level_path);
        start_menu_get_input_state(session->menu, &inherited);
        candidate = session_make_game(session, selected_level, &inherited);
        if (!candidate) {
            copy_path(session->status_message, sizeof(session->status_message),
                      "Selected level could not be loaded");
            start_menu_set_error(session->menu, session->status_message);
            session->menu->route = MENU_ROUTE_NONE;
            session->route = APP_ROUTE_NONE;
        } else {
            session_close_menu(session);
            session->game = candidate;
            session->screen = APP_SCREEN_GAME;
            session->route = APP_ROUTE_NONE;
        }
    } else if (route == MENU_ROUTE_EXIT) {
        session->route = APP_ROUTE_MENU_EXIT;
        session_end(session, 0);
    } else {
        session->route = APP_ROUTE_FATAL;
        session_end(session, 1);
    }
}

#ifdef __EMSCRIPTEN__
static int session_browser_store_replay(AppSession *session, const char *path)
{
    if (session->hooks.store_replay)
        return session->hooks.store_replay(path, session->hooks.userdata);
    return session_browser_store_replay_js(path);
}
#else
static int session_browser_store_replay(AppSession *session, const char *path)
{
    if (!session->hooks.store_replay) return 0;
    return session->hooks.store_replay(path, session->hooks.userdata);
}
#endif

static void session_release_after_callback(AppSession *session)
{
    if (!session) return;
    session_emit(session, APP_SESSION_EVENT_SESSION_FREED, NULL);
    session_free_owned(session);
}

static int session_browser_replay(AppSession *session, const char *path)
{
    AppSessionHooks hooks;

    if (!session || !path || !session_browser_store_replay(session, path)) {
        if (session) {
            session->replay_storage_attempts++;
            copy_path(session->status_message, sizeof(session->status_message),
                      "Replay unavailable: storage failed");
            if (session->game) session->game->route = GAME_ROUTE_NONE;
            session->route = APP_ROUTE_NONE;
            SDL_Log("%s", session->status_message);
        }
        return 0;
    }
    session->replay_storage_attempts++;
    session->replay_storage_successes++;
    hooks = session->hooks;
    session_cancel_callback(session);
    session_close_game(session);
    session->screen = APP_SCREEN_ENDED;
    session->ended = 1;
    session->browser_reload_requested = 1;
    session_runtime_cleanup(session);
    session_emit(session, APP_SESSION_EVENT_SESSION_FREED, path);
    session_free_owned(session); /* callback already cancelled; no future frame can use it */
    if (hooks.reload) {
        hooks.reload(path, hooks.userdata);
    }
#ifdef __EMSCRIPTEN__
    else {
        session_browser_reload(path);
    }
#endif
    return 1;
}

static int session_apply_game_route(AppSession *session, int callback_owned)
{
    GameRoute route;
    char current_level[sizeof(session->boot_level_path)];

    if (!session || !session->game) return 0;
#ifdef __EMSCRIPTEN__
    (void)callback_owned;
#endif
    route = session->game->route;
    if (route == GAME_ROUTE_NONE && !session->game->running) {
        route = GAME_ROUTE_EXIT;
    }
    if (route == GAME_ROUTE_NONE) return 0;
    if (session_controller_worker_running(session)) return 0;
    session->game->route = GAME_ROUTE_NONE;

    switch (route) {
    case GAME_ROUTE_NEXT_LEVEL:
        session->route = APP_ROUTE_GAME_NEXT_LEVEL;
        /* Failure deliberately leaves completion overlay and action focus. */
        copy_path(current_level, sizeof(current_level), session->game->completion.next_phase);
        if (game_load_next_phase(session->game) != 0) {
            session->game->route = GAME_ROUTE_NONE;
            session->route = APP_ROUTE_NONE;
        } else {
            session_profile_key(session->game, current_level);
            game_profile_select(&session->profile, session->game->profile_level_key);
            session->preferences_applied = 0;
            session->game->loop.prev_ticks = SDL_GetTicks64();
            game_input_arm_release_latch(session->game, NULL);
            session->route = APP_ROUTE_NONE;
        }
        break;
    case GAME_ROUTE_REPLAY:
        session->route = APP_ROUTE_GAME_REPLAY;
        copy_path(current_level, sizeof(current_level), session->game->level_path);
#ifdef __EMSCRIPTEN__
        return session_browser_replay(session, current_level);
#else
        if (callback_owned) {
            return session_browser_replay(session, current_level);
        }
        {
            GameInputPhysicalState inherited;
            game_input_read_physical(session->game->controller, &inherited);
            session_close_game(session);
            if (session_open_game(session, current_level, &inherited) != 0)
                session_end(session, 1);
            else session->route = APP_ROUTE_NONE;
        }
#endif
        break;
    case GAME_ROUTE_LEVEL_SELECT:
        session->route = APP_ROUTE_GAME_LEVEL_SELECT;
        if (session_load_catalog(session) != 0) {
            copy_path(session->status_message, sizeof(session->status_message),
                      "Level Select unavailable: campaign manifest failed");
            session->game->route = GAME_ROUTE_NONE;
            session->route = APP_ROUTE_NONE;
            return 0;
        }
        session_close_game(session);
        if (session_open_menu(session) != 0) {
            session_end(session, 1);
        } else {
            session->route = APP_ROUTE_NONE;
        }
        break;
    case GAME_ROUTE_SMOKE_EXIT:
        session->route = APP_ROUTE_SMOKE_EXIT;
        session_end(session, 0);
        break;
    case GAME_ROUTE_FATAL:
        session->route = APP_ROUTE_FATAL;
        session_end(session, 1);
        break;
    case GAME_ROUTE_EXIT:
    default:
        session->route = APP_ROUTE_GAME_EXIT;
        session_end(session, 0);
        break;
    }
    return 0;
}

AppSession *session_create(const AppSessionConfig *config)
{
    AppSession *session = calloc(1, sizeof(*session));
    const char *level_path = config ? config->level_path : NULL;

    if (!session) return NULL;
    game_profile_init(&session->profile);
    if ((level_path && strlen(level_path) >= sizeof(session->boot_level_path)) ||
        (config && config->replay_script_path &&
         strlen(config->replay_script_path) >= sizeof(session->replay_script_path))) {
        fprintf(stderr, "Error: startup path exceeds supported storage\n");
        free(session);
        return NULL;
    }
    session->debug_mode = config ? config->debug_mode : 0;
    session->smoke_test_frames = config ? config->smoke_test_frames : 0;
    if (config && config->hooks) session->hooks = *config->hooks;
    SDL_AtomicSet(&session->controller_init_done, 0);
    SDL_AtomicSet(&session->controller_init_result, -1);
    copy_path(session->replay_script_path, sizeof(session->replay_script_path),
              config ? config->replay_script_path : NULL);
    SDL_AtomicSet(&session->game_open_in_progress, 0);

    if (session_runtime_init(session) != 0) {
        session_free_owned(session);
        return NULL;
    }

    if (config && config->profile_enabled && !session->smoke_test_frames && !session->replay_script_path[0]) {
        if (game_profile_open(&session->profile, config->profile_path)) SDL_Log("%s", session->profile.status);
    } else copy_path(session->profile.status, sizeof(session->profile.status), "Saving disabled; settings apply to this run.");
    if (!level_path && config && config->continue_last && session->profile.data.last_level[0])
        level_path = session->profile.data.last_level;

    if (level_path && level_path[0]) {
        copy_path(session->boot_level_path, sizeof(session->boot_level_path), level_path);
        if (session_open_game(session, session->boot_level_path, NULL) != 0) {
            session_controller_cleanup(session);
            session_free_owned(session);
            return NULL;
        }
    } else if (session_open_menu(session) != 0) {
        session_controller_cleanup(session);
        session_free_owned(session);
        return NULL;
    }
    return session;
}

static void session_step(AppSession *session, int callback_owned)
{
    int frame_presented = 0;
    int route_requested = 0;

    if (!session || session->ended) return;
    session_apply_preferences(session);
    session_controller_poll(session);
    if (session->screen == APP_SCREEN_MENU) {
        StartMenu *menu = session->menu;

        if (menu) {
            menu->route_waiting_render = session_controller_worker_running(session);
        }
        frame_presented = start_menu_frame(menu);
        if (menu) menu->route_waiting_render = 0;
        route_requested = menu && menu->route != MENU_ROUTE_NONE;
        if (frame_presented) session->menu_presented_count++;
        session_apply_menu_route(session);
        if (frame_presented && !route_requested && session->screen == APP_SCREEN_MENU &&
            session->menu == menu) {
            session_controller_schedule(session);
        }
    } else if (session->screen == APP_SCREEN_GAME) {
        GameState *game = session->game;

        frame_presented = game_frame(game);
        route_requested = game &&
            (game->route != GAME_ROUTE_NONE || !game->running);
        if (frame_presented) session->game_presented_count++;
        if (game && game->completion.complete && !game->profile_completion_recorded) {
            game->profile_completion_recorded = 1;
            (void)game_profile_record(&session->profile, game->profile_level_key,
                                      game->score - game->level_score_start,
                                      game->completion.coins_collected, game->completion.elapsed);
        }
        if (session_apply_game_route(session, callback_owned)) return;
        if (frame_presented && !route_requested && session->screen == APP_SCREEN_GAME &&
            session->game == game) {
            session_controller_schedule(session);
        }
    } else {
        session_end(session, 1);
    }

    if (!session->ended) session_apply_preferences(session);

    if (session->ended && callback_owned) {
        session_release_after_callback(session);
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
    while (!session->ended) {
        /* The native caller retains ownership until session_destroy. Hooks
         * cannot silently switch this frame to callback-owned teardown. */
        session_step(session, 0);
        if (session->screen == APP_SCREEN_MENU) SDL_Delay(16);
    }
    return session->fatal ? EXIT_FAILURE : EXIT_SUCCESS;
#endif
}

void session_destroy(AppSession **session)
{
    AppSession *owned;

    if (!session || !*session) return;
    owned = *session;
    *session = NULL;
    session_cancel_callback(owned);
    /* Join before closing menu/game; controller worker may still be in SDL. */
    session_controller_join(owned);
    session_close_menu(owned);
    session_close_game(owned);
    session_runtime_cleanup(owned);
    session_emit(owned, APP_SESSION_EVENT_SESSION_FREED, NULL);
    session_free_owned(owned);
}
