/*
 * start_menu.c — Start Menu: logo, "Play" button, and quit option.
 *
 * This is the default entry point.  It renders a black background with
 * the game logo centred at the top and a "Play" button below it.
 *
 * The user can:
 *   - Click "Play" or press Enter/Space → result = MENU_PLAY
 *   - Press ESC or close the window      → result = MENU_QUIT
 *
 * main.c checks menu.result after the loop exits to decide whether to
 * launch the game or exit the application.
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

/* Play button dimensions and position (centred below the logo) */
#define BTN_W  120
#define BTN_H   28
#define BTN_X  ((MENU_GAME_W - BTN_W) / 2)
#define BTN_Y  170

/* ---- Helpers -------------------------------------------------------- */

/*
 * draw_text_centered — Render a string centred at (cx, y).
 *
 * Creates a surface+texture, draws it, and immediately frees both.
 * Simple and correct for a menu that renders a handful of labels per frame.
 */
static void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font,
                               const char *text, int cx, int y,
                               SDL_Color color) {
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = { cx - surf->w / 2, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

/*
 * point_in_rect — Check if (px, py) is inside the rectangle.
 */
static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

/* ------------------------------------------------------------------ */

void start_menu_init(StartMenu *menu, SDL_Window *window, SDL_Renderer *renderer) {
    menu->window   = window;
    menu->renderer = renderer;
    menu->running  = 1;
    menu->result   = MENU_QUIT;

    /*
     * Load the same bitmap font used by the HUD (Round9x13.ttf at size 13).
     * Fatal if missing — we need it to render button and title text.
     */
    menu->font = TTF_OpenFont("assets/round9x13.ttf", 13);
    if (!menu->font) {
        fprintf(stderr, "Failed to load Round9x13.ttf: %s\n", TTF_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * Load the logo texture (displayed at top centre).
     * Non-fatal — the menu can run without the logo.
     */
    menu->logo_tex = IMG_LoadTexture(renderer, "assets/start_menu_logo.png");
    if (!menu->logo_tex) {
        fprintf(stderr, "Warning: Failed to load start_menu_logo.png: %s\n",
                IMG_GetError());
    }
}

/* ------------------------------------------------------------------ */

/*
 * start_menu_frame — Render one frame of the start menu.
 *
 * Handles input (click on Play button, Enter/Space, ESC, quit) and
 * renders the logo, title, and Play button each frame.
 */
static void start_menu_frame(void *arg) {
    StartMenu *menu = (StartMenu *)arg;

    /* ---- Event polling --------------------------------------------- */
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            menu->result  = MENU_QUIT;
            menu->running = 0;
        }
        if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:
                menu->result  = MENU_QUIT;
                menu->running = 0;
                break;
            case SDLK_RETURN:
            case SDLK_SPACE:
                /*
                 * Enter or Space starts the game — same as clicking Play.
                 * This gives keyboard-only users a fast way to start.
                 */
                menu->result  = MENU_PLAY;
                menu->running = 0;
                break;
            default:
                break;
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            /*
             * Convert screen-space click to logical coordinates.
             *
             * SDL_RenderSetLogicalSize scales from 800×600 window to
             * 400×300 logical. SDL provides a helper, but since our
             * logical size is exactly half the window, dividing by 2
             * is equivalent and simpler.
             *
             * We use SDL_RenderGetScale + manual conversion instead of
             * assuming a fixed 2× ratio, for robustness.
             */
            float sx, sy;
            SDL_RenderGetScale(menu->renderer, &sx, &sy);
            int lx = (int)(e.button.x / sx);
            int ly = (int)(e.button.y / sy);

            if (point_in_rect(lx, ly, BTN_X, BTN_Y, BTN_W, BTN_H)) {
                menu->result  = MENU_PLAY;
                menu->running = 0;
            }
        }
    }

    /* ---- Render ---------------------------------------------------- */

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

    /* Title text below logo */
    {
        SDL_Color white = {255, 255, 255, 255};
        draw_text_centered(menu->renderer, menu->font,
                           "Super Mango", MENU_GAME_W / 2, 130, white);
    }

    /* ---- Play button ---- */
    {
        /*
         * Button hover detection — check if mouse is over the button.
         * We read the current mouse position each frame (not just on click)
         * to provide visual hover feedback.
         */
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        float sx, sy;
        SDL_RenderGetScale(menu->renderer, &sx, &sy);
        int lmx = (int)(mx / sx);
        int lmy = (int)(my / sy);
        int hovering = point_in_rect(lmx, lmy, BTN_X, BTN_Y, BTN_W, BTN_H);

        /* Button background — lighter when hovered */
        SDL_Color btn_color = hovering
            ? (SDL_Color){0x4A, 0x90, 0xD9, 0xFF}   /* blue on hover   */
            : (SDL_Color){0x4D, 0x4D, 0x4D, 0xFF};  /* grey default    */
        SDL_SetRenderDrawColor(menu->renderer,
                               btn_color.r, btn_color.g,
                               btn_color.b, btn_color.a);
        SDL_Rect btn_rect = { BTN_X, BTN_Y, BTN_W, BTN_H };
        SDL_RenderFillRect(menu->renderer, &btn_rect);

        /* Button border */
        SDL_SetRenderDrawColor(menu->renderer, 0xE0, 0xE0, 0xE0, 0xFF);
        SDL_RenderDrawRect(menu->renderer, &btn_rect);

        /* Button label */
        SDL_Color white = {255, 255, 255, 255};
        draw_text_centered(menu->renderer, menu->font,
                           "Play", BTN_X + BTN_W / 2, BTN_Y + 7, white);
    }

    /* Hint text at the bottom */
    {
        SDL_Color grey = {120, 120, 120, 255};
        draw_text_centered(menu->renderer, menu->font,
                           "Press Enter or click Play",
                           MENU_GAME_W / 2, 220, grey);
        draw_text_centered(menu->renderer, menu->font,
                           "ESC to quit",
                           MENU_GAME_W / 2, 238, grey);
    }

    SDL_RenderPresent(menu->renderer);

    /* Cap at ~60 FPS to avoid burning CPU (native only) */
#ifndef __EMSCRIPTEN__
    SDL_Delay(16);
#endif
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
