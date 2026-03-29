/*
 * main.c — Entry point for Super Mango.
 *
 * Responsibilities:
 *   1. Boot every SDL subsystem the game needs.
 *   2. Hand control to the game (init → loop → cleanup).
 *   3. Tear every subsystem back down before exiting.
 *
 * The order of init and teardown is intentional:
 *   - SDL core must be first up / last down.
 *   - Each subsystem that succeeds must be shut down if a later one fails,
 *     which is why the cleanup calls "stack up" as we go deeper.
 */

/* SDL2 core: window, renderer, events, input, timing */
#include <SDL.h>
/* SDL2_image: load PNG/JPG/etc. files as textures */
#include <SDL_image.h>
/* SDL2_mixer: audio playback and mixing */
#include <SDL_mixer.h>
/* SDL2_ttf: render TrueType fonts to textures */
#include <SDL_ttf.h>
/* Standard C I/O (fprintf, stderr) and exit codes (EXIT_FAILURE/SUCCESS) */
#include <stdio.h>
#include <stdlib.h>

/* Our own game state and loop declarations */
#include "game.h"

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;   /* unused; required by SDL2 on Windows */
    /*
     * SDL_Init — start the SDL core.
     * Flags tell SDL which subsystems to activate:
     *   SDL_INIT_VIDEO  → creates the event queue, window, and renderer support.
     *   SDL_INIT_AUDIO  → sets up the platform audio device.
     *
     * SDL_INIT_GAMECONTROLLER is intentionally omitted here.
     * It is initialised lazily inside game_init via SDL_InitSubSystem, after
     * the window is already visible.  Deferring the gamepad subsystem avoids
     * triggering antivirus heuristics that flag programs enumerating HID /
     * XInput devices during the very first moments of process startup.
     * Returns 0 on success, negative on failure.
     */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    /*
     * IMG_Init — initialise SDL2_image for PNG support.
     * The return value is a bitmask of the formats that were actually loaded.
     * We mask it with IMG_INIT_PNG to check that PNG support is available.
     */
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init error: %s\n", IMG_GetError());
        SDL_Quit();   /* undo the SDL_Init that already succeeded */
        return EXIT_FAILURE;
    }

    /*
     * TTF_Init — initialise SDL2_ttf (FreeType under the hood).
     * Not used in the MVP yet, but the foundation is here for on-screen text.
     */
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init error: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    /*
     * Mix_OpenAudio — open the audio device and configure it:
     *   44100  → sample rate in Hz (CD quality)
     *   MIX_DEFAULT_FORMAT → 16-bit signed samples (platform default)
     *   2      → stereo (2 channels)
     *   2048   → audio buffer size in samples (controls latency vs. stability)
     */
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "Mix_OpenAudio error: %s\n", Mix_GetError());
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    /*
     * Create our game state on the stack, zero-initialised.
     * The {0} syntax in C sets every field to 0/NULL, so no garbage values.
     * We pass it by pointer (&gs) so functions can modify it in place.
     */
    /*
     * Seed the C standard library RNG so vine layout (and any other
     * rand()-driven decoration) differs each run.  SDL_GetTicks() returns
     * milliseconds since SDL_Init, which varies by OS scheduling jitter
     * and gives a different seed on every launch.
     */
    srand((unsigned int)SDL_GetTicks());

    GameState gs = {0};
    game_init(&gs);     /* create window, renderer, load textures           */
    game_loop(&gs);     /* run until the player quits                       */
    game_cleanup(&gs);  /* free all game-level resources                    */

    /* Tear down SDL subsystems in reverse init order */
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return EXIT_SUCCESS;
}
