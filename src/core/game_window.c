/*
 * game_window.c — SDL window and renderer lifecycle helpers.
 */

#include "game_window.h"

#include <stdio.h>
#include <stdlib.h>

static void game_window_fail(GameState *gs, const char *label, const char *detail)
{
    fprintf(stderr, "%s: %s\n", label, detail);
    game_cleanup(gs);
    exit(EXIT_FAILURE);
}

void game_window_init(GameState *gs)
{
    gs->window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN
    );
    if (!gs->window) {
        game_window_fail(gs, "SDL_CreateWindow error", SDL_GetError());
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
        game_window_fail(gs, "SDL_CreateRenderer error", SDL_GetError());
    }

    SDL_RenderSetLogicalSize(gs->renderer, GAME_W, GAME_H);
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
