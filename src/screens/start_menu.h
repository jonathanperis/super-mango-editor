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

#include "../mangolib/massets.h"    /* AssetCache — the menu's three files come from it */
#include "../mangolib/mtext.h"
#include "../mangolib/maudio.h"

#include "../input/game_input.h"
#include "../levels/level_session.h"

/*
 * MenuRoute — request consumed by AppSession after a frame.
 */
typedef enum {
    MENU_ROUTE_NONE = 0,
    MENU_ROUTE_PLAY,
    MENU_ROUTE_CONTINUE,  /* play the selected level from its Continue point */
    MENU_ROUTE_EXIT,
    MENU_ROUTE_FATAL
} MenuRoute;

typedef MenuRoute MenuResult;

/* The two files the menu loads; AppSession holds them between menus. */
#define START_MENU_LOGO_PATH    "assets/sprites/screens/start_menu_logo.png"
#define START_MENU_CONFIRM_PATH "assets/sounds/screens/confirm_ui.wav"

/*
 * StartMenu — resources and state for the start menu screen.
 *
 * The menu owns its render target. Its font, logo and confirm sound are
 * held from the asset cache, one reference each, taken in start_menu_init
 * and given back in start_menu_cleanup.
 */
typedef struct {
    RenderTexture2D frame_target;
    AssetCache *cache;        /* borrowed from AppSession; NULL = load directly */
    TextFont *font;           /* held from the asset cache */
    Texture2D *logo_tex;      /* held from the asset cache */
    SoundEffect *snd_confirm; /* held from the asset cache */
    int controller; /* raylib device index + 1 */
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

/*
 * StartMenuLevelRow — what the Level Select list shows for one campaign
 * entry: its name and the player's best results from the profile. Building
 * the text apart from drawing it lets the tests read exactly what a row says.
 */
typedef struct {
    char name[CAMPAIGN_DISPLAY_NAME_SIZE];
    char time[16];   /* best time as m:ss.cc ("1:05.25"), "--" before a clear */
    char coins[16];  /* best coins of the level's total ("5/12"), "--" before */
    int  cleared;    /* 1 = the profile records a finished run of this level */
    int  available;  /* 0 = broken entry: drawn greyed out, cannot be played */
} StartMenuLevelRow;

/* How many list rows fit between the logo and the Play button. */
#define START_MENU_LIST_ROWS 5

/* Fill row for catalog entry index (index must be < catalog->count). */
void start_menu_level_row(const StartMenu *menu, size_t index, StartMenuLevelRow *row);

/* 1 when the profile holds a Continue point for the selected level: the
 * menu then shows Continue beside Play (C key / gamepad X). */
int start_menu_can_continue(const StartMenu *menu);

/* Write seconds as m:ss.cc, rounded to hundredths ("--" when negative). */
void start_menu_format_time(float seconds, char *out, size_t size);

/* Initialise the start menu: create its canvas and get the font, logo and
 * confirm sound from menu->cache. */
int start_menu_init(StartMenu *menu);

/* Allocate, initialise, and own a complete menu screen. Its font, logo and
 * confirm sound come from cache, which must outlive the menu; NULL loads
 * them for this menu alone. */
StartMenu *start_menu_create(const CampaignCatalog *catalog, AssetCache *cache);

/* Show a recoverable menu load error without losing the current selection. */
void start_menu_set_error(StartMenu *menu, const char *message);

/* Snapshot controls before the menu controller is closed during a route. */
void start_menu_get_input_state(const StartMenu *menu,
                                GameInputPhysicalState *state);

/* Discover the first available raylib gamepad. */
void start_menu_refresh_controller(StartMenu *menu);

/* Execute one menu frame. Returns 1 only after presentation. */
int start_menu_frame(StartMenu *menu);

/* Free the canvas and give the font, logo and confirm sound back to the
 * cache; a confirm sound still playing is cut off. */
void start_menu_cleanup(StartMenu *menu);

/* Close and free a menu screen, nulling caller ownership. */
void start_menu_close(StartMenu **menu);
