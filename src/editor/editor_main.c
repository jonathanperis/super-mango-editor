/*
 * editor_main.c — Entry point for the Super Mango level editor.
 *
 * Responsibilities:
 *   1. Boot the SDL subsystems the editor needs (video, image, TTF).
 *   2. Initialise the EditorState via editor_init().
 *   3. Optionally load a level file passed as a command-line argument.
 *   4. Run the editor's main loop until the user quits.
 *   5. Tear every subsystem back down before exiting.
 *
 * The editor does NOT use SDL_mixer — it has no audio playback.  Only the
 * game executable needs sound; the editor is a silent visual tool.
 *
 * Init and teardown follow the same reverse-order convention as main.c:
 *   SDL → IMG → TTF  (init order, earliest to latest)
 *   TTF → IMG → SDL  (teardown order, latest to earliest)
 * This ensures later subsystems are shut down before the foundational
 * ones they depend on.
 */

#include <SDL.h>       /* SDL_Init, SDL_Quit — core event loop and window */
#include <SDL_image.h> /* IMG_Init, IMG_Quit — PNG texture loading        */
#include <SDL_ttf.h>   /* TTF_Init, TTF_Quit — TrueType font rendering   */
#include <stdio.h>     /* fprintf, stderr — error reporting               */
#include <stdlib.h>    /* EXIT_FAILURE, EXIT_SUCCESS — process exit codes */
#include <string.h>    /* strncpy — safe string copy for file path        */

#include "editor.h"       /* EditorState, editor_init/loop/cleanup */
#include "serializer.h"   /* level_load_toml — load a .toml level file     */

/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    /*
     * SDL_Init — start the SDL core.
     *
     * SDL_INIT_VIDEO activates the event queue, window management, and
     * renderer support.  Unlike the game's main.c we do NOT pass
     * SDL_INIT_AUDIO because the editor has no sound playback.
     *
     * Returns 0 on success, negative on failure.
     */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    /*
     * IMG_Init — initialise SDL2_image for PNG support.
     *
     * The return value is a bitmask of the formats that were actually
     * loaded.  We mask with IMG_INIT_PNG to confirm PNG decoding works.
     * All entity sprite sheets are stored as .png files in assets/.
     */
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init error: %s\n", IMG_GetError());
        SDL_Quit();   /* undo the SDL_Init that already succeeded */
        return EXIT_FAILURE;
    }

    /*
     * TTF_Init — initialise SDL2_ttf (FreeType under the hood).
     *
     * The editor uses TTF_OpenFont to load assets/round9x13.ttf for
     * rendering toolbar labels, status bar text, and property fields.
     * Returns 0 on success, negative on failure.
     */
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init error: %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    /*
     * Create the EditorState on the stack, zero-initialised.
     *
     * The {0} initialiser sets every byte to zero: all pointers become NULL,
     * all integers become 0, and all floats become 0.0f.  This is the same
     * pattern the game uses for GameState in main.c.
     */
    EditorState es = {0};

    /*
     * editor_init — create the window, renderer, font, textures, undo stack.
     *
     * Returns 0 on success, non-zero on fatal error.  If init fails the
     * function prints its own error message; we just need to clean up
     * the SDL subsystems we already started and exit.
     */
    if (editor_init(&es) != 0) {
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    /*
     * Optional level load from command line:
     *   ./editor levels/sandbox_00.toml
     *
     * argv[0] is the executable name; argv[1] (if present) is treated as
     * a path to a TOML level file.  If the file fails to load we warn but
     * continue — the designer can start a fresh level instead.
     *
     * strncpy with sizeof-1 ensures the path is always NUL-terminated
     * even if the argument is longer than the buffer (truncation is safe
     * here — the save dialog will use the stored path as-is).
     */
    if (argc > 1) {
        if (level_load_toml(argv[1], &es.level) == 0) {
            strncpy(es.file_path, argv[1], sizeof(es.file_path) - 1);
            es.file_path[sizeof(es.file_path) - 1] = '\0';
            es.modified = 0;
        } else {
            fprintf(stderr, "Warning: could not load '%s' — starting empty\n",
                    argv[1]);
        }
    }

    /*
     * editor_loop — run the event/render loop until the user quits.
     *
     * This blocks until es.running is set to 0 (window close or Escape).
     */
    editor_loop(&es);

    /*
     * editor_cleanup — free all editor resources in reverse init order.
     *
     * Textures, font, undo stack, renderer, and window are all released.
     */
    editor_cleanup(&es);

    /*
     * Tear down SDL subsystems in reverse init order:
     *   TTF (last started) → IMG → SDL (first started).
     *
     * This matches the game's teardown pattern and ensures no subsystem
     * outlives the one it depends on.
     */
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return EXIT_SUCCESS;
}
