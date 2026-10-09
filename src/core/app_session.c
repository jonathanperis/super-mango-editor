/*
 * app_session.c — Own the application, not just one level.
 *
 * main → session_create → session_run → session_destroy is the native path.
 * A browser registers a frame callback instead of blocking in a while loop.
 * Both paths use the same screen routes below.
 *
 * Ownership hierarchy:
 *   session: one window/context, audio device, profile and campaign catalog
 *     screen: menu OR GameState, its render target, textures/fonts/sounds
 *
 * A transition can prepare a candidate game before releasing the old menu.
 * Resources outlive neither their owning screen nor the shared GPU/audio
 * context. Read session_apply_*_route for transitions and session_step for
 * the frame order; low-level drawing belongs to the screens/render modules.
 */
#include "app_session.h"
#include "../shared/platform.h"  /* str_copy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../input/game_web_input.h"
#include "../input/game_input.h"
#include "../levels/level_session.h"
#include "../levels/level_path.h"
#include "game_overlay.h"
#include "game_experiment.h"
#include "game_ghost.h"
#include "game_resume.h"
#include "game_timing.h"
#include "../shared/serializer_io.h"  /* serializer_fingerprint_utf8 */

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
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
    /* Several failure/exit routes meet here. The flag makes teardown happen
     * once even when a caller later destroys an already-ended session. */
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
    /* Detach the owner first, then release its contents and the allocation.
     * game_cleanup releases members; it does not free the GameState itself. */
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
    /* Settings edit a profile in memory. Apply a changed revision after the
     * panel closes, rather than resizing/reconfiguring on every draw call. */
    if (game_profile_poll(&session->profile) < 0) TraceLog(LOG_WARNING, "%s", session->profile.status);
    GameSettings *s = &session->profile.data.settings;
    if (!session->settings.open && (!session->preferences_applied ||
        session->applied_settings_revision != session->profile.revision)) {
        int volume = s->muted ? 0 : s->effects_volume;
        if (session->game) game_audio_apply_settings(session->game);
        if (session->menu) sound_set_volume(session->menu->snd_confirm, volume);
#ifndef __EMSCRIPTEN__
        if (IsWindowReady() && (GetScreenWidth() != GAME_W*s->window_scale || GetScreenHeight() != GAME_H*s->window_scale))
            SetWindowSize(GAME_W*s->window_scale, GAME_H*s->window_scale);
#endif
        session->preferences_applied = 1;
        session->applied_settings_revision = session->profile.revision;
    }
    if (session->settings.open != session->settings_were_open) {
        /* In a game, one predicate decides (pause overlay, focus, settings);
         * the menu has no pause overlay, so only the panel matters there. */
        if (session->game) game_music_sync(session->game);
        else if (session->settings.open) music_pause();
        else music_resume();
        session->settings_were_open = session->settings.open;
    }
    /* A failed save is visible to the user. Retry only after another edit,
     * not every frame; a pending browser save retains its own snapshot. */
    if (!session->settings.open && !session->profile.pending_text && session->profile.dirty &&
        session->attempted_save_revision != session->profile.revision) {
        session->attempted_save_revision = session->profile.revision;
        if (game_profile_save(&session->profile) < 0) TraceLog(LOG_WARNING, "%s", session->profile.status);
    }
}

static int session_profile_ready_to_leave(AppSession *session)
{
    /* Native saves complete synchronously. Browser saves may still hold a
     * Web Lock; keep rendering until their acknowledgement settles. */
    int result = game_profile_poll(&session->profile);
    if (result == PROFILE_SAVE_PENDING) return 0;
    if (session->profile.dirty && session->attempted_save_revision != session->profile.revision) {
        session->attempted_save_revision = session->profile.revision;
        result = game_profile_save(&session->profile);
        if (result < 0) TraceLog(LOG_WARNING, "%s", session->profile.status);
    }
    return result != PROFILE_SAVE_PENDING;
}

/*
 * Continue points (GameResume in game_profile.h).
 *
 * The session records one when the player reaches a new respawn point,
 * when a pause begins (the browser pauses when its tab is hidden, the only
 * warning before a tab is closed), and when the player leaves a level
 * part-way through Exit or Level Select. Finishing the level or losing its
 * last life clears it. Recording only on those events, not every frame,
 * keeps profile writes rare. resume_respawn_x/y and resume_was_paused
 * remember what the last check saw.
 */
static void session_watch_resume(AppSession *session, const GameState *game)
{
    session->resume_respawn_x = game->respawn_x;
    session->resume_respawn_y = game->respawn_y;
    session->resume_was_paused = 0;
}

