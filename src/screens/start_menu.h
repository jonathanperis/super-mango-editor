/*
 * start_menu.h — Public interface for the Start Menu screen.
 *
 * The start menu is the first screen the player sees.  It shows the game
 * logo and a "Play" button.  Pressing Play transitions to the game.
 * Pressing ESC or closing the window exits.
 *
 * The menu communicates its result to main.c via the `result` field:
 *   MENU_QUIT  — user closed the window or pressed ESC
 *   MENU_PLAY  — user clicked Play or pressed Enter/Space
 */
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

/*
 * MenuResult — what the user chose in the start menu.
 * Checked by main.c after start_menu_loop returns.
 */
typedef enum {
    MENU_QUIT = 0,   /* exit the application */
    MENU_PLAY = 1    /* start the game       */
} MenuResult;

/*
 * StartMenu — resources and state for the start menu screen.
 */
typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;
    SDL_Texture  *logo_tex;
    int           running;    /* 1 = menu loop active, 0 = done   */
    MenuResult    result;     /* what the user chose               */
} StartMenu;

/* Initialise the start menu: load font and logo. */
void start_menu_init(StartMenu *menu, SDL_Window *window, SDL_Renderer *renderer);

/* Run the start menu loop until the user quits or clicks Play. */
void start_menu_loop(StartMenu *menu);

/* Release all start menu resources. */
void start_menu_cleanup(StartMenu *menu);
