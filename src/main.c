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
#include "core/app_session.h"

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
    const char *profile_path = NULL;
    int no_save = 0, continue_last = 0, expect_profile = 0;
    int expect_level_path = 0;
    int expect_replay_script = 0;
    int expect_smoke_frames = 0;
    int expect_seed = 0;

    for (int i = 1; i < argc; i++) {
        if (expect_profile) {
            if (!argv[i][0] || argv[i][0] == '-') { fprintf(stderr,"Error: --profile requires a path\n"); return EXIT_FAILURE; }
            profile_path = argv[i]; expect_profile = 0;
        } else if (expect_level_path) {
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
        else if (strcmp(argv[i], "--no-save") == 0) no_save = 1;
        else if (strcmp(argv[i], "--continue") == 0) continue_last = 1;
        else if (strcmp(argv[i], "--profile") == 0) expect_profile = 1;
        else if (strcmp(argv[i], "--help") == 0) {
            puts("Super Mango: --level PATH | --continue | --sandbox\n"
                 "  --profile PATH   explicit native player profile\n"
                 "  --no-save        memory-only settings/results\n"
                 "  --debug --seed N --smoke-test-frames N --replay-script NAME\n"
                 "F1: settings (menu: gamepad Y; gameplay: Back).\n"
                 "Continue opens the last stage, not a mid-level save.\n"
                 "Profiles: SDL preference directory on native; localStorage on web.\n"
                 "Smoke/replay never reads or writes personal profiles.");
            return EXIT_SUCCESS;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    if (expect_level_path) {
        fprintf(stderr, "Error: --level requires a path\n");
        return EXIT_FAILURE;
    }
    if (expect_profile) { fprintf(stderr,"Error: --profile requires a path\n"); return EXIT_FAILURE; }
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
     * SDL_INIT_GAMECONTROLLER is owned by AppSession.  This keeps controller
     * support alive while the menu and game screens replace each other, while
     * avoiding screen-level init/quit pairs.
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

    {
        AppSessionConfig config = {
            .level_path = level_path,
            .debug_mode = debug_mode,
            .smoke_test_frames = smoke_test_frames,
            .replay_script_path = replay_script_path,
            .profile_enabled = !no_save,
            .profile_path = profile_path,
            .continue_last = continue_last
        };
        AppSession *session = session_create(&config);
        int result;

        if (!session) {
            Mix_HaltChannel(-1);
            Mix_CloseAudio();
            TTF_Quit();
            IMG_Quit();
            SDL_Quit();
            return EXIT_FAILURE;
        }

        result = session_run(session);
#ifndef __EMSCRIPTEN__
        session_destroy(&session);
#endif
        return result;
    }
}