static void session_track_resume(AppSession *session, const GameState *game, int leaving)
{
    GameProfile *profile = &session->profile;
    /* Labs and editor playtests have no profile key; F8 experiments are
     * debug runs, which never touch the personal profile anyway. */
    if (!game->profile_level_key[0] || game->experiment) return;
    if (game->completion.complete || game->game_over) {
        /* A finished or lost attempt leaves nothing to continue. */
        if (game_profile_resume(profile, game->profile_level_key)) game_profile_clear_resume(profile);
        return;
    }
    int paused = game->paused || game->pause_reasons;
    int moved = game->respawn_x != session->resume_respawn_x || game->respawn_y != session->resume_respawn_y;
    int pause_began = paused && !session->resume_was_paused;
    session->resume_was_paused = paused;
    if (!leaving && !moved && !pause_began) return;
    session->resume_respawn_x = game->respawn_x;
    session->resume_respawn_y = game->respawn_y;
    GameResume resume;
    game_resume_capture(game, &resume);
    if (game_profile_set_resume(profile, &resume))
        TraceLog(LOG_WARNING, "Profile: Continue point for %s was out of range and not saved",
                 game->profile_level_key);
}

/*
 * Put a freshly opened game on its saved Continue point. A point that no
 * longer fits (the level file changed since it was saved) is dropped and
 * the run starts at the level start.
 */
static void session_apply_resume(AppSession *session, GameState *game)
{
    const GameResume *resume = game_profile_resume(&session->profile, game->profile_level_key);
    if (resume && game_resume_apply(game, resume)) {
        TraceLog(LOG_WARNING, "Continue point for %s no longer matches the level; starting from its start",
                 game->profile_level_key);
        game_profile_clear_resume(&session->profile);
    }
    session_watch_resume(session, game);
}

/*
 * Before the menu offers Continue, check the saved point against the level
 * file's current bytes, the same hash game_resume_apply compares. An edited
 * or missing level loses its Continue point here, so the menu never shows a
 * button that would start somewhere else.
 */
static void session_drop_stale_resume(AppSession *session)
{
    const GameResume *resume = &session->profile.data.resume;
    char resolved[GAME_LEVEL_PATH_MAX];
    SerializerFileFingerprint fingerprint;
    if (!resume->path[0]) return;
    if (level_resolve_path(resume->path, resolved, sizeof(resolved)) != 0 ||
        serializer_fingerprint_utf8(resolved, &fingerprint) != 1 ||
        fingerprint.content_hash != resume->level_hash) {
        TraceLog(LOG_WARNING, "Continue point for %s dropped: the level changed", resume->path);
        game_profile_clear_resume(&session->profile);
    }
}

/*
 * Time trial. A game that saves to the personal profile gets a ghost
 * recorder and its level's best run to race. A stored ghost recorded on
 * another version of the level file (a different content hash) is ignored:
 * it would run through walls that have moved. The next finished run then
 * replaces it.
 */
static void session_load_ghost(AppSession *session, GameState *game)
{
    if (!session->profile.enabled || !game->profile_level_key[0]) return;
    if (!game->ghost && game_ghost_begin(game)) return;  /* out of memory: no ghost */
    GameGhostTrack best;
    int found = game_ghost_load(&session->profile, game->profile_level_key, &best);
    if (found < 0)
        TraceLog(LOG_WARNING, "Ghost for %s is unreadable or invalid; ignored", game->profile_level_key);
    else if (found == 1 && best.level_hash != game->source_level_hash) {
        TraceLog(LOG_INFO, "Ghost for %s was recorded on another version of the level; ignored",
                 game->profile_level_key);
        game_ghost_track_free(&best);
    } else if (found == 1) {
        game_ghost_set_best(game, &best);
    }
}

/*
 * A finished run becomes the level's ghost when there is none yet (or only
 * one for another version of the level, which was never loaded) or when it
 * beat the stored ghost's time. A run continued from a saved point, or one
 * longer than GHOST_MAX_STEPS, recorded no whole run and is skipped.
 */
