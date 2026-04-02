/*
 * main.c — Entry point for Super Mango.
 *
 * Responsibilities:
 *   1. Boot every SDL subsystem the game needs.
 *   2. Route to the appropriate screen based on CLI arguments:
 *        default           → start_menu (black screen with title)
 *        --sandbox         → game sandbox (the full gameplay phase)
 *        --sandbox --debug → game sandbox with debug overlays
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
#include <string.h>    /* strcmp — used to match CLI flags */

/* Our own modules */
#include "game.h"
#include "screens/start_menu.h"

int main(int argc, char *argv[]) {
    /*
     * Scan command-line arguments for flags:
     *   --debug   → enable debug overlays in the sandbox
     *   --sandbox → run the gameplay sandbox instead of the start menu
     */
    int debug_mode  = 0;
    int sandbox_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0)   debug_mode   = 1;
        if (strcmp(argv[i], "--sandbox") == 0) sandbox_mode = 1;
    }
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
     * Seed the C standard library RNG so vine layout (and any other
     * rand()-driven decoration) differs each run.  SDL_GetTicks() returns
     * milliseconds since SDL_Init, which varies by OS scheduling jitter
     * and gives a different seed on every launch.
     */
    srand((unsigned int)SDL_GetTicks());

    if (sandbox_mode) {
        /*
         * Sandbox mode — run the full gameplay phase.
         * This is the original game loop, now isolated behind a flag.
         */
        GameState gs = {0};
        gs.debug_mode = debug_mode;
        game_init(&gs);
        game_loop(&gs);
        game_cleanup(&gs);
    } else {
        /*
         * Start Menu — the default entry point.
         * Creates its own window+renderer, renders a black screen
         * with "Start Menu" text and the game logo.
         */

        /*
         * Nearest-neighbour pixel scaling for crisp pixel art.
         */
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

        SDL_Window *window = SDL_CreateWindow(
            "Super Mango",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            800, 600,
            SDL_WINDOW_SHOWN
        );
        if (!window) {
            fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
            Mix_CloseAudio();
            TTF_Quit();
            IMG_Quit();
            SDL_Quit();
            return EXIT_FAILURE;
        }

        SDL_Renderer *renderer = SDL_CreateRenderer(
            window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );
        if (!renderer) {
            fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            Mix_CloseAudio();
            TTF_Quit();
            IMG_Quit();
            SDL_Quit();
            return EXIT_FAILURE;
        }

        SDL_RenderSetLogicalSize(renderer, 400, 300);

        StartMenu menu = {0};
        start_menu_init(&menu, window, renderer);
        start_menu_loop(&menu);
        start_menu_cleanup(&menu);

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }

    /* Tear down SDL subsystems in reverse init order */
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return EXIT_SUCCESS;
}
