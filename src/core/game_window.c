/*
 * game_window.c — SDL window and renderer lifecycle helpers.
 */

#include "game_window.h"

#include <stdio.h>

static int game_window_fail(const char *label, const char *detail)
{
    fprintf(stderr, "%s: %s\n", label, detail);
    return -1;
}

int game_window_init(GameState *gs)
{
    gs->window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN
    );
    if (!gs->window) {
        return game_window_fail("SDL_CreateWindow error", SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    gs->renderer = SDL_CreateRenderer(
        gs->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!gs->renderer) {
        gs->renderer = SDL_CreateRenderer(gs->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!gs->renderer) {
        return game_window_fail("SDL_CreateRenderer error", SDL_GetError());
    }

    if (SDL_RenderSetLogicalSize(gs->renderer, GAME_W, GAME_H) < 0) {
        return game_window_fail("SDL_RenderSetLogicalSize error", SDL_GetError());
    }

    return 0;
}

void game_window_cleanup(GameState *gs)
{
    if (gs->renderer) {
        SDL_DestroyRenderer(gs->renderer);
        gs->renderer = NULL;
    }

    if (gs->window) {
        SDL_DestroyWindow(gs->window);
        gs->window = NULL;
    }
}
