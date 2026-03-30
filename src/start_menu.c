/*
 * start_menu.c — Start Menu: black screen with centred title text and logo.
 *
 * This is the default entry point.  It renders a black background with
 * "Start Menu" centred on screen and Logo.png at the top centre.
 * The user can press ESC or close the window to exit.
 */

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>

#include "start_menu.h"

/* ---- Constants ------------------------------------------------------ */

/*
 * Logical canvas matches the game's logical resolution so the start menu
 * fills the same viewport without any scaling mismatch.
 */
#define MENU_GAME_W  400
#define MENU_GAME_H  300

/* Logo display size — 2× the 64×48 source for visibility. */
#define LOGO_DISPLAY_W  128
#define LOGO_DISPLAY_H   96

/* ------------------------------------------------------------------ */

void start_menu_init(StartMenu *menu, SDL_Window *window, SDL_Renderer *renderer) {
    menu->window   = window;
    menu->renderer = renderer;
    menu->running  = 1;

    /*
     * Load the same bitmap font used by the HUD (Round9x13.ttf at size 13).
     * Fatal if missing — we need it to render the title text.
     */
    menu->font = TTF_OpenFont("assets/round9x13.ttf", 13);
    if (!menu->font) {
        fprintf(stderr, "Failed to load Round9x13.ttf: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * Load the Logo.png texture (64×48 px source).
     * Non-fatal — the menu can run without the logo.
     */
    menu->logo_tex = IMG_LoadTexture(renderer, "assets/start_menu_logo.png");
    if (!menu->logo_tex) {
        fprintf(stderr, "Warning: Failed to load Logo.png: %s\n", IMG_GetError());
    }
}

/* ------------------------------------------------------------------ */

/*
 * start_menu_frame — Render one frame of the start menu.
 *
 * Extracted so it can be used as an emscripten_set_main_loop_arg callback.
 */
static void start_menu_frame(void *arg) {
    StartMenu *menu = (StartMenu *)arg;
    {
        /* ---- Event polling ----------------------------------------- */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                menu->running = 0;
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                menu->running = 0;
            }
        }

        /* ---- Render ------------------------------------------------ */
        /* Clear to black */
        SDL_SetRenderDrawColor(menu->renderer, 0, 0, 0, 255);
        SDL_RenderClear(menu->renderer);

        /* Draw logo at top centre */
        if (menu->logo_tex) {
            SDL_Rect logo_dst = {
                (MENU_GAME_W - LOGO_DISPLAY_W) / 2,
                20,
                LOGO_DISPLAY_W,
                LOGO_DISPLAY_H
            };
            SDL_RenderCopy(menu->renderer, menu->logo_tex, NULL, &logo_dst);
        }

        /* Draw "Start Menu" centred text */
        {
            SDL_Color white = {255, 255, 255, 255};
            const char *title = "Start Menu";
            SDL_Surface *surf = TTF_RenderText_Solid(menu->font, title, white);
            if (surf) {
                SDL_Texture *tex = SDL_CreateTextureFromSurface(menu->renderer, surf);
                if (tex) {
                    SDL_Rect dst = {
                        (MENU_GAME_W - surf->w) / 2,
                        (MENU_GAME_H - surf->h) / 2,
                        surf->w,
                        surf->h
                    };
                    SDL_RenderCopy(menu->renderer, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        /*
         * Draw hint text below centre: tells the user how to launch the game.
         * Rendered in a dim grey so it doesn't compete with the title.
         */
        {
            SDL_Color grey = {160, 160, 160, 255};
            const char *hint = "run with: make run-sandbox";
            SDL_Surface *surf = TTF_RenderText_Solid(menu->font, hint, grey);
            if (surf) {
                SDL_Texture *tex = SDL_CreateTextureFromSurface(menu->renderer, surf);
                if (tex) {
                    SDL_Rect dst = {
                        (MENU_GAME_W - surf->w) / 2,
                        (MENU_GAME_H - surf->h) / 2 + 20,
                        surf->w,
                        surf->h
                    };
                    SDL_RenderCopy(menu->renderer, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        {
            SDL_Color grey = {160, 160, 160, 255};
            const char *hint2 = "or: make run-sandbox-debug";
            SDL_Surface *surf = TTF_RenderText_Solid(menu->font, hint2, grey);
            if (surf) {
                SDL_Texture *tex = SDL_CreateTextureFromSurface(menu->renderer, surf);
                if (tex) {
                    SDL_Rect dst = {
                        (MENU_GAME_W - surf->w) / 2,
                        (MENU_GAME_H - surf->h) / 2 + 34,
                        surf->w,
                        surf->h
                    };
                    SDL_RenderCopy(menu->renderer, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        SDL_RenderPresent(menu->renderer);

        /* Cap at ~60 FPS to avoid burning CPU (native only) */
#ifndef __EMSCRIPTEN__
        SDL_Delay(16);
#endif
    }
}

/* ------------------------------------------------------------------ */

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

void start_menu_loop(StartMenu *menu) {
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(start_menu_frame, menu, 0, 1);
#else
    while (menu->running) {
        start_menu_frame(menu);
    }
#endif
}

/* ------------------------------------------------------------------ */

void start_menu_cleanup(StartMenu *menu) {
    if (menu->logo_tex) {
        SDL_DestroyTexture(menu->logo_tex);
        menu->logo_tex = NULL;
    }
    if (menu->font) {
        TTF_CloseFont(menu->font);
        menu->font = NULL;
    }
}
