/*
 * start_menu.h — Public interface for the Start Menu screen.
 *
 * The start menu is the default entry point when the game launches.
 * It renders a black screen with "Start Menu" centred text and the
 * game logo at the top centre.  No gameplay occurs here — it is a
 * placeholder for future menu functionality.
 */
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

/*
 * StartMenu — resources and state for the start menu screen.
 *
 * window   : the OS window (shared with the game, created externally).
 * renderer : GPU drawing context (shared with the game).
 * font     : the bitmap font used for the title text.
 * logo_tex : the Logo.png texture displayed at the top centre.
 * running  : 1 = menu loop is active, 0 = quit/exit.
 */
typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;
    SDL_Texture  *logo_tex;
    int           running;
} StartMenu;

/* Initialise the start menu: load font and logo. */
void start_menu_init(StartMenu *menu, SDL_Window *window, SDL_Renderer *renderer);

/* Run the start menu loop until the user quits (ESC or window close). */
void start_menu_loop(StartMenu *menu);

/* Release all start menu resources. */
void start_menu_cleanup(StartMenu *menu);
