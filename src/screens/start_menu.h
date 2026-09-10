/*
 * start_menu.h — Public interface for the Start Menu screen.
 *
 * The start menu is the first screen the player sees.  It shows the game
 * logo and a "Play" button.  Pressing Play transitions to the game.
 * Pressing ESC or closing the window exits.
 *
 * The session consumes the explicit route after each frame.  This screen
 * never owns a loop or an Emscripten callback.
 */
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include "../input/game_input.h"
#include "../levels/level_session.h"

/*
 * MenuRoute — request consumed by AppSession after a frame.
 */
typedef enum {
    MENU_ROUTE_NONE = 0,
    MENU_ROUTE_PLAY,
    MENU_ROUTE_EXIT,
    MENU_ROUTE_FATAL
} MenuRoute;

typedef MenuRoute MenuResult;

/*
 * StartMenu — resources and state for the start menu screen.
 */
typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;
    SDL_Texture  *logo_tex;
    Mix_Chunk    *snd_confirm; /* SFX played when Play is clicked   */
    SDL_GameController *controller;
    int controller_ready;
    int           confirm_release_required;
    int           route_waiting_render;
    MenuRoute     route;
    const CampaignCatalog *catalog;     /* AppSession-owned validated catalog */
    int           selected_level;      /* selected campaign level index      */
    char          selected_level_path[256]; /* TOML path selected for Play     */
    char          error_message[160];  /* transient level-load failure       */
    struct GameProfile *profile;
    struct SettingsMenu *settings_menu;
} StartMenu;

/* Initialise the start menu: load font and logo. */
int start_menu_init(StartMenu *menu, SDL_Window *window, SDL_Renderer *renderer);

/* Allocate, initialise, and own a complete menu screen. */
StartMenu *start_menu_create(const CampaignCatalog *catalog);

/* Show a recoverable menu load error without losing the current selection. */
void start_menu_set_error(StartMenu *menu, const char *message);

/* Snapshot controls before the menu controller is closed during a route. */
void start_menu_get_input_state(const StartMenu *menu,
                                GameInputPhysicalState *state);

/* Discover a controller after AppSession publishes subsystem readiness. */
void start_menu_refresh_controller(StartMenu *menu);

/* Publish AppSession controller readiness without initializing the subsystem. */
void start_menu_set_controller_ready(StartMenu *menu, int ready);

/* Execute one menu frame. Returns 1 only after SDL_RenderPresent. */
int start_menu_frame(StartMenu *menu);

/* Release all start menu resources. */
void start_menu_cleanup(StartMenu *menu);

/* Close and free a menu screen, nulling caller ownership. */
void start_menu_close(StartMenu **menu);