static void session_save_ghost(AppSession *session, GameState *game)
{
    GameGhostTrack run;
    if (!game->ghost || !session->profile.enabled || !session->profile.writable) return;
    if (game_ghost_take_run(game, &run)) return;
    const GameGhostTrack *best = &game->ghost->best;
    if ((best->count == 0 || run.time < best->time) && game_ghost_save(&session->profile, &run))
        TraceLog(LOG_WARNING, "Ghost for %s was not saved (storage full or unavailable)", run.level);
    /* Whatever comes next (Replay, Next Level, Level Select) loads the
     * stored ghost again, so this game does not need the new one. */
    game_ghost_track_free(&run);
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
    copy_path(game->replay_dir, sizeof(game->replay_dir), session->replay_dir);
    if (game_init(game)) {
        free(game);
        return NULL;
    }
    game_timing_restart_clock(game);
    game->profile = &session->profile;
    game->settings_menu = &session->settings;
    /* These two pointers are borrowed from the longer-lived session. The
     * screen must not free them when a replay replaces its GameState. */
    session_profile_key(game, path);
    game_profile_select(&session->profile, game->profile_level_key);
    session->preferences_applied = 0;
    game->loop.fp_prev_riding = -1;
    session_repair_web_input(session);
    /* A confirm held on the old screen cannot immediately jump/confirm on
     * the new one. The latch waits for those physical controls to release. */
    game_input_arm_release_latch(game, inherited);
    session_watch_resume(session, game);
    session_load_ghost(session, game);
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
    session_drop_stale_resume(session);
    session->menu = start_menu_create(&session->catalog);
    if (!session->menu) return -1;
    session->menu->profile = &session->profile;
    session->menu->settings_menu = &session->settings;
    for (size_t i = 0; i < session->catalog.count; i++) {
        if (session->catalog.levels[i].available &&
            !strcmp(session->catalog.levels[i].path, session->profile.data.last_level)) {
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
    if (route == MENU_ROUTE_PLAY || route == MENU_ROUTE_CONTINUE) {
        /* Prepare before commit: a bad level leaves the menu usable, with an
         * error message, rather than destroying the only reachable screen. */
        GameInputPhysicalState inherited;
        session->route = APP_ROUTE_MENU_PLAY;
        start_menu_get_input_state(session->menu, &inherited);
        GameState *candidate = session_make_game(session, session->menu->selected_level_path, &inherited);
        if (!candidate) {
            copy_path(session->status_message, sizeof(session->status_message), "Selected level could not be loaded");
            start_menu_set_error(session->menu, session->status_message);
            session->menu->route = MENU_ROUTE_NONE;
        } else {
            /* Continue starts from the saved point; Play from the start. */
            if (route == MENU_ROUTE_CONTINUE) session_apply_resume(session, candidate);
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

static void session_apply_game_route(AppSession *session)
{
    /* Routes are requests, not nested main loops. Consume them after the
     * screen frame, when its event/update/render code is no longer running. */
    GameState *game = session->game;
    if (!game) return;
    GameRoute route = game->route;
    char path[GAME_LEVEL_PATH_MAX];
    if (route == GAME_ROUTE_NONE && !game->running) route = GAME_ROUTE_EXIT;
    if (route == GAME_ROUTE_NONE) return;
    /* Leaving a level part-way keeps a Continue point; record it before the
     * save below, so Exit writes it to disk with everything else. */
    if (route == GAME_ROUTE_EXIT || route == GAME_ROUTE_LEVEL_SELECT) session_track_resume(session, game, 1);
    /* Leaving the program waits for a pending browser save to settle. Every
     * other route keeps the session, and with it the profile in memory. */
    if (route == GAME_ROUTE_EXIT && !session_profile_ready_to_leave(session)) return;
    game->route = GAME_ROUTE_NONE;
    switch (route) {
    case GAME_ROUTE_NEXT_LEVEL:
        session->route = APP_ROUTE_GAME_NEXT_LEVEL;
        copy_path(path, sizeof(path), game->completion.next_phase);
        if (!game_load_next_phase(game)) {
            session_profile_key(game, path);
            game_profile_select(&session->profile, game->profile_level_key);
            game->resumed = 0;  /* the new level is played from its start */
            session_watch_resume(session, game);
            /* A new level, a new race: restart the recording, load its ghost. */
            game_ghost_restart(game);
            game_ghost_track_free(game->ghost ? &game->ghost->best : NULL);
            session_load_ghost(session, game);
            session->preferences_applied = 0;
            game_timing_restart_clock(game);
            game_input_arm_release_latch(game, NULL);
        } else {
            /* The current level is untouched, so Replay, Level Select and
             * Exit still work. Say what happened, drop the dead Next Level
             * row and focus the first remaining action. */
            game->completion.next_phase_failed = 1;
            game->terminal_action_index = 0;
            copy_path(session->status_message, sizeof(session->status_message), "Next level failed to load");
            TraceLog(LOG_WARNING, "%s: %s", session->status_message, path);
        }
        game->route = GAME_ROUTE_NONE;
        session->route = APP_ROUTE_NONE;
        break;
    case GAME_ROUTE_REPLAY:
        /*
         * Native and browser builds both replace the game in place.
         *
         * Browser Replay used to save the level path in sessionStorage and
         * reload the whole page. That dates from the SDL2 build, where every
         * GameState created its own window and renderer, and the canvas's
         * WebGL context could not be torn down and rebuilt inside the
         * browser's frame callback. Since the raylib port this session owns
         * the one window, GL context and audio device for the whole run; a
         * GameState owns only its render target, textures and sounds, which
         * is exactly what Level Select and Play already swap in place on the
         * web. Staying on the page also keeps the browser's unlocked audio
         * (a reloaded page needs a new click or key press before it may play
         * sound) and the profile held in memory, even an unsaved one. The
         * old game is freed before the new one loads, so the WebAssembly
         * heap reuses its blocks instead of growing on every Replay.
         */
        session->route = APP_ROUTE_GAME_REPLAY;
        /* Keep an independent path before the old GameState is freed. */
        copy_path(path, sizeof(path), game->level_path);
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
            return;
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
}

AppSession *session_create(const AppSessionConfig *config)
{
    /* calloc gives each owned resource slot an empty initial state. This
     * makes the fail label valid after any partially completed startup. */
    AppSession *session = calloc(1, sizeof(*session));
    const char *level = config ? config->level_path : NULL;
    if (!session) return NULL;
    game_profile_init(&session->profile);
    if ((level && strlen(level) >= sizeof(session->boot_level_path)) ||
        (config && config->replay_script_path && strlen(config->replay_script_path) >= sizeof(session->replay_script_path)) ||
        (config && config->replay_dir && strlen(config->replay_dir) >= sizeof(session->replay_dir))) {
        free(session);
        return NULL;
    }
    session->debug_mode = config && (config->debug_mode || config->experiment_path);
    session->random_seed = config ? config->random_seed : 1;
    session->smoke_test_frames = config ? config->smoke_test_frames : 0;
    if (config && config->hooks) session->hooks = *config->hooks;
    copy_path(session->replay_script_path, sizeof(session->replay_script_path), config ? config->replay_script_path : NULL);
    copy_path(session->replay_dir, sizeof(session->replay_dir), config ? config->replay_dir : NULL);
    /* Acquire process resources before a screen loads GPU/audio assets. Tests
     * can supply a context; production creates one here and retains it across
     * menu/game transitions. The game requires an audio device to start. */
    if (!IsWindowReady() && display_open(WINDOW_W, WINDOW_H, WINDOW_TITLE,
        session->smoke_test_frames > 0 || getenv("MANGO_TEST_WINDOW") != NULL)) goto fail;
    input_open(GAME_W, GAME_H);
    if (!IsAudioDeviceReady() && audio_open()) goto fail;
    /* Debug, smoke and replay are experiments, not personal progress. Keep
     * their settings in memory instead of opening the user's profile. */
    if (config && config->profile_enabled && !session->debug_mode && !config->experiment_path &&
        !session->smoke_test_frames && !session->replay_script_path[0]) {
        if (game_profile_open(&session->profile, config->profile_path)) TraceLog(LOG_WARNING, "%s", session->profile.status);
    } else copy_path(session->profile.status, sizeof(session->profile.status), "Saving disabled; settings apply to this run.");
    int continued = 0;  /* the level came from the profile, not the command line */
    if (!level && config && config->continue_last && session->profile.data.last_level[0]) {
        level = session->profile.data.last_level;
        continued = 1;
    }
    if (level && level[0]) {
        copy_path(session->boot_level_path, sizeof(session->boot_level_path), level);
        /* An explicit --level that fails is an error. A remembered stage
         * that no longer loads falls back to the selector instead. */
        if (session_open_game(session, session->boot_level_path, NULL) &&
            (!continued || session_open_menu(session))) goto fail;
        /* --continue also picks the stage up at its saved Continue point. */
        if (continued && session->game) session_apply_resume(session, session->game);
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
    /* EndDrawing captured backend commands at the end of the previous frame.
     * Add sampled focus/gamepad changes, then let the active screen consume
     * the queue. Music decoding must be pumped even while an overlay is up. */
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
            /* A level outside levels/ (a lab, an editor playtest) has no
             * profile key and is simply not recorded. A refusal for a keyed
             * level means a lost result, so say so. */
            if (game->profile_level_key[0] &&
                game_profile_record(&session->profile, game->profile_level_key, game->score-game->level_score_start,
                                    game->completion.coins_collected, game->completion.elapsed) != 0)
                TraceLog(LOG_WARNING, "Profile: result for %s was not recorded (profile full or values out of range)",
                         game->profile_level_key);
            session_save_ghost(session, game);
        }
        session_track_resume(session, game, 0);
        session_apply_game_route(session);
    } else session_end(session, 1);
    if (!session->ended) session_apply_preferences(session);
    if (session->ended && callback_owned) {
        /* The callback owns terminal teardown; a native blocking loop leaves
         * final destruction to main after session_run returns. */
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
        /* fps=0 follows browser animation frames; simulate_infinite_loop=0
         * returns to the host. Keep session alive for subsequent callbacks. */
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
    while (!session->ended)
        session_step(session, 0);
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
