/*
 * main.c — Entry point for Super Mango.
 *
 * Responsibilities:
 *   1. Boot every SDL subsystem the game needs.
 *   2. Route to the appropriate screen based on CLI arguments:
 *        default                → start_menu (title screen with Play button)
 *        --level <path>         → load a TOML level and start gameplay directly
 *        --level <path> --debug → same, with debug overlays
 *        --smoke-test-frames N  → run N frames and exit 0
 *        --seed N               → seed rand() for deterministic smoke/replay
 *        --replay-script <name>  → inject out/replays-smoke/<name>.replay
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
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>    /* strcmp — used to match CLI flags */

/* Our own modules */
#include "game.h"
#include "screens/start_menu.h"

static int run_game(const char *level_path,
                    int debug_mode,
                    int smoke_test_frames,
                    const char *replay_script_path) {
    GameState *gs = calloc(1, sizeof(*gs));
    if (!gs) {
        fprintf(stderr, "Error: unable to allocate game state\n");
        return EXIT_FAILURE;
    }

    gs->debug_mode = debug_mode;
    gs->smoke_test_frames = smoke_test_frames;
    if (replay_script_path) {
        strncpy(gs->replay_script_path, replay_script_path,
                sizeof(gs->replay_script_path) - 1);
    }
    strncpy(gs->level_path, level_path, sizeof(gs->level_path) - 1);
    gs->level_path[sizeof(gs->level_path) - 1] = '\0';

    game_init(gs);
    game_loop(gs);
    game_cleanup(gs);
    free(gs);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    /*
     * Scan command-line arguments for flags:
     *   --debug        → enable debug overlays
     *   --level <path> → load a TOML level file (also skips the start menu)
     *   --sandbox      → load the sandbox level (alias for --level levels/00_sandbox_01.toml)
     *   --smoke-test-frames N → deterministic CI smoke exit after N frames
     *   --seed N              → seed rand() for deterministic smoke/replay
     */
    int debug_mode  = 0;
    int smoke_test_frames = 0;
    unsigned int rng_seed = 0;
    int rng_seed_set = 0;
    const char *level_path = NULL;
    const char *replay_script_path = NULL;
    int expect_level_path = 0;
    int expect_replay_script = 0;
    int expect_smoke_frames = 0;
    int expect_seed = 0;

    for (int i = 1; i < argc; i++) {
        if (expect_level_path) {
            if (argv[i][0] == '-') {
                fprintf(stderr, "Error: --level requires a path\n");
                return EXIT_FAILURE;
            }
            level_path = argv[i];
            expect_level_path = 0;
        } else if (expect_replay_script) {
            if (argv[i][0] == '-') {
                fprintf(stderr, "Error: --replay-script requires a path\n");
                return EXIT_FAILURE;
            }
            replay_script_path = argv[i];
            expect_replay_script = 0;
        } else if (expect_smoke_frames) {
            char *end = NULL;
            long parsed = 0;

            errno = 0;
            parsed = strtol(argv[i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0' ||
                parsed <= 0 || parsed > INT_MAX) {
                fprintf(stderr,
                        "Error: --smoke-test-frames requires a positive integer\n");
                return EXIT_FAILURE;
            }
            smoke_test_frames = (int)parsed;
            expect_smoke_frames = 0;
        } else if (expect_seed) {
            char *end = NULL;
            unsigned long parsed = 0;

            if (argv[i][0] == '-') {
                fprintf(stderr, "Error: --seed requires an unsigned integer\n");
                return EXIT_FAILURE;
            }

            errno = 0;
            parsed = strtoul(argv[i], &end, 10);
            if (errno != 0 || end == argv[i] || *end != '\0' ||
                parsed > (unsigned long)UINT_MAX) {
                fprintf(stderr, "Error: --seed requires an unsigned integer\n");
                return EXIT_FAILURE;
            }
            rng_seed = (unsigned int)parsed;
            rng_seed_set = 1;
            expect_seed = 0;
        } else if (strcmp(argv[i], "--debug") == 0)
            debug_mode = 1;
        else if (strcmp(argv[i], "--sandbox") == 0)
            level_path = "levels/00_sandbox_01.toml";
        else if (strcmp(argv[i], "--level") == 0)
            expect_level_path = 1;
        else if (strcmp(argv[i], "--replay-script") == 0)
            expect_replay_script = 1;
        else if (strcmp(argv[i], "--smoke-test-frames") == 0)
            expect_smoke_frames = 1;
        else if (strcmp(argv[i], "--seed") == 0)
            expect_seed = 1;
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (expect_level_path) {
        fprintf(stderr, "Error: --level requires a path\n");
        return EXIT_FAILURE;
    }
    if (expect_replay_script) {
        fprintf(stderr, "Error: --replay-script requires a path\n");
        return EXIT_FAILURE;
    }
    if (expect_smoke_frames) {
        fprintf(stderr, "Error: --smoke-test-frames requires a positive integer\n");
        return EXIT_FAILURE;
    }
    if (expect_seed) {
        fprintf(stderr, "Error: --seed requires an unsigned integer\n");
        return EXIT_FAILURE;
    }

    if (smoke_test_frames > 0 && !level_path) {
        level_path = "levels/00_sandbox_01.toml";
    }
    /*
     * WebAssembly: attach SDL2's keyboard listeners to the canvas element
     * instead of the document.  Without this hint, SDL2 registers keydown/keyup
     * on the document and can miss the matching keyup when the canvas loses
     * focus (e.g. user opens DevTools).  The missing keyup leaves the key
     * "stuck" as pressed in SDL's state table, causing the player to walk or
     * jump without input.  By binding to "#canvas", events only arrive while
     * the canvas has focus and SDL naturally clears keys on blur.
     *
     * Must be called before SDL_Init so the hint is in place when SDL2
     * registers its JavaScript event listeners during video initialisation.
     */
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif

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
     * Seed the C standard library RNG. Normal runs use SDL_GetTicks() so
     * rand()-driven decoration differs each launch; tests can pass --seed
     * for deterministic smoke/replay behavior.
     */
    srand(rng_seed_set ? rng_seed : (unsigned int)SDL_GetTicks());

    if (level_path) {
        /*
         * Direct play — --level <path> skips the start menu.
         * Used by the editor's Play button and make run-level.
         */
        if (run_game(level_path, debug_mode, smoke_test_frames,
                     replay_script_path) != EXIT_SUCCESS) {
            Mix_CloseAudio();
            TTF_Quit();
            IMG_Quit();
            SDL_Quit();
            return EXIT_FAILURE;
        }
    } else {
        /*
         * Start Menu → Game flow.
         *
         * The start menu creates its own window+renderer at 800×600 with
         * a 400×300 logical canvas (matching the game's resolution).
         *
         * When the user clicks "Play" or presses Enter/Space, the menu
         * sets result = MENU_PLAY.  The menu's window and renderer are
         * then destroyed, and a fresh GameState is created for the game
         * loop.  This clean separation avoids resource leaks and ensures
         * the game gets a pristine renderer with all its own settings.
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
        MenuResult result = menu.result;
        char selected_level_path[sizeof(menu.selected_level_path)] = {0};
        strncpy(selected_level_path, menu.selected_level_path,
                sizeof(selected_level_path) - 1);
        selected_level_path[sizeof(selected_level_path) - 1] = '\0';
        start_menu_cleanup(&menu);

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        /*
         * If the user clicked Play, launch the full game.
         *
         * game_init creates its own window and renderer, so we destroy
         * the menu's first to avoid having two windows open at once.
         * Start menu owns selected_level_path so players can choose a level
         * before launching the full game window.
         */
        if (result == MENU_PLAY) {
            if (run_game(selected_level_path, debug_mode, smoke_test_frames,
                         replay_script_path) != EXIT_SUCCESS) {
                Mix_CloseAudio();
                TTF_Quit();
                IMG_Quit();
                SDL_Quit();
                return EXIT_FAILURE;
            }
        }
    }

    /* Tear down SDL subsystems in reverse init order */
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return EXIT_SUCCESS;
}
