/*
 * game.h — Public interface for the game module.
 *
 * Defines:
 *   - Compile-time constants shared across all files.
 *   - The GameState struct that holds everything the game needs.
 *   - Declarations for the three functions that drive the game.
 */

/*
 * #pragma once is a non-standard but universally supported header guard.
 * It tells the compiler: "include this file only once per translation unit,
 * even if #included multiple times". Prevents duplicate-definition errors.
 */
#pragma once

#include <SDL.h>      /* SDL_Window, SDL_Renderer, SDL_Texture */
#include "player.h"   /* Player struct — embedded by value in GameState */

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define WINDOW_TITLE  "Super Mango"   /* title bar text                */
#define WINDOW_W      800             /* window width  in pixels       */
#define WINDOW_H      600             /* window height in pixels       */
#define TARGET_FPS    60             /* desired frames per second      */

/*
 * TILE_SIZE — display size of one grass tile in pixels.
 * The Grass_Tileset.png is 48×48; we render it at its natural size.
 */
#define TILE_SIZE     48

/*
 * FLOOR_Y — the Y coordinate of the top edge of the floor.
 * Anything at or below this Y is "inside" the floor.
 */
#define FLOOR_Y       (WINDOW_H - TILE_SIZE)

/*
 * GRAVITY — downward acceleration in pixels per second squared.
 * Applied every frame so the player accelerates toward the floor.
 */
#define GRAVITY       800.0f

/* ------------------------------------------------------------------ */
/* GameState — the single source of truth for everything the game owns */
/* ------------------------------------------------------------------ */

typedef struct {
    SDL_Window   *window;      /* the OS window (created by SDL)              */
    SDL_Renderer *renderer;    /* GPU-accelerated 2D drawing context          */
    SDL_Texture  *background;  /* forest background loaded into GPU memory    */
    SDL_Texture  *floor_tile;  /* grass tile repeated across the floor layer  */
    Player        player;      /* the player, stored by value (not a pointer) */
    int           running;     /* loop flag: 1 = keep running, 0 = quit       */
} GameState;

/* ------------------------------------------------------------------ */
/* Function declarations                                               */
/* These tell the compiler "these functions exist; their bodies are    */
/* in game.c". Any file that includes this header can call them.       */
/* ------------------------------------------------------------------ */

/* Create the window, renderer, and load all textures. */
void game_init(GameState *gs);

/* Run the main game loop until gs->running becomes 0. */
void game_loop(GameState *gs);

/* Free every resource owned by the game in reverse-init order. */
void game_cleanup(GameState *gs);
