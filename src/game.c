/*
 * game.c — Window, renderer, background, and main game loop.
 */

#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>

#include "game.h"
#include "player.h"

/* ------------------------------------------------------------------ */

/*
 * game_init — Set up everything the game needs before the loop starts.
 *
 * Called once at startup. Creates the OS window, the GPU renderer,
 * loads the background image, and initialises the player.
 */
void game_init(GameState *gs) {
    /*
     * SDL_CreateWindow — ask the OS to create a window.
     *   WINDOW_TITLE            → text shown in the title bar
     *   SDL_WINDOWPOS_CENTERED  → center the window on the monitor (x and y)
     *   WINDOW_W / WINDOW_H     → 800×600 pixels
     *   SDL_WINDOW_SHOWN        → make it visible immediately
     */
    gs->window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN
    );
    if (!gs->window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    /*
     * SDL_CreateRenderer — create a 2D drawing context tied to the window.
     *   -1 → let SDL pick the best available GPU driver automatically
     *   SDL_RENDERER_ACCELERATED  → use the GPU (not software fallback)
     *   SDL_RENDERER_PRESENTVSYNC → lock present() to the monitor refresh rate
     *                               (prevents screen tearing and runaway CPU)
     */
    gs->renderer = SDL_CreateRenderer(
        gs->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!gs->renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /*
     * SDL_RenderSetLogicalSize — define the internal canvas resolution.
     * All SDL render calls now use GAME_W × GAME_H coordinates (400×300).
     * SDL scales this canvas up to the OS window size (800×600) automatically,
     * giving every pixel a 2×2 block on screen — the chunky pixel-art look.
     */
    SDL_RenderSetLogicalSize(gs->renderer, GAME_W, GAME_H);

    /*
     * IMG_LoadTexture — decode a PNG file and upload it to GPU memory.
     * A "texture" is an image that lives on the GPU and can be drawn very fast.
     * The path is relative to where the binary is run from (repo root).
     */
    /* Load the forest background (stretched to fill the window at render time) */
    gs->background = IMG_LoadTexture(gs->renderer, "assets/Forest_Background_0.png");
    if (!gs->background) {
        fprintf(stderr, "Failed to load Forest_Background_0.png: %s\n", IMG_GetError());
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /*
     * Load the grass tile. This single 48×48 texture will be repeated
     * (tiled) across the full window width to form the floor.
     */
    gs->floor_tile = IMG_LoadTexture(gs->renderer, "assets/Grass_Tileset.png");
    if (!gs->floor_tile) {
        fprintf(stderr, "Failed to load Grass_Tileset.png: %s\n", IMG_GetError());
        SDL_DestroyTexture(gs->background);
        SDL_DestroyRenderer(gs->renderer);
        SDL_DestroyWindow(gs->window);
        exit(EXIT_FAILURE);
    }

    /* Set up the player (loads texture, sets initial position on the floor) */
    player_init(&gs->player, gs->renderer);

    /* Signal the loop to start running */
    gs->running = 1;
}

/* ------------------------------------------------------------------ */

/*
 * game_loop — The heart of the game. Runs until gs->running == 0.
 *
 * Every iteration of the while loop is one "frame". Each frame:
 *   1. Calculate how much time has passed (delta time).
 *   2. Process all pending OS/input events.
 *   3. Update game state (player position, etc.).
 *   4. Draw everything to the screen.
 */
void game_loop(GameState *gs) {
    /*
     * frame_ms — how many milliseconds one frame should take at TARGET_FPS.
     * e.g. 1000ms / 60fps ≈ 16ms per frame.
     * Used as a manual frame cap fallback if VSync is not active.
     */
    const Uint32 frame_ms = 1000 / TARGET_FPS;

    /* Record the timestamp at the start of the very first frame */
    Uint64 prev = SDL_GetTicks64();

    while (gs->running) {
        /*
         * Delta time (dt) — seconds elapsed since the previous frame.
         * SDL_GetTicks64() returns milliseconds since SDL was initialised.
         * Dividing by 1000 converts to seconds (a float like 0.016).
         * We multiply velocities by dt so movement is frame-rate independent.
         */
        Uint64 now = SDL_GetTicks64();
        float  dt  = (float)(now - prev) / 1000.0f;
        prev = now;

        /* ---- 1. Events ------------------------------------------- */
        /*
         * SDL_PollEvent — drain the event queue one event at a time.
         * Events are things the OS tells us about: key presses, mouse clicks,
         * the user clicking the window's close button, etc.
         * Returns 1 while there are events to process, 0 when the queue is empty.
         */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                /* User clicked the window's X button */
                gs->running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                /* A key was pressed this frame */
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    gs->running = 0;
                }
            }
        }

        /* ---- 2. Update ------------------------------------------- */
        /* Read the keyboard and set the player's velocity for this frame */
        player_handle_input(&gs->player);
        /* Move the player based on velocity × dt, then clamp to the window */
        player_update(&gs->player, dt);

        /* ---- 3. Render ------------------------------------------- */
        /*
         * SDL_RenderClear — fill the back buffer with the draw colour.
         * We always clear before drawing to avoid leftover pixels from the
         * previous frame showing through.
         */
        SDL_RenderClear(gs->renderer);

        /*
         * Draw the background stretched to cover the entire logical canvas (GAME_W×GAME_H).
         * SDL_RenderSetLogicalSize handles scaling to the OS window automatically.
         */
        SDL_Rect bg = {0, 0, GAME_W, GAME_H};
        SDL_RenderCopy(gs->renderer, gs->background, NULL, &bg);

        /*
         * Draw the grass floor: repeat the 48×48 tile across the full logical
         * width (GAME_W = 400). All coordinates are in logical space; SDL 2× scales.
         */
        for (int tx = 0; tx < GAME_W; tx += TILE_SIZE) {
            SDL_Rect tile_dst = {tx, FLOOR_Y, TILE_SIZE, TILE_SIZE};
            SDL_RenderCopy(gs->renderer, gs->floor_tile, NULL, &tile_dst);
        }

        /* Draw the player sprite on top of the background and floor */
        player_render(&gs->player, gs->renderer);

        /*
         * SDL_RenderPresent — swap the back buffer to the screen.
         * Everything drawn since RenderClear was on a hidden buffer.
         * This call makes it visible instantly, preventing flicker.
         * With VSync enabled, this call also blocks until the monitor
         * is ready for the next frame (typically ~16ms at 60 Hz).
         */
        SDL_RenderPresent(gs->renderer);

        /*
         * Manual frame cap fallback: if the frame finished before frame_ms ms
         * (e.g. VSync is off), sleep for the remaining time.
         * SDL_Delay sleeps the CPU so we don't burn 100% for no reason.
         */
        Uint64 elapsed = SDL_GetTicks64() - now;
        if (elapsed < frame_ms) {
            SDL_Delay((Uint32)(frame_ms - elapsed));
        }
    }
}

/* ------------------------------------------------------------------ */

/*
 * game_cleanup — Free every resource owned by the game.
 *
 * Always destroy in reverse init order, because later objects may
 * depend on earlier ones (e.g. a texture requires the renderer to exist).
 * After destroying, we set pointers to NULL so accidental double-frees
 * are safe (SDL_Destroy* and free() on NULL are no-ops).
 */
void game_cleanup(GameState *gs) {
    /* Free the player's texture first (also renderer-dependent) */
    player_cleanup(&gs->player);

    if (gs->floor_tile) {
        SDL_DestroyTexture(gs->floor_tile);
        gs->floor_tile = NULL;
    }

    if (gs->background) {
        SDL_DestroyTexture(gs->background);  /* release GPU memory */
        gs->background = NULL;
    }

    if (gs->renderer) {
        SDL_DestroyRenderer(gs->renderer);
        gs->renderer = NULL;
    }

    if (gs->window) {
        SDL_DestroyWindow(gs->window);
        gs->window = NULL;
    }
}
