#pragma once

#include "../game_assets.h"     /* GameAssets, owned here */
#include "../game_constants.h"  /* GAME_LEVEL_PATH_MAX */
#include "../game_fwd.h"        /* GameState, used by pointer only */
#include "../screens/start_menu.h"
#include "game_profile.h"
#include "../screens/settings_menu.h"
#include "../levels/level_start.h"  /* LevelStart: --start-x / --start-checkpoint */

typedef enum {
    APP_SCREEN_NONE = 0,
    APP_SCREEN_MENU,
    APP_SCREEN_GAME,
    APP_SCREEN_ENDED
} AppScreen;

typedef enum {
    APP_ROUTE_NONE = 0,
    APP_ROUTE_MENU_PLAY,
    APP_ROUTE_MENU_EXIT,
    APP_ROUTE_GAME_NEXT_LEVEL,
    APP_ROUTE_GAME_REPLAY,
    APP_ROUTE_GAME_LEVEL_SELECT,
    APP_ROUTE_GAME_EXIT,
    APP_ROUTE_FATAL,
    APP_ROUTE_SMOKE_EXIT
} AppRoute;

typedef enum {
    APP_SESSION_EVENT_CALLBACK_REGISTERED = 0,
    APP_SESSION_EVENT_CALLBACK_CANCELLED,
    APP_SESSION_EVENT_GAME_CLOSED,
    APP_SESSION_EVENT_RUNTIME_CLEANED,
    APP_SESSION_EVENT_SESSION_FREED,
    APP_SESSION_EVENT_WEB_INPUT_REPAIRED
} AppSessionLifecycleEvent;

typedef void (*AppSessionLifecycleFn)(AppSessionLifecycleEvent event,
                                      const char *level_path,
                                      void *userdata);

typedef struct {
    AppSessionLifecycleFn lifecycle;
    void *userdata;
    int force_callback_mode; /* narrow native lifecycle-test seam */
} AppSessionHooks;

typedef struct {
    const char *level_path;
    int debug_mode;
    int smoke_test_frames;
    const char *replay_script_path;
    const char *replay_dir; /* optional folder for replay scripts */
    const AppSessionHooks *hooks;
    int profile_enabled; /* opt-in; tests/smoke default to memory-only */
    int continue_last;
    const char *profile_path; /* optional native profile override */
    unsigned int random_seed;
    const char *experiment_path; /* explicit native capture import */
    LevelStart start; /* where the --level game first starts; kind 0 = its own start */
    /* The campaign manifest; NULL = CAMPAIGN_MANIFEST_PATH. Tests point it
     * at a fixture to get a campaign with an unavailable entry. */
    const char *campaign_path;
} AppSessionConfig;

typedef struct AppSession {
    StartMenu *menu;
    GameState *game;
    /* The sprites and sounds every level uses. Loaded when the first game
     * opens, copied into each GameState (game->assets, borrowed) and
     * unloaded once, with the window and audio device, so Replay and Play
     * reuse them. */
    GameAssets assets;
    int assets_loaded;
    CampaignCatalog catalog; /* owned once loaded (menu, Next Level, --continue) */
    int catalog_loaded;
    char campaign_path[GAME_LEVEL_PATH_MAX]; /* the manifest catalog is read from */
    GameProfile profile;
    SettingsMenu settings;
    unsigned int applied_settings_revision;
    unsigned int attempted_save_revision;
    int preferences_applied;
    int settings_were_open;
    AppScreen screen;
    AppRoute route;
    int ended;
    int fatal;
    int runtime_cleaned;
    int runtime_cleanup_count;
    int menu_close_count;
    int game_close_count;
    int callback_registered;
    int callback_cancelled;
    int callback_registration_count;
    int callback_cancellation_count;
    int web_input_repair_count;
    int menu_presented_count;
    int game_presented_count;
    int game_open_count;
    int debug_mode;
    int smoke_test_frames;
    AppSessionHooks hooks;
    char status_message[160];
    char replay_script_path[256];
    /* main.c makes a relative --replay-dir absolute, so this holds a whole
     * path, the same size as a level path. */
    char replay_dir[GAME_LEVEL_PATH_MAX];
    char boot_level_path[GAME_LEVEL_PATH_MAX];
    unsigned int random_seed;
    /* Continue-point bookkeeping: the respawn point and pause state seen at
     * the last check, so a new point is recorded only when one changes. */
    float resume_respawn_x, resume_respawn_y;
    int resume_was_paused;
    /* The config's start point, used up by the first game opened (Replay,
     * Level Select and Next Level start levels at their own start). */
    LevelStart start;
} AppSession;

/* Heap-allocate a session and its initial menu or game screen. */
AppSession *session_create(const AppSessionConfig *config);

/* One frame shared by native and web runners. */
void session_frame(void *arg);

/* Native blocks here; web registers this one callback and returns. */
int session_run(AppSession *session);

/* Close active screen, shut runtime down once, free session, null ownership. */
void session_destroy(AppSession **session);
