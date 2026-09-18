#pragma once

#include "../game.h"
#include "../screens/start_menu.h"
#include "game_profile.h"
#include "../screens/settings_menu.h"

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
    APP_SESSION_EVENT_RELOAD_REQUESTED,
    APP_SESSION_EVENT_WEB_INPUT_REPAIRED
} AppSessionLifecycleEvent;

typedef int (*AppSessionReplayStoreFn)(const char *level_path, void *userdata);
typedef void (*AppSessionReloadFn)(const char *level_path, void *userdata);
typedef void (*AppSessionLifecycleFn)(AppSessionLifecycleEvent event,
                                      const char *level_path,
                                      void *userdata);

typedef struct {
    AppSessionReplayStoreFn store_replay;
    AppSessionReloadFn reload;
    AppSessionLifecycleFn lifecycle;
    void *userdata;
    int force_callback_mode; /* narrow native lifecycle-test seam */
} AppSessionHooks;

typedef struct {
    const char *level_path;
    int debug_mode;
    int smoke_test_frames;
    const char *replay_script_path;
    const AppSessionHooks *hooks;
    int profile_enabled; /* opt-in; tests/smoke default to memory-only */
    int continue_last;
    const char *profile_path; /* optional native profile override */
    unsigned int random_seed;
    const char *experiment_path; /* explicit native capture import */
} AppSessionConfig;

typedef struct AppSession {
    StartMenu *menu;
    GameState *game;
    CampaignCatalog catalog; /* owned when a campaign menu is active */
    int catalog_loaded;
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
    int browser_reload_requested;
    int replay_storage_attempts;
    int replay_storage_successes;
    int debug_mode;
    int smoke_test_frames;
    AppSessionHooks hooks;
    char status_message[160];
    char replay_script_path[256];
    char boot_level_path[GAME_LEVEL_PATH_MAX];
    unsigned int random_seed;
} AppSession;

/* Heap-allocate a session and its initial menu or game screen. */
AppSession *session_create(const AppSessionConfig *config);

/* One frame shared by native and web runners. */
void session_frame(void *arg);

/* Native blocks here; web registers this one callback and returns. */
int session_run(AppSession *session);

/* Close active screen, shut runtime down once, free session, null ownership. */
void session_destroy(AppSession **session);
